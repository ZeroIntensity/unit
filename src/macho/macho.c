// ================================================================
// HOW MACH-O WORKS
// ================================================================
//
// A Mach-O (Mach Object) file is the standard object file format
// on Apple systems (macOS, iOS, etc). For relocatable .o files,
// the structure is:
//
//   +-------------------+
//   | Mach-O Header     |  32 bytes for 64-bit. Contains magic
//   |                   |  (0xFEEDFACF), CPU type, file type,
//   |                   |  number of load commands.
//   +-------------------+
//   | Load Commands     |  Variable-length commands that describe
//   |                   |  segments, symbol tables, etc.
//   |   LC_SEGMENT_64   |  Contains section headers for __text
//   |                   |  and __cstring.
//   |   LC_SYMTAB       |  Symbol table location and size.
//   |   LC_DYSYMTAB     |  Dynamic symbol table info.
//   |   LC_BUILD_VERSION|  Minimum OS version info.
//   +-------------------+
//   | __TEXT,__text      |  Machine code (executable).
//   +-------------------+
//   | __TEXT,__cstring   |  String constants (read-only data).
//   +-------------------+
//   | Relocations       |  For __text section. Each entry tells
//   |                   |  the linker where to patch.
//   +-------------------+
//   | Symbol Table      |  Array of nlist_64 entries.
//   +-------------------+
//   | String Table      |  Null-terminated strings for symbols.
//   +-------------------+
//
// KEY DIFFERENCES FROM ELF:
//
//   - Sections live inside segments (LC_SEGMENT_64) rather than
//     being top-level.
//   - Relocations reference symbols directly (no addend in the
//     relocation entry itself for ARM64 — the addend is implicit
//     or encoded in the instruction).
//   - ARM64 relocations:
//     * ARM64_RELOC_BRANCH26 for BL instructions (calls)
//     * ARM64_RELOC_PAGE21 for ADRP (page-relative)
//     * ARM64_RELOC_PAGEOFF12 for ADD #lo12 (page offset)
//   - Symbol names are prefixed with underscore (_) by convention.
//   - Section numbering is 1-based (section 1 = __text).
//
// ================================================================

#include <stdio.h>
#include <string.h>

#include <unit/base.h>
#include <unit/errors.h>

#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/executable_formats.h>
#include <unit/internal/size_map.h>
#include <unit/internal/vector.h>

#include "macho_local.h"

typedef struct {
    UNIT_Context *context;
    MachO_Header64 header;
    MachO_SegmentCommand64 segment;
    MachO_Section64 text_section;
    MachO_Section64 cstring_section;
    MachO_SymtabCommand symtab_cmd;
    MachO_DysymtabCommand dysymtab_cmd;
    MachO_BuildVersionCommand build_version_cmd;
    _UNIT_Vector symbols;        /* MachO_Nlist64* */
    _UNIT_Vector string_table;   /* char* (one per byte) */
    _UNIT_Vector relocations;    /* MachO_RelocationInfo* */
    const _UNIT_CodeBuffer *text;
    const _UNIT_CodeBuffer *constant_data;
    _UNIT_SizeMap symtab_indices; /* internal index -> Mach-O symbol index */
    UNIT_Size num_local_syms;
    UNIT_Size num_extdef_syms;
    UNIT_Size num_undef_syms;
} _UNIT_MachO_Object;

/* Append a string to the string table, return its offset */
static UNIT_Size
append_string(_UNIT_MachO_Object *object, const char *string)
{
    assert(object != NULL);
    assert(string != NULL);
    _UNIT_Vector *st = &object->string_table;
    UNIT_Size offset = _UNIT_Vector_SIZE(st);
    UNIT_Size length = strlen(string);

    for (UNIT_Size i = 0; i <= length; ++i) {
        char *ch = _UNIT_Alloc(object->context, sizeof(char));
        if (ch == NULL) return (UNIT_Size)-1;
        *ch = string[i];
        if (UNIT_FAILED(_UNIT_Vector_Append(st, ch))) return (UNIT_Size)-1;
    }
    return offset;
}

/* Append a string with leading underscore (Mach-O convention) */
static UNIT_Size
append_string_with_underscore(_UNIT_MachO_Object *object, const char *name)
{
    assert(object != NULL);
    assert(name != NULL);
    _UNIT_Vector *st = &object->string_table;
    UNIT_Size offset = _UNIT_Vector_SIZE(st);

    /* prepend '_' */
    char *underscore = _UNIT_Alloc(object->context, sizeof(char));
    if (underscore == NULL) return (UNIT_Size)-1;
    *underscore = '_';
    if (UNIT_FAILED(_UNIT_Vector_Append(st, underscore))) return (UNIT_Size)-1;

    UNIT_Size length = strlen(name);
    for (UNIT_Size i = 0; i <= length; ++i) {
        char *ch = _UNIT_Alloc(object->context, sizeof(char));
        if (ch == NULL) return (UNIT_Size)-1;
        *ch = name[i];
        if (UNIT_FAILED(_UNIT_Vector_Append(st, ch))) return (UNIT_Size)-1;
    }
    return offset;
}

static MachO_Nlist64 *
create_and_store_symbol(_UNIT_MachO_Object *object)
{
    MachO_Nlist64 *sym = _UNIT_Alloc(object->context, sizeof(MachO_Nlist64));
    if (sym == NULL) return NULL;
    memset(sym, 0, sizeof(MachO_Nlist64));
    if (UNIT_FAILED(_UNIT_Vector_Append(&object->symbols, sym))) return NULL;
    return sym;
}

/*
 * Builds the full Mach-O symbol table in the correct order:
 *   1. Local symbols (ltmp* for string data references)
 *   2. External defined symbols (our generated functions)
 *   3. Undefined external symbols (libc functions we call)
 *
 * Also builds the relocation table at the same time, since
 * data relocations create local symbols.
 */
static UNIT_Status
build_symbols_and_relocations(_UNIT_MachO_Object *object,
                              const _UNIT_CompileContext *compile_context,
                              UNIT_Size cstring_address)
{
    const _UNIT_Vector *symbols = &compile_context->symbol_table.symbols;
    const _UNIT_Vector *relocations = &compile_context->symbol_table.relocations;
    UNIT_Size sym_count = _UNIT_Vector_SIZE(symbols);
    UNIT_Size reloc_count = _UNIT_Vector_SIZE(relocations);

    /* Count symbol categories */
    UNIT_Size num_extdef = 0;
    UNIT_Size num_undef = 0;
    UNIT_Size num_data_relocs = 0;

    for (UNIT_Size i = 0; i < sym_count; ++i) {
        _UNIT_Symbol *sym = _UNIT_Vector_GET(symbols, i);
        if (sym->is_defined) num_extdef++;
        else num_undef++;
    }

    for (UNIT_Size i = 0; i < reloc_count; ++i) {
        _UNIT_Relocation *reloc = _UNIT_Vector_GET(relocations, i);
        if (reloc->type == RELOCATION_DATA) num_data_relocs++;
    }

    object->num_local_syms = num_data_relocs;
    object->num_extdef_syms = num_extdef;
    object->num_undef_syms = num_undef;

    /* Phase 1: Add local symbols for each data relocation (string refs) */
    /* We need to know each local symbol's index, so we track them by
     * the relocation index. */
    _UNIT_SizeMap data_reloc_sym_indices;
    if (UNIT_FAILED(_UNIT_SizeMap_Init(&data_reloc_sym_indices,
                                       object->context, num_data_relocs + 1))) {
        return _UNIT_FAIL;
    }

    for (UNIT_Size i = 0; i < reloc_count; ++i) {
        _UNIT_Relocation *reloc = _UNIT_Vector_GET(relocations, i);
        if (reloc->type != RELOCATION_DATA) continue;

        UNIT_Size local_sym_idx = _UNIT_Vector_SIZE(&object->symbols);

        MachO_Nlist64 *nlist = create_and_store_symbol(object);
        if (nlist == NULL) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }

        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "ltmp%d", (int)i);
        nlist->n_strx = append_string(object, name_buf);
        if (nlist->n_strx == (uint32_t)-1) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
        nlist->n_type = MACHO_N_SECT;
        nlist->n_sect = 2; /* __cstring, section 2 (1-based) */
        nlist->n_desc = 0;
        nlist->n_value = cstring_address + reloc->symbol_index;

        if (UNIT_FAILED(_UNIT_SizeMap_Set(&data_reloc_sym_indices, i,
                                          local_sym_idx))) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
    }

    /* Phase 2: Add external defined symbols */
    UNIT_Size sym_index = _UNIT_Vector_SIZE(&object->symbols);

    for (UNIT_Size i = 0; i < sym_count; ++i) {
        _UNIT_Symbol *sym = _UNIT_Vector_GET(symbols, i);
        if (!sym->is_defined) continue;

        MachO_Nlist64 *nlist = create_and_store_symbol(object);
        if (nlist == NULL) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }

        nlist->n_strx = append_string_with_underscore(object, sym->name);
        if (nlist->n_strx == (uint32_t)-1) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
        nlist->n_type = MACHO_N_SECT | MACHO_N_EXT;
        nlist->n_sect = 1; /* __text, section 1 */
        nlist->n_desc = 0;
        nlist->n_value = sym->text_offset;

        if (UNIT_FAILED(_UNIT_SizeMap_Set(&object->symtab_indices, i,
                                          sym_index++))) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
    }

    /* Phase 3: Add undefined external symbols */
    for (UNIT_Size i = 0; i < sym_count; ++i) {
        _UNIT_Symbol *sym = _UNIT_Vector_GET(symbols, i);
        if (sym->is_defined) continue;

        MachO_Nlist64 *nlist = create_and_store_symbol(object);
        if (nlist == NULL) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }

        nlist->n_strx = append_string_with_underscore(object, sym->name);
        if (nlist->n_strx == (uint32_t)-1) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
        nlist->n_type = MACHO_N_EXT;
        nlist->n_sect = 0;
        nlist->n_desc = 0;
        nlist->n_value = 0;

        if (UNIT_FAILED(_UNIT_SizeMap_Set(&object->symtab_indices, i,
                                          sym_index++))) {
            _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
            return _UNIT_FAIL;
        }
    }

    /* Phase 4: Build relocation entries */
    for (UNIT_Size i = 0; i < reloc_count; ++i) {
        _UNIT_Relocation *reloc = _UNIT_Vector_GET(relocations, i);

        if (reloc->type == RELOCATION_CALL) {
            UNIT_Size resolved_idx = _UNIT_SizeMap_GET(&object->symtab_indices,
                                                       reloc->symbol_index);

            MachO_RelocationInfo *entry = _UNIT_Alloc(object->context,
                                                       sizeof(MachO_RelocationInfo));
            if (entry == NULL) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }

            entry->r_address = (int32_t)reloc->offset;
            entry->r_info = MACHO_RELOC_INFO(resolved_idx, 1, 2, 1,
                                              MACHO_ARM64_RELOC_BRANCH26);

            if (UNIT_FAILED(_UNIT_Vector_Append(&object->relocations, entry))) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }
        } else if (reloc->type == RELOCATION_DATA) {
            UNIT_Size local_sym_idx = _UNIT_SizeMap_GET(&data_reloc_sym_indices, i);

            /* ARM64_RELOC_PAGE21 for ADRP */
            MachO_RelocationInfo *page_entry = _UNIT_Alloc(object->context,
                                                            sizeof(MachO_RelocationInfo));
            if (page_entry == NULL) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }
            page_entry->r_address = (int32_t)reloc->offset;
            page_entry->r_info = MACHO_RELOC_INFO(local_sym_idx, 1, 2, 1,
                                                   MACHO_ARM64_RELOC_PAGE21);
            if (UNIT_FAILED(_UNIT_Vector_Append(&object->relocations,
                                                page_entry))) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }

            /* ARM64_RELOC_PAGEOFF12 for ADD */
            MachO_RelocationInfo *off_entry = _UNIT_Alloc(object->context,
                                                           sizeof(MachO_RelocationInfo));
            if (off_entry == NULL) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }
            off_entry->r_address = (int32_t)(reloc->offset + 4);
            off_entry->r_info = MACHO_RELOC_INFO(local_sym_idx, 0, 2, 1,
                                                  MACHO_ARM64_RELOC_PAGEOFF12);
            if (UNIT_FAILED(_UNIT_Vector_Append(&object->relocations,
                                                off_entry))) {
                _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
                return _UNIT_FAIL;
            }
        }
    }

    _UNIT_SizeMap_Clear(&data_reloc_sym_indices);
    return _UNIT_OK;
}

static UNIT_Status
build_macho_object(_UNIT_MachO_Object *object,
                   const _UNIT_CompileContext *compile_context)
{
    assert(object != NULL);
    assert(compile_context != NULL);

    object->context = compile_context->context;
    object->text = &compile_context->buffer;
    object->constant_data = &compile_context->string_data.constant_buffer;

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->string_table,
                                      compile_context->context, 64,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->symbols,
                                      compile_context->context, 16,
                                      _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&object->string_table);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&object->relocations,
                                      compile_context->context, 16,
                                      _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&object->string_table);
        _UNIT_Vector_Clear(&object->symbols);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&object->symtab_indices,
                                       compile_context->context, 8))) {
        _UNIT_Vector_Clear(&object->string_table);
        _UNIT_Vector_Clear(&object->symbols);
        _UNIT_Vector_Clear(&object->relocations);
        return _UNIT_FAIL;
    }

    /* String table starts with a null byte (and a space) per Mach-O convention */
    if (append_string(object, " ") == (UNIT_Size)-1) {
        goto error;
    }

    /* Now compute file layout */
    UNIT_Size text_size = _UNIT_CodeBuffer_CurrentIndex(object->text);
    UNIT_Size cstring_size = object->constant_data->size;

    if (UNIT_FAILED(build_symbols_and_relocations(object, compile_context,
                                                  text_size))) {
        goto error;
    }

    UNIT_Size num_relocs = _UNIT_Vector_SIZE(&object->relocations);
    UNIT_Size num_syms = _UNIT_Vector_SIZE(&object->symbols);
    UNIT_Size strtab_size = _UNIT_Vector_SIZE(&object->string_table);

    /* We have 2 sections: __text and __cstring */
    UNIT_Size num_sections = (cstring_size > 0) ? 2 : 1;

    /* Load commands:
     * 1. LC_SEGMENT_64 (with sections)
     * 2. LC_BUILD_VERSION
     * 3. LC_SYMTAB
     * 4. LC_DYSYMTAB */
    UNIT_Size segment_cmd_size = sizeof(MachO_SegmentCommand64)
                               + num_sections * sizeof(MachO_Section64);
    UNIT_Size build_version_size = sizeof(MachO_BuildVersionCommand);
    UNIT_Size symtab_cmd_size = sizeof(MachO_SymtabCommand);
    UNIT_Size dysymtab_cmd_size = sizeof(MachO_DysymtabCommand);
    UNIT_Size total_cmds_size = segment_cmd_size + build_version_size
                              + symtab_cmd_size + dysymtab_cmd_size;

    UNIT_Size header_size = sizeof(MachO_Header64);
    UNIT_Size text_offset = header_size + total_cmds_size;
    UNIT_Size cstring_offset = text_offset + text_size;
    UNIT_Size reloc_offset = cstring_offset + cstring_size;
    UNIT_Size symtab_offset = reloc_offset + num_relocs * sizeof(MachO_RelocationInfo);
    UNIT_Size strtab_offset = symtab_offset + num_syms * sizeof(MachO_Nlist64);

    UNIT_Size num_locals = object->num_local_syms;

    /* Populate header */
    memset(&object->header, 0, sizeof(MachO_Header64));
    object->header.magic = MACHO_MAGIC_64;
    object->header.cputype = MACHO_CPU_TYPE_ARM64;
    object->header.cpusubtype = MACHO_CPU_SUBTYPE_ARM64_ALL;
    object->header.filetype = MACHO_FILETYPE_OBJECT;
    object->header.ncmds = 4;
    object->header.sizeofcmds = (uint32_t)total_cmds_size;
    object->header.flags = MACHO_FLAG_SUBSECTIONS_VIA_SYMBOLS;
    object->header.reserved = 0;

    /* LC_SEGMENT_64 */
    memset(&object->segment, 0, sizeof(MachO_SegmentCommand64));
    object->segment.cmd = MACHO_LC_SEGMENT_64;
    object->segment.cmdsize = (uint32_t)segment_cmd_size;
    /* segname is empty for object files */
    object->segment.vmaddr = 0;
    object->segment.vmsize = text_size + cstring_size;
    object->segment.fileoff = text_offset;
    object->segment.filesize = text_size + cstring_size;
    object->segment.maxprot = 7;  /* rwx */
    object->segment.initprot = 7;
    object->segment.nsects = (uint32_t)num_sections;
    object->segment.flags = 0;

    /* __TEXT,__text section */
    memset(&object->text_section, 0, sizeof(MachO_Section64));
    strncpy(object->text_section.sectname, "__text", 16);
    strncpy(object->text_section.segname, "__TEXT", 16);
    object->text_section.addr = 0;
    object->text_section.size = text_size;
    object->text_section.offset = (uint32_t)text_offset;
    object->text_section.align = 2;  /* 2^2 = 4 byte alignment */
    object->text_section.reloff = (num_relocs > 0) ? (uint32_t)reloc_offset : 0;
    object->text_section.nreloc = (uint32_t)num_relocs;
    object->text_section.flags = MACHO_S_ATTR_PURE_INSTRUCTIONS
                               | MACHO_S_ATTR_SOME_INSTRUCTIONS;

    /* __TEXT,__cstring section */
    if (num_sections > 1) {
        memset(&object->cstring_section, 0, sizeof(MachO_Section64));
        strncpy(object->cstring_section.sectname, "__cstring", 16);
        strncpy(object->cstring_section.segname, "__TEXT", 16);
        object->cstring_section.addr = text_size;
        object->cstring_section.size = cstring_size;
        object->cstring_section.offset = (uint32_t)cstring_offset;
        object->cstring_section.align = 0;  /* byte aligned */
        object->cstring_section.reloff = 0;
        object->cstring_section.nreloc = 0;
        object->cstring_section.flags = MACHO_S_CSTRING_LITERALS;
    }

    /* LC_BUILD_VERSION (macOS, ARM64) */
    memset(&object->build_version_cmd, 0, sizeof(MachO_BuildVersionCommand));
    object->build_version_cmd.cmd = MACHO_LC_BUILD_VERSION;
    object->build_version_cmd.cmdsize = (uint32_t)build_version_size;
    object->build_version_cmd.platform = 1;  /* PLATFORM_MACOS */
    object->build_version_cmd.minos = 0x000E0000;  /* 14.0.0 */
    object->build_version_cmd.sdk = 0x000E0000;
    object->build_version_cmd.ntools = 0;

    /* LC_SYMTAB */
    memset(&object->symtab_cmd, 0, sizeof(MachO_SymtabCommand));
    object->symtab_cmd.cmd = MACHO_LC_SYMTAB;
    object->symtab_cmd.cmdsize = (uint32_t)symtab_cmd_size;
    object->symtab_cmd.symoff = (uint32_t)symtab_offset;
    object->symtab_cmd.nsyms = (uint32_t)num_syms;
    object->symtab_cmd.stroff = (uint32_t)strtab_offset;
    object->symtab_cmd.strsize = (uint32_t)strtab_size;

    /* LC_DYSYMTAB */
    memset(&object->dysymtab_cmd, 0, sizeof(MachO_DysymtabCommand));
    object->dysymtab_cmd.cmd = MACHO_LC_DYSYMTAB;
    object->dysymtab_cmd.cmdsize = (uint32_t)dysymtab_cmd_size;
    object->dysymtab_cmd.ilocalsym = 0;
    object->dysymtab_cmd.nlocalsym = (uint32_t)num_locals;
    object->dysymtab_cmd.iextdefsym = (uint32_t)num_locals;
    object->dysymtab_cmd.nextdefsym = (uint32_t)object->num_extdef_syms;
    object->dysymtab_cmd.iundefsym = (uint32_t)(num_locals + object->num_extdef_syms);
    object->dysymtab_cmd.nundefsym = (uint32_t)object->num_undef_syms;

    return _UNIT_OK;

error:
    _UNIT_Vector_Clear(&object->string_table);
    _UNIT_Vector_Clear(&object->symbols);
    _UNIT_Vector_Clear(&object->relocations);
    _UNIT_SizeMap_Clear(&object->symtab_indices);
    return _UNIT_FAIL;
}

static UNIT_Status
write_object_to_file(const _UNIT_MachO_Object *object, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (!file) {
        _UNIT_SetOSError(object->context, "writing Mach-O object");
        return _UNIT_FAIL;
    }

    UNIT_Size cstring_size = object->constant_data->size;
    UNIT_Size num_sections = (cstring_size > 0) ? 2 : 1;

    /* Mach-O Header */
    fwrite(&object->header, sizeof(MachO_Header64), 1, file);

    /* LC_SEGMENT_64 */
    fwrite(&object->segment, sizeof(MachO_SegmentCommand64), 1, file);
    fwrite(&object->text_section, sizeof(MachO_Section64), 1, file);
    if (num_sections > 1) {
        fwrite(&object->cstring_section, sizeof(MachO_Section64), 1, file);
    }

    /* LC_BUILD_VERSION */
    fwrite(&object->build_version_cmd, sizeof(MachO_BuildVersionCommand), 1, file);

    /* LC_SYMTAB */
    fwrite(&object->symtab_cmd, sizeof(MachO_SymtabCommand), 1, file);

    /* LC_DYSYMTAB */
    fwrite(&object->dysymtab_cmd, sizeof(MachO_DysymtabCommand), 1, file);

    /* __text data */
    fwrite(object->text->data, 1, object->text->size, file);

    /* __cstring data */
    if (cstring_size > 0) {
        fwrite(object->constant_data->data, 1, cstring_size, file);
    }

    /* Relocations */
    UNIT_Size num_relocs = _UNIT_Vector_SIZE(&object->relocations);
    for (UNIT_Size i = 0; i < num_relocs; ++i) {
        MachO_RelocationInfo *rel = _UNIT_Vector_GET(&object->relocations, i);
        fwrite(rel, sizeof(MachO_RelocationInfo), 1, file);
    }

    /* Symbol table */
    UNIT_Size num_syms = _UNIT_Vector_SIZE(&object->symbols);
    for (UNIT_Size i = 0; i < num_syms; ++i) {
        MachO_Nlist64 *sym = _UNIT_Vector_GET(&object->symbols, i);
        fwrite(sym, sizeof(MachO_Nlist64), 1, file);
    }

    /* String table */
    UNIT_Size strtab_size = _UNIT_Vector_SIZE(&object->string_table);
    for (UNIT_Size i = 0; i < strtab_size; ++i) {
        char *ch = _UNIT_Vector_GET(&object->string_table, i);
        fwrite(ch, 1, 1, file);
    }

    fclose(file);
    return _UNIT_OK;
}

UNIT_Status
_UNIT_MachO_WriteObjectFile(const _UNIT_CompileContext *context, const char *path)
{
    assert(context != NULL);
    assert(path != NULL);

    _UNIT_MachO_Object macho_object;
    if (UNIT_FAILED(build_macho_object(&macho_object, context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(write_object_to_file(&macho_object, path))) {
        return _UNIT_FAIL;
    }

    _UNIT_SizeMap_Clear(&macho_object.symtab_indices);
    _UNIT_Vector_Clear(&macho_object.relocations);
    _UNIT_Vector_Clear(&macho_object.string_table);
    _UNIT_Vector_Clear(&macho_object.symbols);

    return _UNIT_OK;
}
