#ifndef UNIT_MACHO_LOCAL_H
#define UNIT_MACHO_LOCAL_H

#include <stdint.h>

/* Mach-O magic numbers */
enum {
    MACHO_MAGIC_64 = 0xFEEDFACF,
};

/* CPU types */
enum {
    MACHO_CPU_TYPE_ARM64  = 0x0100000C, /* CPU_TYPE_ARM64 */
    MACHO_CPU_TYPE_X86_64 = 0x01000007, /* CPU_TYPE_X86_64 */
};

/* CPU subtypes */
enum {
    MACHO_CPU_SUBTYPE_ARM64_ALL = 0x00000000,
    MACHO_CPU_SUBTYPE_X86_64_ALL = 0x00000003,
};

/* File types */
enum {
    MACHO_FILETYPE_OBJECT = 1, /* MH_OBJECT */
};

/* Mach-O header flags */
enum {
    MACHO_FLAG_SUBSECTIONS_VIA_SYMBOLS = 0x00002000,
};

/* Load command types */
enum {
    MACHO_LC_SEGMENT_64 = 0x19,
    MACHO_LC_SYMTAB     = 0x02,
    MACHO_LC_DYSYMTAB   = 0x0B,
    MACHO_LC_BUILD_VERSION = 0x32,
};

/* Section types and attributes */
enum {
    MACHO_S_REGULAR          = 0x00,
    MACHO_S_CSTRING_LITERALS = 0x02,

    MACHO_S_ATTR_PURE_INSTRUCTIONS = 0x80000000,
    MACHO_S_ATTR_SOME_INSTRUCTIONS = 0x00000400,
};

/* Symbol types (n_type field) */
enum {
    MACHO_N_EXT  = 0x01,  /* External symbol */
    MACHO_N_UNDF = 0x00,  /* Undefined */
    MACHO_N_SECT = 0x0E,  /* Defined in a section */
};

/* Relocation types for ARM64 */
enum {
    MACHO_ARM64_RELOC_UNSIGNED   = 0,
    MACHO_ARM64_RELOC_SUBTRACTOR = 1,
    MACHO_ARM64_RELOC_BRANCH26   = 2,
    MACHO_ARM64_RELOC_PAGE21     = 3,
    MACHO_ARM64_RELOC_PAGEOFF12  = 4,
    MACHO_ARM64_RELOC_GOT_LOAD_PAGE21 = 5,
    MACHO_ARM64_RELOC_GOT_LOAD_PAGEOFF12 = 6,
};

/* Reference types for dysymtab */
enum {
    MACHO_REFERENCE_FLAG_UNDEFINED_NON_LAZY = 0,
};

/* ---- Structures ---- */

typedef struct {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} MachO_Header64;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} MachO_SegmentCommand64;

typedef struct {
    char     sectname[16];
    char     segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} MachO_Section64;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t symoff;
    uint32_t nsyms;
    uint32_t stroff;
    uint32_t strsize;
} MachO_SymtabCommand;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t ilocalsym;
    uint32_t nlocalsym;
    uint32_t iextdefsym;
    uint32_t nextdefsym;
    uint32_t iundefsym;
    uint32_t nundefsym;
    uint32_t tocoff;
    uint32_t ntoc;
    uint32_t modtaboff;
    uint32_t nmodtab;
    uint32_t extrefsymoff;
    uint32_t nextrefsyms;
    uint32_t indirectsymoff;
    uint32_t nindirectsyms;
    uint32_t extreloff;
    uint32_t nextrel;
    uint32_t locreloff;
    uint32_t nlocrel;
} MachO_DysymtabCommand;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t platform;
    uint32_t minos;
    uint32_t sdk;
    uint32_t ntools;
} MachO_BuildVersionCommand;

typedef struct {
    uint32_t n_strx;
    uint8_t  n_type;
    uint8_t  n_sect;
    uint16_t n_desc;
    uint64_t n_value;
} MachO_Nlist64;

typedef struct {
    int32_t  r_address;
    uint32_t r_info;
    /* r_info bits: symbolnum(24) | pcrel(1) | length(2) | extern(1) | type(4) */
} MachO_RelocationInfo;

/* Pack relocation info */
#define MACHO_RELOC_INFO(symbolnum, pcrel, length, ext, type) \
    (((uint32_t)(symbolnum) & 0x00FFFFFF)       | \
     (((uint32_t)(pcrel) & 1) << 24)            | \
     (((uint32_t)(length) & 3) << 25)           | \
     (((uint32_t)(ext) & 1) << 27)              | \
     (((uint32_t)(type) & 0xF) << 28))

_Static_assert(sizeof(MachO_Header64) == 32,
               "MachO_Header64 must be 32 bytes");
_Static_assert(sizeof(MachO_SegmentCommand64) == 72,
               "MachO_SegmentCommand64 must be 72 bytes");
_Static_assert(sizeof(MachO_Section64) == 80,
               "MachO_Section64 must be 80 bytes");
_Static_assert(sizeof(MachO_SymtabCommand) == 24,
               "MachO_SymtabCommand must be 24 bytes");
_Static_assert(sizeof(MachO_DysymtabCommand) == 80,
               "MachO_DysymtabCommand must be 80 bytes");
_Static_assert(sizeof(MachO_BuildVersionCommand) == 24,
               "MachO_BuildVersionCommand must be 24 bytes");
_Static_assert(sizeof(MachO_Nlist64) == 16,
               "MachO_Nlist64 must be 16 bytes");
_Static_assert(sizeof(MachO_RelocationInfo) == 8,
               "MachO_RelocationInfo must be 8 bytes");

#endif
