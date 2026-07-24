#include <stdlib.h>
#include <string.h>

#include <unit/base.h>
#include <unit/errors.h>

#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/executable_formats.h>
#include <unit/internal/size_map.h>
#include <unit/internal/vector.h>

#include "coff_local.h"

#define WRITE_INT_IMPL(size_bytes)                        \
        assert(context != NULL);                          \
        assert(file != NULL);                             \
        if (fwrite(&value, size_bytes, 1, file) != 1) {   \
            _UNIT_SetOSError(context, "writing integer"); \
            return _UNIT_FAIL;                            \
        }                                                 \
        return _UNIT_OK;

#define WRITE_INT(name, value)                                   \
        if (UNIT_FAILED(write_ ## name(context, file, value))) { \
            return _UNIT_FAIL;                                   \
        }

static inline UNIT_Status
write_u8(UNIT_Context *context, FILE *file, uint8_t value)
{
    WRITE_INT_IMPL(1);
}

#define WRITE_U8(value) WRITE_INT(u8, value)

static inline UNIT_Status
write_u16(UNIT_Context *context, FILE *file, uint16_t value)
{
    WRITE_INT_IMPL(2);
}

#define WRITE_U16(value) WRITE_INT(u16, value)

static inline UNIT_Status
write_u32(UNIT_Context *context, FILE *file, uint32_t value)
{
    WRITE_INT_IMPL(4);
}

#define WRITE_U32(value) WRITE_INT(u32, value)

static inline UNIT_Status
write_i16(UNIT_Context *context, FILE *file, int16_t value)
{
    WRITE_INT_IMPL(2);
}

#define WRITE_I16(value) WRITE_INT(i16, value)

static inline UNIT_Status
write_bytes(UNIT_Context *context, FILE *file, const void *data, size_t num_bytes)
{
    assert(context != NULL);
    assert(file != NULL);
    assert(data != NULL);
    if (num_bytes == 0) {
        return _UNIT_OK;
    }

    if (fwrite(data, 1, num_bytes, file) != num_bytes) {
        _UNIT_SetOSError(context, "writing bytes");
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static inline UNIT_Status
write_name8(UNIT_Context *context, FILE *file, const char *name)
{
    assert(file != NULL);
    assert(name != NULL);
    char buf[8] = {0};
    size_t len = strlen(name);
    if (len > 8) {
        len = 8;
    }

    memcpy(buf, name, len);
    return write_bytes(context, file, buf, 8);
}

#define WRITE_NAME(name)                                     \
        if (UNIT_FAILED(write_name8(context, file, name))) { \
            return _UNIT_FAIL;                               \
        }

typedef struct {
    uint32_t offset;
    uint32_t symbol_table_index;
    uint16_t type;
} COFF_Relocation;

typedef struct {
    char name[8];
    _UNIT_Vector relocations;
    const _UNIT_CodeBuffer *data;
} COFF_Section;

static COFF_Section *
COFF_Section_New(const _UNIT_CodeBuffer *buffer, const char *name)
{
    assert(name != NULL);

    COFF_Section *section = _UNIT_Alloc(buffer->context, sizeof(COFF_Section));
    if (section == NULL) {
        return NULL;
    }

    section->data = buffer;

    assert(strlen(name) < 8);
    strcpy(section->name, name);

    if (UNIT_FAILED(_UNIT_Vector_Init(&section->relocations, buffer->context, 4, _UNIT_Dealloc))) {
        _UNIT_Dealloc(buffer->context, section);
        return NULL;
    }

    return section;
}

static void
COFF_Section_Dealloc(UNIT_Context *context, void *ptr)
{
    assert(ptr != NULL);
    COFF_Section *section = (COFF_Section *)ptr;
    _UNIT_Vector_Clear(&section->relocations);
    _UNIT_Dealloc(context, section);
}

typedef struct {
    union {
        char name[8];
        struct {
            uint32_t _padding; // First 4 bytes must be 0
            uint32_t offset_in_string_table;
        };
    };

    uint32_t section_offset;
    int16_t section_number;
    uint16_t type;
    uint8_t storage_class;
} COFF_Symbol;

typedef struct {
    _UNIT_Vector sections; // COFF_Section*
    _UNIT_Vector strings;
    _UNIT_Vector symbols;
} COFF_Object;

static UNIT_Status
COFF_Object_Init(COFF_Object *coff_object, UNIT_Context *context)
{
    assert(coff_object != NULL);
    assert(context != NULL);

    if (UNIT_FAILED(_UNIT_Vector_Init(&coff_object->sections,
                                      context,
                                      2,
                                      COFF_Section_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&coff_object->strings,
                                      context,
                                      8,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&coff_object->symbols,
                                      context,
                                      8,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static void
COFF_Object_Clear(COFF_Object *coff_object)
{
    assert(coff_object != NULL);
    _UNIT_Vector_Clear(&coff_object->sections);
    _UNIT_Vector_Clear(&coff_object->strings);
    _UNIT_Vector_Clear(&coff_object->symbols);
}

static UNIT_Size
add_string(COFF_Object *coff_object, const char *string)
{
    assert(coff_object != NULL);
    assert(string != NULL);

    char *owned = _UNIT_StrDup(coff_object->strings.context, string);
    if (owned == NULL) {
        return -1;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&coff_object->strings);
    if (UNIT_FAILED(_UNIT_Vector_Append(&coff_object->strings, owned))) {
        return -1;
    }

    return index;
}

static uint32_t
get_string_table_offset(COFF_Object *coff_object,
                        UNIT_Size target_index)
{
    assert(coff_object != NULL);
    assert(target_index >= 0);
    assert(target_index < _UNIT_Vector_SIZE(&coff_object->strings));
    uint32_t offset = 4;

    for (UNIT_Size index = 0; index < target_index; ++index) {
        char *string = _UNIT_Vector_GET(&coff_object->strings, index);
        assert(string != NULL);
        offset += strlen(string) + 1;
    }

    return offset;
}

static UNIT_Size
add_symbol(COFF_Object *coff_object,
           const char *name,
           uint32_t section_offset,
           int16_t section_number,
           int8_t is_defined)
{
    assert(coff_object != NULL);
    assert(name != NULL);

    COFF_Symbol *symbol = _UNIT_Alloc(coff_object->symbols.context, sizeof(COFF_Symbol));
    if (symbol == NULL) {
        return -1;
    }

    UNIT_Size symbol_index = _UNIT_Vector_SIZE(&coff_object->symbols);
    if (UNIT_FAILED(_UNIT_Vector_Append(&coff_object->symbols, symbol))) {
        return -1;
    }

    if (strlen(name) >= 8) {
        UNIT_Size index = add_string(coff_object, name);
        if (index == -1) {
            return -1;
        }

        symbol->_padding = 0;
        symbol->offset_in_string_table = get_string_table_offset(coff_object, index);
    } else {
        strcpy(symbol->name, name);
    }

    symbol->section_offset = is_defined ? section_offset : 0;
    symbol->section_number = section_number;
    symbol->type = 0; // is_defined ? COFF_SYM_TYPE_FUNCTION : COFF_SYM_TYPE_NULL;
    symbol->storage_class = COFF_SYM_CLASS_EXTERNAL;

    return symbol_index;
}

static UNIT_Status
build_text_section(COFF_Object *coff_object, const _UNIT_CompileContext *compile_context)
{
    assert(coff_object != NULL);
    assert(compile_context != NULL);

    UNIT_Context *context = compile_context->context;
    assert(context != NULL);

    COFF_Section *text_section = COFF_Section_New(&compile_context->buffer, ".text");
    if (text_section == NULL) {
        return _UNIT_FAIL;
    }

    _UNIT_Vector_APPEND(&coff_object->sections, text_section);

    UNIT_Size size = _UNIT_Vector_SIZE(&compile_context->symbol_table.relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Relocation *relocation = _UNIT_Vector_GET(&compile_context->symbol_table.relocations,
                                                        index);
        assert(relocation != NULL);
        COFF_Relocation *coff_relocation = _UNIT_Alloc(context, sizeof(COFF_Relocation));
        if (coff_relocation == NULL) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(&text_section->relocations, coff_relocation))) {
            return _UNIT_FAIL;
        }

        assert(relocation->type == _UNIT_RELOCATION_CALL); // TODO
        _UNIT_Symbol *symbol = _UNIT_Vector_GET(&compile_context->symbol_table.symbols,
                                                relocation->symbol_index);
        int16_t section_number =
            symbol->is_defined ? (_UNIT_Vector_SIZE(&coff_object->sections)) : COFF_SYM_UNDEFINED;
        UNIT_Size symbol_table_index = add_symbol(coff_object,
                                                  symbol->name,
                                                  symbol->text_offset,
                                                  section_number,
                                                  symbol->is_defined);
        if (symbol_table_index == -1) {
            return _UNIT_FAIL;
        }

        coff_relocation->symbol_table_index = symbol_table_index;
        coff_relocation->offset = relocation->offset;
        coff_relocation->type = COFF_REL_AMD64_REL32;
    }

    return _UNIT_OK;
}

static UNIT_Status
build_coff_object(COFF_Object *coff_object, const _UNIT_CompileContext *compile_context)
{
    assert(coff_object != NULL);
    assert(compile_context != NULL);

    if (UNIT_FAILED(COFF_Object_Init(coff_object, compile_context->context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(build_text_section(coff_object, compile_context))) {
        goto error;
    }

    return _UNIT_OK;

error:
    COFF_Object_Clear(coff_object);
    return _UNIT_FAIL;
}

static UNIT_Status
write_coff_header(COFF_Object *coff_object, UNIT_Context *context, FILE *file)
{
    assert(coff_object != NULL);
    assert(file != NULL);
    assert(context != NULL);

    WRITE_U16(COFF_MACHINE_AMD64);
    uint16_t num_sections = _UNIT_Vector_SIZE(&coff_object->sections);
    WRITE_U16(num_sections);
    WRITE_U32(0); // Timestamp; left zero to keep things deterministic

    uint32_t sections_size = 0;
    for (uint16_t index = 0; index < num_sections; ++index) {
        COFF_Section *section = _UNIT_Vector_GET(&coff_object->sections, index);
        assert(section != NULL);
        sections_size += COFF_RELOCATION_SIZE * _UNIT_Vector_SIZE(&section->relocations);
        sections_size += section->data->size;
    }

    uint32_t symbol_table_offset = COFF_FILE_HEADER_SIZE +
                                   (COFF_SECTION_HEADER_SIZE * num_sections)
                                   + sections_size;
    WRITE_U32(symbol_table_offset);
    WRITE_U32(_UNIT_Vector_SIZE(&coff_object->symbols));

    WRITE_U16(0); // Size of optional header
    WRITE_U16(0); // Characteristics

    return _UNIT_OK;
}

static UNIT_Status
write_section_header(UNIT_Context *context,
                     COFF_Section *section,
                     FILE *file,
                     uint32_t data_offset)
{
    assert(context != NULL);
    assert(section != NULL);
    assert(file != NULL);

    WRITE_NAME(section->name);
    WRITE_U32(0); // Virtual address
    WRITE_U32(0); // Virtual size

    WRITE_U32(section->data->size);
    WRITE_U32(data_offset);
    WRITE_U32(data_offset + section->data->size); // Offset to relocations

    WRITE_U32(0); // Offset to line numbers

    WRITE_U16(_UNIT_Vector_SIZE(&section->relocations));
    WRITE_U16(0); // Numbers of linenos

    WRITE_U32(COFF_SCN_MEM_EXECUTE | COFF_SCN_MEM_READ | COFF_SCN_CNT_CODE); // Characteristics

    return _UNIT_OK;
}

static UNIT_Status
write_section_data(UNIT_Context *context, COFF_Section *section, FILE *file)
{
    assert(context != NULL);
    assert(section != NULL);
    assert(file != NULL);

    if (UNIT_FAILED(write_bytes(context,
                                file,
                                section->data->data,
                                section->data->size))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static UNIT_Status
write_section_relocations(UNIT_Context *context, COFF_Section *section, FILE *file)
{
    assert(context != NULL);
    assert(section != NULL);
    assert(file != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&section->relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        COFF_Relocation *relocation = _UNIT_Vector_GET(&section->relocations, index);
        assert(relocation != NULL);
        WRITE_U32(relocation->offset);
        WRITE_U32(relocation->symbol_table_index);
        WRITE_U16(relocation->type);
    }

    return _UNIT_OK;
}

static UNIT_Status
write_coff_sections(COFF_Object *object, UNIT_Context *context, FILE *file)
{
    assert(object != NULL);
    assert(file != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(&object->sections);
    uint32_t data_offset = COFF_FILE_HEADER_SIZE + (size * COFF_SECTION_HEADER_SIZE);

    for (UNIT_Size index = 0; index < size; ++index) {
        COFF_Section *section = _UNIT_Vector_GET(&object->sections, index);
        assert(section != NULL);

        if (UNIT_FAILED(write_section_header(context, section, file, data_offset))) {
            return _UNIT_FAIL;
        }

        UNIT_Size relocation_offset = _UNIT_Vector_SIZE(&section->relocations) *
                                      COFF_RELOCATION_SIZE;
        data_offset += section->data->size + relocation_offset;
    }

    for (UNIT_Size index = 0; index < size; ++index) {
        COFF_Section *section = _UNIT_Vector_GET(&object->sections, index);
        assert(section != NULL);
        if (UNIT_FAILED(write_section_data(context, section, file))) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(write_section_relocations(context, section, file))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
write_symbol_table(COFF_Object *object, UNIT_Context *context, FILE *file)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(file != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&object->symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        COFF_Symbol *symbol = _UNIT_Vector_GET(&object->symbols, index);
        assert(symbol != NULL);
        if (UNIT_FAILED(write_bytes(context, file, (char *)symbol, 8))) {
            return _UNIT_FAIL;
        }

        WRITE_U32(symbol->section_offset);
        WRITE_I16(symbol->section_number);
        WRITE_U16(symbol->type);
        WRITE_U8(symbol->storage_class);
        WRITE_U8(0); // Number of aux symbols; always zero
    }

    return _UNIT_OK;
}

static UNIT_Status
write_string_table(COFF_Object *object, UNIT_Context *context, FILE *file)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(file != NULL);

    uint32_t string_table_size = 4;

    UNIT_Size size = _UNIT_Vector_SIZE(&object->strings);
    for (UNIT_Size index = 0; index < size; ++index) {
        char *string = _UNIT_Vector_GET(&object->strings, index);
        assert(string != NULL);
        string_table_size += strlen(string) + 1;
    }

    WRITE_U32(string_table_size);

    for (UNIT_Size index = 0; index < size; ++index) {
        char *string = _UNIT_Vector_GET(&object->strings, index);
        assert(string != NULL);
        if (UNIT_FAILED(write_bytes(context, file, string, strlen(string) + 1))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
write_coff_object_to_file(COFF_Object *object, UNIT_Context *context, const char *path)
{
    assert(object != NULL);
    assert(context != NULL);
    assert(path != NULL);

    FILE *file = fopen(path, "wb");
    if (!file) {
        _UNIT_SetOSError(context, "writing COFF object");
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(write_coff_header(object, context, file))) {
        goto error;
    }

    if (UNIT_FAILED(write_coff_sections(object, context, file))) {
        goto error;
    }

    if (UNIT_FAILED(write_symbol_table(object, context, file))) {
        goto error;
    }

    if (UNIT_FAILED(write_string_table(object, context, file))) {
        goto error;
    }

    fclose(file);
    return _UNIT_OK;
error:
    fclose(file);
    return _UNIT_FAIL;
}

UNIT_Status
_UNIT_COFF_WriteObjectFile(const _UNIT_CompileContext *compile_context,
                           const char *path)
{
    assert(compile_context != NULL);
    assert(path != NULL);

    COFF_Object coff_object;
    if (UNIT_FAILED(build_coff_object(&coff_object, compile_context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(write_coff_object_to_file(&coff_object, compile_context->context, path))) {
        COFF_Object_Clear(&coff_object);
        return _UNIT_FAIL;
    }

    COFF_Object_Clear(&coff_object);
    return _UNIT_OK;
}
