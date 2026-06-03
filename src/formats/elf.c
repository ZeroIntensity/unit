// ================================================================
// HOW ELF WORKS (by Claude)
// ================================================================
//
// An ELF (Executable and Linkable Format) object file is the
// standard format for relocatable object files, shared libraries,
// and executables on Linux and most Unix systems.
//
// The file is organized as:
//
//   +-------------------+
//   | ELF Header        |  Fixed-size header at offset 0. Contains
//   |                   |  the magic number (7f 45 4c 46), the
//   |                   |  machine type, and a pointer to the
//   |                   |  section header table.
//   +-------------------+
//   | Section Header    |  Array of section headers. Each entry
//   | Table             |  describes one section: its type, file
//   |                   |  offset, size, and flags.
//   +-------------------+
//   | .text             |  Machine code. Flagged as allocatable
//   |                   |  and executable.
//   +-------------------+
//   | .rodata           |  Read-only data. String literals and
//   |                   |  constants live here. Flagged as
//   |                   |  allocatable but not writable or
//   |                   |  executable.
//   +-------------------+
//   | .rela.text        |  Relocation entries for .text. Each one
//   |                   |  says "at byte offset X, patch in the
//   |                   |  address of symbol Y with addend Z."
//   |                   |  The linker processes these to resolve
//   |                   |  external function calls and data refs.
//   +-------------------+
//   | .symtab           |  Symbol table. Each entry has a name
//   |                   |  (offset into .strtab), a type (function,
//   |                   |  object, etc.), a binding (local/global),
//   |                   |  and either a defined value (offset in a
//   |                   |  section) or SHN_UNDEF for externals.
//   +-------------------+
//   | .strtab           |  String table for symbol names. Just a
//   |                   |  blob of null-terminated strings packed
//   |                   |  together. Symbol entries point into this
//   |                   |  by byte offset.
//   +-------------------+
//   | .shstrtab         |  String table for section names. Same
//   |                   |  format as .strtab but used by the
//   |                   |  section headers instead of symbols.
//   +-------------------+
//
// KEY RELATIONSHIPS:
//
//   - Section headers reference .shstrtab for their names (by
//     byte offset into the string table).
//
//   - .symtab's sh_link points to .strtab so the linker knows
//     where to find symbol name strings.
//
//   - .rela.text's sh_link points to .symtab (which symbols the
//     relocations reference) and sh_info points to .text (which
//     section the relocations patch).
//
//   - Relocations use R_X86_64_PLT32 for function calls. The
//     linker computes: S + A - P, where S is the symbol address,
//     A is the addend (-4, to account for the displacement being
//     relative to the end of the instruction, not the start of
//     the 4-byte field), and P is the patch location.
//
//   - String literals are stored in .rodata. Code references them
//     via RIP-relative addressing with R_X86_64_PC32 relocations
//     that point at the .rodata section symbol. The addend is the
//     offset of the string within .rodata, minus 4 (for the same
//     reason as PLT32).
//
// ================================================================

#include <stdio.h>
#include <string.h>

#include <unit/base.h>
#include <unit/errors.h>
#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/size_map.h>
#include <unit/internal/vector.h>

#include <unit/internal/formats/elf.h>

typedef struct {
    UNIT_Context *context;
    ELF_Header header;
    ELF_SectionHeader sections[7];
    _UNIT_Vector symbols;
    _UNIT_Vector string_table;
    _UNIT_Vector relocations_table;
    _UNIT_CodeBuffer *text;
    _UNIT_CodeBuffer *constant_data;
    const char *section_string_table;
    UNIT_Size section_string_table_size;
    _UNIT_SizeMap symtab_indices;
} _UNIT_ELF_Object;

enum {
    SECTION_NULL = 0,
    SECTION_TEXT = 1,
    SECTION_RODATA = 2,
    SECTION_RELA_TEXT = 3,
    SECTION_SYMTAB = 4,
    SECTION_STRTAB = 5,
    SECTION_SHSTRTAB = 6,
    SECTION_COUNT = 7
};

static const char section_string_table[] =
    "\0"
    ".text\0"         // offset 1
    ".rodata\0"       // offset 7
    ".rela.text\0"    // offset 15
    ".symtab\0"       // offset 26
    ".strtab\0"       // offset 34
    ".shstrtab\0";    // offset 42

// Appends a null-terminated string to the string table vector.
// Returns the byte offset where the string starts, which is
// what symbol and section header name fields expect.
static UNIT_Size
append_string(_UNIT_ELF_Object *object, const char *string)
{
    assert(object != NULL);
    assert(string != NULL);
    _UNIT_Vector *string_table = &object->string_table;
    UNIT_Size offset = _UNIT_Vector_SIZE(string_table);
    UNIT_Size length = strlen(string);

    for (UNIT_Size index = 0; index <= length; ++index) {
        char *character = _UNIT_Alloc(object->context, sizeof(char));
        if (character == NULL) {
            return -1;
        }
        *character = string[index];
        if (UNIT_FAILED(_UNIT_Vector_Append(string_table,
                                            character))) {
            return -1;
        }
    }

    return offset;
}

static ELF_Symbol *
create_and_store_symbol(_UNIT_ELF_Object *object)
{
    assert(object != NULL);
    ELF_Symbol *symbol = _UNIT_Alloc(object->context, sizeof(ELF_Symbol));
    if (symbol == NULL) {
        return NULL;
    }
    memset(symbol, 0, sizeof(ELF_Symbol));
    if (UNIT_FAILED(_UNIT_Vector_Append(&object->symbols, symbol))) {
        return NULL;
    }
    return symbol;
}

/* Adds a null symbol at index 0 of the symbol table, because
 * the ELF spec mandates that the first entry is always zeroed. */
static UNIT_Status
add_null_symbol(_UNIT_ELF_Object *object)
{
    assert(object != NULL);
    assert(_UNIT_Vector_SIZE(&object->symbols) == 0);
    ELF_Symbol *symbol = create_and_store_symbol(object);
    if (symbol == NULL) {
        return UNIT_FAIL;
    }
    return UNIT_OK;
}

/* Adds a local section symbol for .rodata at index 1. Relocations
 * that load string addresses point at this symbol, with an addend
 * equal to the string's byte offset within .rodata. */
static UNIT_Status
add_rodata_section_symbol(_UNIT_ELF_Object *object)
{
    assert(object != NULL);
    assert(_UNIT_Vector_SIZE(&object->symbols) == 1);
    ELF_Symbol *symbol = create_and_store_symbol(object);
    if (symbol == NULL) {
        return UNIT_FAIL;
    }
    symbol->info = ELF_SYMBOL_INFO(ELF_SYMBOL_BINDING_LOCAL,
                                   ELF_SYMBOL_TYPE_SECTION);
    symbol->section_index = SECTION_RODATA;
    return UNIT_OK;
}

/* Adds the entry point symbol. */
static UNIT_Status
add_start_symbol(_UNIT_ELF_Object *object)
{
    _UNIT_Vector *symbols = &object->symbols;
    assert(_UNIT_Vector_SIZE(symbols) == 2);
    ELF_Symbol *symbol = create_and_store_symbol(object);
    if (symbol == NULL) {
        return UNIT_FAIL;
    }
    symbol->name = append_string(object, "main");
    if (symbol->name == -1) {
        return UNIT_FAIL;
    }
    symbol->info = ELF_SYMBOL_INFO(ELF_SYMBOL_BINDING_GLOBAL,
                                   ELF_SYMBOL_TYPE_FUNCTION);
    symbol->section_index = SECTION_TEXT;
    symbol->size = _UNIT_CodeBuffer_CurrentIndex(object->text);
    return UNIT_OK;
}

/* Adds undefined symbols for each external function referenced
 * by the program (e.g. libc functions like putchar, getchar).
 * Also builds a mapping from internal symbol IDs (from the
 * procedure) to their indices in the ELF symbol table, so
 * relocations can reference them. */
static UNIT_Status
add_external_symbols(_UNIT_ELF_Object *object,
                     _UNIT_CompileContext *compile_context)
{
    UNIT_Size symtab_index = _UNIT_Vector_SIZE(&object->symbols);
    _UNIT_Vector *symbol_names = compile_context->symbol_table.names;

    UNIT_Size size = _UNIT_Vector_SIZE(symbol_names);
    for (UNIT_Size index = 0; index < size; ++index) {
        char *name = _UNIT_Vector_GET(symbol_names, index);
        ELF_Symbol *symbol = create_and_store_symbol(object);
        if (symbol == NULL) {
            return UNIT_FAIL;
        }
        symbol->name = append_string(object, name);
        if (symbol->name == -1) {
            return UNIT_FAIL;
        }
        symbol->info = ELF_SYMBOL_INFO(ELF_SYMBOL_BINDING_GLOBAL,
                                       ELF_SYMBOL_TYPE_NONE);
        symbol->section_index = ELF_SECTION_UNDEFINED;

        if (UNIT_FAILED(_UNIT_SizeMap_Set(&object->symtab_indices, index,
                                          symtab_index++))) {
            return UNIT_FAIL;
        }
    }

    return UNIT_OK;
}

/* Converts our internal relocations into ELF relocations.
 * Function call relocations use R_X86_64_PLT32.
 * String/data relocations use R_X86_64_PC32 and point at the
 * .rodata section symbol (index 1). */
static UNIT_Status
build_relocation_table(_UNIT_ELF_Object *object,
                       _UNIT_CompileContext *compile_context)
{
    _UNIT_Vector *relocations = &compile_context->symbol_table.relocations;
    UNIT_Size count = _UNIT_Vector_SIZE(relocations);

    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_Relocation *relocation = _UNIT_Vector_GET(relocations, index);

        ELF_RelocationAddend *entry = _UNIT_Alloc(object->context, sizeof(ELF_RelocationAddend));
        if (entry == NULL) {
            return UNIT_FAIL;
        }
        memset(entry, 0, sizeof(ELF_RelocationAddend));

        entry->offset = relocation->offset;

        if (relocation->type == RELOCATION_CALL) {
            UNIT_Size resolved_index = _UNIT_SizeMap_GET(&object->symtab_indices,
                                                         relocation->symbol_index);
            entry->info = ELF_RELOCATION_INFO(resolved_index,
                                              ELF_RELOCATION_X86_64_PLT32);
            entry->add = -4;
        } else if (relocation->type == RELOCATION_DATA) {
            // Points at the .rodata section symbol (index 1).
            // The addend is the byte offset of the data within
            // .rodata, minus 4 for the RIP-relative adjustment.
            entry->info = ELF_RELOCATION_INFO(1,
                                              ELF_RELOCATION_X86_64_PC32);
            entry->add = relocation->symbol_index - 4;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(&object->relocations_table, entry))) {
            return UNIT_FAIL;
        }
    }

    return UNIT_OK;
}

/* Fills in the section header table. Computes file offsets for
 * each section based on the sizes of all preceding sections. */
static void
build_section_headers(_UNIT_ELF_Object *object,
                      _UNIT_CompileContext *compile_context)
{
    UNIT_Size header_size = sizeof(ELF_Header);

    ELF_SectionHeader *sections = object->sections;
    memset(sections, 0, sizeof(ELF_SectionHeader) * SECTION_COUNT);

    UNIT_Size text_size = _UNIT_CodeBuffer_CurrentIndex(object->text);
    UNIT_Size section_headers_size = sizeof(ELF_SectionHeader) * SECTION_COUNT;
    UNIT_Size text_offset = header_size + section_headers_size;

    sections[SECTION_TEXT] = (ELF_SectionHeader) {
        .name = 1,
        .type = ELF_SECTION_TYPE_PROGRAM_DATA,
        .flags = ELF_SECTION_FLAG_ALLOC | ELF_SECTION_FLAG_EXECUTABLE,
        .offset = text_offset,
        .size = text_size,
        .alignment = 16,
    };

    UNIT_Size rodata_offset = text_offset + text_size;
    UNIT_Size rodata_size = compile_context->string_data.constant_buffer.size;

    sections[SECTION_RODATA] = (ELF_SectionHeader) {
        .name = 7,
        .type = ELF_SECTION_TYPE_PROGRAM_DATA,
        .flags = ELF_SECTION_FLAG_ALLOC,
        .offset = rodata_offset,
        .size = rodata_size,
        .alignment = 1,
    };

    UNIT_Size relocation_offset = rodata_offset + rodata_size;
    UNIT_Size relocation_count = _UNIT_Vector_SIZE(&object->relocations_table);
    UNIT_Size relocation_table_size = relocation_count * sizeof(ELF_RelocationAddend);

    sections[SECTION_RELA_TEXT] = (ELF_SectionHeader) {
        .name = 15,
        .type = ELF_SECTION_TYPE_RELA,
        .flags = ELF_SECTION_FLAG_INFO_LINK,
        .offset = relocation_offset,
        .size = relocation_table_size,
        .link = SECTION_SYMTAB,
        .info = SECTION_TEXT,
        .alignment = 8,
        .entry_size = sizeof(ELF_RelocationAddend),
    };

    UNIT_Size symbol_offset = relocation_offset + relocation_table_size;
    UNIT_Size symbol_count = _UNIT_Vector_SIZE(&object->symbols);
    UNIT_Size symbol_table_size = symbol_count * sizeof(ELF_Symbol);

    sections[SECTION_SYMTAB] = (ELF_SectionHeader) {
        .name = 26,
        .type = ELF_SECTION_TYPE_SYMBOL_TABLE,
        .offset = symbol_offset,
        .size = symbol_table_size,
        .link = SECTION_STRTAB,
        .info = 2,  // first global symbol (after null + rodata section sym)
        .alignment = 8,
        .entry_size = sizeof(ELF_Symbol),
    };

    UNIT_Size string_table_size = _UNIT_Vector_SIZE(&object->string_table);
    UNIT_Size string_offset = symbol_offset + symbol_table_size;

    sections[SECTION_STRTAB] = (ELF_SectionHeader) {
        .name = 34,
        .type = ELF_SECTION_TYPE_STRING_TABLE,
        .offset = string_offset,
        .size = string_table_size,
        .alignment = 1,
    };

    UNIT_Size section_string_offset = string_offset + string_table_size;

    sections[SECTION_SHSTRTAB] = (ELF_SectionHeader) {
        .name = 42,
        .type = ELF_SECTION_TYPE_STRING_TABLE,
        .offset = section_string_offset,
        .size = object->section_string_table_size,
        .alignment = 1,
    };
}

static UNIT_Status
build_symbol_table(_UNIT_ELF_Object *object, _UNIT_CompileContext *context)
{
    if (UNIT_FAILED(add_null_symbol(object))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(add_rodata_section_symbol(object))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(add_start_symbol(object))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(add_external_symbols(object, context))) {
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

static void
populate_elf_data(_UNIT_ELF_Object *object,
                  _UNIT_CompileContext *compile_context)
{
    object->constant_data = &compile_context->string_data.constant_buffer;
    object->header = (ELF_Header) {
        .identification = {
            [ELF_IDENT_MAGIC_0] = 0x7f,
            [ELF_IDENT_MAGIC_1] = 'E',
            [ELF_IDENT_MAGIC_2] = 'L',
            [ELF_IDENT_MAGIC_3] = 'F',
            [ELF_IDENT_CLASS] = ELF_CLASS_64,
            [ELF_IDENT_DATA_ENCODING] = ELF_DATA_LITTLE_ENDIAN,
            [ELF_IDENT_VERSION] = ELF_VERSION_CURRENT,
        },
        .type = ELF_TYPE_RELOCATABLE,
        .machine = ELF_MACHINE_X86_64,
        .version = ELF_VERSION_CURRENT,
        .section_header_offset = sizeof(ELF_Header),
        .header_size = sizeof(ELF_Header),
        .section_header_entry_size = sizeof(ELF_SectionHeader),
        .section_header_count = SECTION_COUNT,
        .section_name_string_table_index = SECTION_SHSTRTAB,
    };
    object->section_string_table = section_string_table;
    object->section_string_table_size = sizeof(section_string_table);

    build_section_headers(object, compile_context);
}

/* Assembles the complete ELF object in memory: header, section
 * headers, symbol table, string tables, and relocation entries.
 * Does not write anything to disk. */
static UNIT_Status
build_elf_object(_UNIT_ELF_Object *object, _UNIT_CompileContext *compile_context)
{
    assert(object != NULL);
    assert(compile_context != NULL);
    object->context = compile_context->context;
    object->text = &compile_context->buffer;

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->string_table,
                                      compile_context->context,
                                      8, _UNIT_Dealloc))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->symbols,
                                      compile_context->context,
                                      16, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&object->string_table);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->relocations_table,
                                      compile_context->context, 16, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&object->string_table);
        _UNIT_Vector_Clear(&object->symbols);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&object->symtab_indices,
                                       compile_context->context, 8))) {
        _UNIT_Vector_Clear(&object->string_table);
        _UNIT_Vector_Clear(&object->symbols);
        return UNIT_FAIL;
    }

    // String table starts with a null byte
    if (append_string(object, "") == -1) {
        goto error;
    }

    if (UNIT_FAILED(build_symbol_table(object, compile_context))) {
        goto error;
    }

    if (UNIT_FAILED(build_relocation_table(object, compile_context))) {
        goto error;
    }

    populate_elf_data(object, compile_context);

    return UNIT_OK;
error:
    _UNIT_Vector_Clear(&object->string_table);
    _UNIT_Vector_Clear(&object->symbols);
    _UNIT_SizeMap_Clear(&object->symtab_indices);
    return UNIT_FAIL;
}

static UNIT_Status
write_object_to_file(_UNIT_ELF_Object *object, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        _UNIT_SetOSError(object->context, "writing ELF object");
        return UNIT_FAIL;
    }

    // ELF header
    fwrite(&object->header, sizeof(object->header), 1, file);

    // Section headers
    fwrite(object->sections, sizeof(object->sections), 1, file);

    // .text
    fwrite(object->text->data, 1, object->text->size, file);

    // .rodata
    if (object->constant_data->size > 0) {
        fwrite(object->constant_data->data, 1,
               object->constant_data->size, file);
    }

    // .rela.text
    UNIT_Size relocation_count = _UNIT_Vector_SIZE(&object->relocations_table);
    for (UNIT_Size index = 0; index < relocation_count; ++index) {
        ELF_RelocationAddend *entry = _UNIT_Vector_GET(&object->relocations_table, index);
        fwrite(entry, sizeof(ELF_RelocationAddend), 1, file);
    }

    // .symtab
    UNIT_Size symbol_count = _UNIT_Vector_SIZE(&object->symbols);
    for (UNIT_Size index = 0; index < symbol_count; ++index) {
        ELF_Symbol *symbol = _UNIT_Vector_GET(&object->symbols, index);
        fwrite(symbol, sizeof(ELF_Symbol), 1, file);
    }

    // .strtab
    UNIT_Size string_table_size = _UNIT_Vector_SIZE(&object->string_table);
    for (UNIT_Size index = 0; index < string_table_size; ++index) {
        char *character = _UNIT_Vector_GET(&object->string_table, index);
        fwrite(character, 1, 1, file);
    }

    // .shstrtab
    fwrite(object->section_string_table, 1,
           object->section_string_table_size, file);

    fclose(file);
    return UNIT_OK;
}

UNIT_Status
_UNIT_ELF_WriteObjectFile(_UNIT_CompileContext *context, const char *path)
{
    assert(context != NULL);
    assert(path != NULL);
    _UNIT_ELF_Object elf_object;
    if (UNIT_FAILED(build_elf_object(&elf_object, context))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(write_object_to_file(&elf_object, path))) {
        return UNIT_FAIL;
    }

    _UNIT_SizeMap_Clear(&elf_object.symtab_indices);
    _UNIT_Vector_Clear(&elf_object.relocations_table);
    _UNIT_Vector_Clear(&elf_object.string_table);
    _UNIT_Vector_Clear(&elf_object.symbols);

    return UNIT_OK;
}
