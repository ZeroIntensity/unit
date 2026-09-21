#include <string.h>

#include <unit/internal/base.h>
#include <unit/internal/errors.h>
#include <unit/internal/utils.h>

#include <unit/internal/compilation/code_buffer.h>
#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/compilation/executable_formats.h>

#include <unit/internal/collections/vector.h>

#include "elf_local.h"

#define WRITE_INT(name, value)                                              \
        if (UNIT_FAILED(_UNIT_File_Write ## name (context, file, value))) { \
            return _UNIT_FAIL;                                              \
        }

#define WRITE_U8(value) WRITE_INT(U8, value)
#define WRITE_U16(value) WRITE_INT(U16, value)
#define WRITE_U32(value) WRITE_INT(U32, value)
#define WRITE_U64(value) WRITE_INT(U64, value)
#define WRITE_I64(value) WRITE_INT(I64, value)

typedef struct {
    uint64_t offset;
    uint32_t symbol_table_index;
    uint32_t type;
    int64_t addend;
} ELF_Relocation;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t alignment;
    uint64_t entry_size;
    const _UNIT_CodeBuffer *data;
    _UNIT_Vector relocations;
} ELF_Section;

static void
ELF_Section_Dealloc(UNIT_Context *context, void *ptr)
{
    assert(context != NULL);
    assert(ptr != NULL);
    ELF_Section *section = (ELF_Section *)ptr;
    if (section->type == ELF_SECTION_TYPE_PROGRAM_DATA) {
        _UNIT_Vector_Clear(&section->relocations);
    }

    _UNIT_Dealloc(context, section);
}

typedef struct {
    const char *name; // Owned by the string table
    uint32_t offset_in_string_table;
    uint64_t section_offset;
    uint16_t section_number;
    uint8_t binding;
    uint8_t type;
} ELF_Symbol;

typedef struct {
    // The null section and symbol are written separately, so indices start at 1.
    _UNIT_Vector sections; // ELF_Section*
    _UNIT_Vector strings;
    _UNIT_Vector section_strings;
    _UNIT_Vector symbols;
    uint32_t first_global_symbol;
    uint16_t section_string_table_index;
} ELF_Object;

static UNIT_Status
ELF_Object_Init(ELF_Object *elf_object, UNIT_Context *context)
{
    assert(elf_object != NULL);
    assert(context != NULL);

    if (UNIT_FAILED(_UNIT_Vector_Init(&elf_object->sections,
                                      context,
                                      6,
                                      ELF_Section_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&elf_object->strings,
                                      context,
                                      8,
                                      _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&elf_object->sections);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&elf_object->section_strings,
                                      context,
                                      6,
                                      _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&elf_object->strings);
        _UNIT_Vector_Clear(&elf_object->sections);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&elf_object->symbols,
                                      context,
                                      8,
                                      _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&elf_object->section_strings);
        _UNIT_Vector_Clear(&elf_object->strings);
        _UNIT_Vector_Clear(&elf_object->sections);
        return _UNIT_FAIL;
    }

    elf_object->first_global_symbol = 1;
    elf_object->section_string_table_index = 0;
    return _UNIT_OK;
}

static void
ELF_Object_Clear(ELF_Object *elf_object)
{
    assert(elf_object != NULL);
    _UNIT_Vector_Clear(&elf_object->sections);
    _UNIT_Vector_Clear(&elf_object->strings);
    _UNIT_Vector_Clear(&elf_object->section_strings);
    _UNIT_Vector_Clear(&elf_object->symbols);
}

static UNIT_Size
add_string(_UNIT_Vector *strings, const char *string)
{
    assert(strings != NULL);
    assert(string != NULL);

    char *owned = _UNIT_StrDup(strings->context, string);
    if (owned == NULL) {
        return -1;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(strings);
    if (UNIT_FAILED(_UNIT_Vector_Append(strings, owned))) {
        return -1;
    }

    return index;
}

static uint32_t
get_string_table_offset(const _UNIT_Vector *strings, UNIT_Size target_index)
{
    assert(strings != NULL);
    assert(target_index >= 0);
    assert(target_index <= _UNIT_Vector_SIZE(strings));
    uint32_t offset = 1; // String tables start with a null byte.

    for (UNIT_Size index = 0; index < target_index; ++index) {
        const char *string = _UNIT_Vector_GET(strings, index);
        assert(string != NULL);
        offset += strlen(string) + 1;
    }

    return offset;
}

static ELF_Section *
add_section(ELF_Object *elf_object,
            const char *name,
            uint32_t type,
            uint64_t flags,
            uint64_t alignment)
{
    assert(elf_object != NULL);
    assert(name != NULL);
    assert(alignment > 0);

    UNIT_Context *context = elf_object->sections.context;
    ELF_Section *section = _UNIT_Calloc(context, 1, sizeof(ELF_Section));
    if (section == NULL) {
        return NULL;
    }

    section->type = type;
    section->flags = flags;
    section->alignment = alignment;

    if (type == ELF_SECTION_TYPE_PROGRAM_DATA &&
        UNIT_FAILED(_UNIT_Vector_Init(&section->relocations, context, 4, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, section);
        return NULL;
    }

    UNIT_Size name_index = add_string(&elf_object->section_strings, name);
    if (name_index == -1) {
        ELF_Section_Dealloc(context, section);
        return NULL;
    }

    section->name = get_string_table_offset(&elf_object->section_strings, name_index);
    if (UNIT_FAILED(_UNIT_Vector_Append(&elf_object->sections, section))) {
        return NULL;
    }

    return section;
}

static UNIT_Size
find_symbol(ELF_Object *elf_object, const char *name, uint8_t binding)
{
    assert(elf_object != NULL);
    assert(name != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&elf_object->symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Symbol *symbol = _UNIT_Vector_GET(&elf_object->symbols, index);
        assert(symbol != NULL);
        if (symbol->binding == binding && !strcmp(symbol->name, name)) {
            return index + 1;
        }
    }

    return -1;
}

static UNIT_Size
add_symbol(ELF_Object *elf_object,
           const char *name,
           uint64_t section_offset,
           uint16_t section_number,
           uint8_t binding,
           uint8_t type)
{
    assert(elf_object != NULL);
    assert(name != NULL);

    UNIT_Context *context = elf_object->symbols.context;
    ELF_Symbol *symbol = _UNIT_Alloc(context, sizeof(ELF_Symbol));
    if (symbol == NULL) {
        return -1;
    }

    UNIT_Size name_index = add_string(&elf_object->strings, name);
    if (name_index == -1) {
        _UNIT_Dealloc(context, symbol);
        return -1;
    }

    symbol->name = _UNIT_Vector_GET(&elf_object->strings, name_index);
    symbol->offset_in_string_table = get_string_table_offset(&elf_object->strings, name_index);
    symbol->section_offset = section_offset;
    symbol->section_number = section_number;
    symbol->binding = binding;
    symbol->type = type;

    UNIT_Size symbol_index = _UNIT_Vector_SIZE(&elf_object->symbols) + 1;
    if (UNIT_FAILED(_UNIT_Vector_Append(&elf_object->symbols, symbol))) {
        return -1;
    }

    return symbol_index;
}

static UNIT_Size
find_or_add_symbol(ELF_Object *elf_object,
                   const char *name,
                   uint64_t section_offset,
                   uint16_t section_number,
                   uint8_t binding,
                   uint8_t type)
{
    assert(elf_object != NULL);
    assert(name != NULL);

    UNIT_Size found_index = find_symbol(elf_object, name, binding);
    if (found_index != -1) {
        return found_index;
    }

    return add_symbol(elf_object, name, section_offset, section_number, binding, type);
}

static UNIT_Status
build_defined_symbols(ELF_Object *elf_object, const _UNIT_CompileContext *compile_context)
{
    assert(elf_object != NULL);
    assert(compile_context != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&compile_context->symbol_table.symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Symbol *symbol = _UNIT_Vector_GET(&compile_context->symbol_table.symbols, index);
        assert(symbol != NULL);
        if (!symbol->is_defined) {
            continue;
        }

        if (add_symbol(elf_object,
                       symbol->name,
                       symbol->text_offset,
                       _UNIT_Vector_SIZE(&elf_object->sections),
                       ELF_SYMBOL_BINDING_GLOBAL,
                       ELF_SYMBOL_TYPE_FUNCTION) == -1) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
build_relocations(ELF_Object *elf_object,
                  ELF_Section *text_section,
                  const _UNIT_CompileContext *compile_context)
{
    assert(elf_object != NULL);
    assert(text_section != NULL);
    assert(compile_context != NULL);

    UNIT_Context *context = compile_context->context;
    assert(context != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&compile_context->symbol_table.relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Relocation *relocation = _UNIT_Vector_GET(&compile_context->symbol_table.relocations,
                                                        index);
        assert(relocation != NULL);
        ELF_Relocation *elf_relocation = _UNIT_Alloc(context, sizeof(ELF_Relocation));
        if (elf_relocation == NULL) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(&text_section->relocations, elf_relocation))) {
            return _UNIT_FAIL;
        }

        elf_relocation->offset = relocation->offset;
        if (relocation->type == _UNIT_RELOCATION_CALL) {
            _UNIT_Symbol *symbol = _UNIT_Vector_GET(&compile_context->symbol_table.symbols,
                                                    relocation->symbol_index);
            assert(symbol != NULL);
            uint16_t section_number = symbol->is_defined ?
                                      _UNIT_Vector_SIZE(&elf_object->sections) :
                                      ELF_SECTION_UNDEFINED;
            UNIT_Size symbol_table_index = find_or_add_symbol(elf_object,
                                                              symbol->name,
                                                              symbol->text_offset,
                                                              section_number,
                                                              ELF_SYMBOL_BINDING_GLOBAL,
                                                              symbol->is_defined ?
                                                              ELF_SYMBOL_TYPE_FUNCTION :
                                                              ELF_SYMBOL_TYPE_NONE);
            if (symbol_table_index == -1) {
                return _UNIT_FAIL;
            }

            elf_relocation->symbol_table_index = symbol_table_index;
            elf_relocation->type = ELF_RELOCATION_AMD64_PLT32;
            elf_relocation->addend = -4;
        } else {
            assert(relocation->type == _UNIT_RELOCATION_DATA);
            UNIT_Size rodata_symbol_index = find_symbol(elf_object,
                                                        ".rodata",
                                                        ELF_SYMBOL_BINDING_LOCAL);
            assert(rodata_symbol_index != -1);
            elf_relocation->symbol_table_index = rodata_symbol_index;
            elf_relocation->type = ELF_RELOCATION_AMD64_PC32;
            elf_relocation->addend = relocation->symbol_index - 4;
        }
    }

    if (size == 0) {
        return _UNIT_OK;
    }

    uint32_t text_section_index = _UNIT_Vector_SIZE(&elf_object->sections);
    ELF_Section *relocation_section = add_section(elf_object,
                                                  ".rela.text",
                                                  ELF_SECTION_TYPE_RELA,
                                                  ELF_SECTION_FLAG_INFO_LINK,
                                                  8);
    if (relocation_section == NULL) {
        return _UNIT_FAIL;
    }

    relocation_section->size = size * ELF_RELOCATION_SIZE;
    relocation_section->info = text_section_index;
    relocation_section->entry_size = ELF_RELOCATION_SIZE;
    return _UNIT_OK;
}

static UNIT_Status
build_text_section(ELF_Object *elf_object, const _UNIT_CompileContext *compile_context)
{
    assert(elf_object != NULL);
    assert(compile_context != NULL);

    ELF_Section *text_section = add_section(elf_object,
                                            ".text",
                                            ELF_SECTION_TYPE_PROGRAM_DATA,
                                            ELF_SECTION_FLAG_ALLOC | ELF_SECTION_FLAG_EXECUTABLE,
                                            16);
    if (text_section == NULL) {
        return _UNIT_FAIL;
    }

    text_section->data = &compile_context->buffer;
    text_section->size = compile_context->buffer.size;

    if (UNIT_FAILED(build_defined_symbols(elf_object, compile_context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(build_relocations(elf_object, text_section, compile_context))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static UNIT_Status
build_rodata_section(ELF_Object *elf_object, const _UNIT_CompileContext *compile_context)
{
    assert(elf_object != NULL);
    assert(compile_context != NULL);

    if (compile_context->string_data.constant_buffer.size == 0) {
        return _UNIT_OK;
    }

    ELF_Section *data_section = add_section(elf_object,
                                            ".rodata",
                                            ELF_SECTION_TYPE_PROGRAM_DATA,
                                            ELF_SECTION_FLAG_ALLOC,
                                            1);
    if (data_section == NULL) {
        return _UNIT_FAIL;
    }

    data_section->data = &compile_context->string_data.constant_buffer;
    data_section->size = compile_context->string_data.constant_buffer.size;

    if (add_symbol(elf_object,
                   ".rodata",
                   0,
                   _UNIT_Vector_SIZE(&elf_object->sections),
                   ELF_SYMBOL_BINDING_LOCAL,
                   ELF_SYMBOL_TYPE_SECTION) == -1) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static UNIT_Status
build_table_sections(ELF_Object *elf_object)
{
    assert(elf_object != NULL);

    ELF_Section *symbol_table = add_section(elf_object,
                                            ".symtab",
                                            ELF_SECTION_TYPE_SYMBOL_TABLE,
                                            0,
                                            8);
    if (symbol_table == NULL) {
        return _UNIT_FAIL;
    }

    uint32_t symbol_table_index = _UNIT_Vector_SIZE(&elf_object->sections);
    symbol_table->size = (_UNIT_Vector_SIZE(&elf_object->symbols) + 1) * ELF_SYMBOL_SIZE;
    symbol_table->info = elf_object->first_global_symbol;
    symbol_table->entry_size = ELF_SYMBOL_SIZE;

    ELF_Section *string_table = add_section(elf_object,
                                            ".strtab",
                                            ELF_SECTION_TYPE_STRING_TABLE,
                                            0,
                                            1);
    if (string_table == NULL) {
        return _UNIT_FAIL;
    }

    symbol_table->link = _UNIT_Vector_SIZE(&elf_object->sections);
    string_table->size = get_string_table_offset(&elf_object->strings,
                                                 _UNIT_Vector_SIZE(&elf_object->strings));

    ELF_Section *section_string_table = add_section(elf_object,
                                                    ".shstrtab",
                                                    ELF_SECTION_TYPE_STRING_TABLE,
                                                    0,
                                                    1);
    if (section_string_table == NULL) {
        return _UNIT_FAIL;
    }

    section_string_table->size = get_string_table_offset(&elf_object->section_strings,
                                                         _UNIT_Vector_SIZE(
                                                             &elf_object->section_strings));
    elf_object->section_string_table_index = _UNIT_Vector_SIZE(&elf_object->sections);

    UNIT_Size size = _UNIT_Vector_SIZE(&elf_object->sections);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Section *section = _UNIT_Vector_GET(&elf_object->sections, index);
        if (section->type == ELF_SECTION_TYPE_RELA) {
            section->link = symbol_table_index;
        }
    }

    return _UNIT_OK;
}

static void
build_section_offsets(ELF_Object *elf_object)
{
    assert(elf_object != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&elf_object->sections);
    uint64_t offset = ELF_FILE_HEADER_SIZE + ((size + 1) * ELF_SECTION_HEADER_SIZE);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Section *section = _UNIT_Vector_GET(&elf_object->sections, index);
        assert(section != NULL);
        offset = (offset + section->alignment - 1) & ~(section->alignment - 1);
        section->offset = offset;
        offset += section->size;
    }
}

static UNIT_Status
build_elf_object(ELF_Object *elf_object, const _UNIT_CompileContext *compile_context)
{
    assert(elf_object != NULL);
    assert(compile_context != NULL);

    if (UNIT_FAILED(ELF_Object_Init(elf_object, compile_context->context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(build_rodata_section(elf_object, compile_context))) {
        goto error;
    }

    // ELF requires all local symbols to precede the global symbols.
    elf_object->first_global_symbol = _UNIT_Vector_SIZE(&elf_object->symbols) + 1;
    if (UNIT_FAILED(build_text_section(elf_object, compile_context))) {
        goto error;
    }

    if (UNIT_FAILED(build_table_sections(elf_object))) {
        goto error;
    }

    build_section_offsets(elf_object);
    return _UNIT_OK;

error:
    ELF_Object_Clear(elf_object);
    return _UNIT_FAIL;
}

static UNIT_Status
write_padding(UNIT_Context *context, FILE *file, uint64_t size)
{
    assert(context != NULL);
    assert(file != NULL);

    const uint8_t zeros[ELF_SECTION_HEADER_SIZE] = {0};
    while (size > 0) {
        size_t count = size < sizeof(zeros) ? size : sizeof(zeros);
        if (UNIT_FAILED(_UNIT_File_WriteBytes(context, file, zeros, count))) {
            return _UNIT_FAIL;
        }

        size -= count;
    }

    return _UNIT_OK;
}

static UNIT_Status
write_elf_header(ELF_Object *elf_object, UNIT_Context *context, FILE *file)
{
    assert(elf_object != NULL);
    assert(context != NULL);
    assert(file != NULL);

    WRITE_U8(0x7f);
    WRITE_U8('E');
    WRITE_U8('L');
    WRITE_U8('F');
    WRITE_U8(ELF_CLASS_64);
    WRITE_U8(ELF_DATA_LITTLE_ENDIAN);
    WRITE_U8(ELF_VERSION_CURRENT);
    WRITE_U8(0); // System V ABI
    WRITE_U8(0); // ABI version
    if (UNIT_FAILED(write_padding(context, file, 7))) {
        return _UNIT_FAIL;
    }

    WRITE_U16(ELF_TYPE_RELOCATABLE);
    WRITE_U16(ELF_MACHINE_AMD64);
    WRITE_U32(ELF_VERSION_CURRENT);
    WRITE_U64(0); // Entry point
    WRITE_U64(0); // Program header offset
    WRITE_U64(ELF_FILE_HEADER_SIZE);
    WRITE_U32(0); // Flags
    WRITE_U16(ELF_FILE_HEADER_SIZE);
    WRITE_U16(0); // Program header entry size
    WRITE_U16(0); // Number of program headers
    WRITE_U16(ELF_SECTION_HEADER_SIZE);
    WRITE_U16(_UNIT_Vector_SIZE(&elf_object->sections) + 1);
    WRITE_U16(elf_object->section_string_table_index);
    return _UNIT_OK;
}

static UNIT_Status
write_section_header(UNIT_Context *context, ELF_Section *section, FILE *file)
{
    assert(context != NULL);
    assert(section != NULL);
    assert(file != NULL);

    WRITE_U32(section->name);
    WRITE_U32(section->type);
    WRITE_U64(section->flags);
    WRITE_U64(0); // Virtual address
    WRITE_U64(section->offset);
    WRITE_U64(section->size);
    WRITE_U32(section->link);
    WRITE_U32(section->info);
    WRITE_U64(section->alignment);
    WRITE_U64(section->entry_size);
    return _UNIT_OK;
}

static UNIT_Status
write_section_relocations(UNIT_Context *context, ELF_Section *section, FILE *file)
{
    assert(context != NULL);
    assert(section != NULL);
    assert(file != NULL);

    // ELF stores addends in relocation entries, leaving the code buffer untouched.
    UNIT_Size size = _UNIT_Vector_SIZE(&section->relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Relocation *relocation = _UNIT_Vector_GET(&section->relocations, index);
        assert(relocation != NULL);
        WRITE_U64(relocation->offset);
        WRITE_U64(((uint64_t)relocation->symbol_table_index << 32) | relocation->type);
        WRITE_I64(relocation->addend);
    }

    return _UNIT_OK;
}

static UNIT_Status
write_symbol_table(ELF_Object *object, UNIT_Context *context, FILE *file)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(file != NULL);

    if (UNIT_FAILED(write_padding(context, file, ELF_SYMBOL_SIZE))) {
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&object->symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Symbol *symbol = _UNIT_Vector_GET(&object->symbols, index);
        assert(symbol != NULL);
        WRITE_U32(symbol->offset_in_string_table);
        WRITE_U8((symbol->binding << 4) | symbol->type);
        WRITE_U8(0); // Default visibility
        WRITE_U16(symbol->section_number);
        WRITE_U64(symbol->section_offset);
        WRITE_U64(0); // Symbol size is not recorded by the compiler.
    }

    return _UNIT_OK;
}

static UNIT_Status
write_string_table(const _UNIT_Vector *strings, UNIT_Context *context, FILE *file)
{
    assert(strings != NULL);
    assert(context != NULL);
    assert(file != NULL);

    WRITE_U8(0);
    UNIT_Size size = _UNIT_Vector_SIZE(strings);
    for (UNIT_Size index = 0; index < size; ++index) {
        const char *string = _UNIT_Vector_GET(strings, index);
        assert(string != NULL);
        if (UNIT_FAILED(_UNIT_File_WriteBytes(context, file, string, strlen(string) + 1))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
write_elf_sections(ELF_Object *object, UNIT_Context *context, FILE *file)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(file != NULL);

    if (UNIT_FAILED(write_padding(context, file, ELF_SECTION_HEADER_SIZE))) {
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&object->sections);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Section *section = _UNIT_Vector_GET(&object->sections, index);
        if (UNIT_FAILED(write_section_header(context, section, file))) {
            return _UNIT_FAIL;
        }
    }

    uint64_t offset = ELF_FILE_HEADER_SIZE + ((size + 1) * ELF_SECTION_HEADER_SIZE);
    for (UNIT_Size index = 0; index < size; ++index) {
        ELF_Section *section = _UNIT_Vector_GET(&object->sections, index);
        assert(section != NULL);
        assert(section->offset >= offset);
        if (UNIT_FAILED(write_padding(context, file, section->offset - offset))) {
            return _UNIT_FAIL;
        }

        UNIT_Status status;
        switch (section->type) {
            case ELF_SECTION_TYPE_PROGRAM_DATA: {
                assert(section->data != NULL);
                status = _UNIT_File_WriteBytes(context, file, section->data->data, section->size);
                break;
            }
            case ELF_SECTION_TYPE_RELA: {
                ELF_Section *target = _UNIT_Vector_GET(&object->sections, section->info - 1);
                status = write_section_relocations(context, target, file);
                break;
            }
            case ELF_SECTION_TYPE_SYMBOL_TABLE: {
                status = write_symbol_table(object, context, file);
                break;
            }
            case ELF_SECTION_TYPE_STRING_TABLE: {
                const _UNIT_Vector *strings = index + 1 == object->section_string_table_index ?
                                              &object->section_strings : &object->strings;
                status = write_string_table(strings, context, file);
                break;
            }
            default: {
                _UNIT_Unreachable();
            }
        }

        if (UNIT_FAILED(status)) {
            return _UNIT_FAIL;
        }

        offset = section->offset + section->size;
    }

    return _UNIT_OK;
}

static UNIT_Status
write_elf_object_to_file(ELF_Object *object, UNIT_Context *context, const char *path)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(path != NULL);

    FILE *file = fopen(path, "wb");
    if (!file) {
        _UNIT_SetOSError(context, "opening ELF file");
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(write_elf_header(object, context, file))) {
        goto error;
    }

    if (UNIT_FAILED(write_elf_sections(object, context, file))) {
        goto error;
    }

    if (fclose(file) != 0) {
        _UNIT_SetOSError(context, "closing ELF file");
        return _UNIT_FAIL;
    }

    return _UNIT_OK;

error:
    fclose(file);
    return _UNIT_FAIL;
}

UNIT_Status
_UNIT_ELF_WriteObjectFile(const _UNIT_CompileContext *compile_context,
                          const char *path)
{
    assert(compile_context != NULL);
    assert(path != NULL);

    ELF_Object elf_object;
    if (UNIT_FAILED(build_elf_object(&elf_object, compile_context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(write_elf_object_to_file(&elf_object, compile_context->context, path))) {
        ELF_Object_Clear(&elf_object);
        return _UNIT_FAIL;
    }

    ELF_Object_Clear(&elf_object);
    return _UNIT_OK;
}
