#ifndef UNIT_ELF_LOCAL_H
#define UNIT_ELF_LOCAL_H

#include <stdint.h>

// Vendored ELF structures for cross-compilation support.
// These mirror the standard ELF64 definitions, but with readable names.

// ELF identification indices
enum {
    ELF_IDENT_MAGIC_0        = 0,   // EI_MAG0
    ELF_IDENT_MAGIC_1        = 1,   // EI_MAG1
    ELF_IDENT_MAGIC_2        = 2,   // EI_MAG2
    ELF_IDENT_MAGIC_3        = 3,   // EI_MAG3
    ELF_IDENT_CLASS          = 4,   // EI_CLASS
    ELF_IDENT_DATA_ENCODING  = 5,   // EI_DATA
    ELF_IDENT_VERSION        = 6,   // EI_VERSION
    ELF_IDENT_OS_ABI         = 7,   // EI_OSABI
    ELF_IDENT_ABI_VERSION    = 8,   // EI_ABIVERSION
    ELF_IDENT_SIZE           = 16,  // EI_NIDENT
};

// ELF class (32-bit vs 64-bit)
enum {
    ELF_CLASS_NONE = 0,
    ELF_CLASS_32   = 1,  // ELFCLASS32
    ELF_CLASS_64   = 2,  // ELFCLASS64
};

// Data encoding (endianness)
enum {
    ELF_DATA_NONE           = 0,
    ELF_DATA_LITTLE_ENDIAN  = 1,  // ELFDATA2LSB
    ELF_DATA_BIG_ENDIAN     = 2,  // ELFDATA2MSB
};

// ELF version
enum {
    ELF_VERSION_NONE    = 0,
    ELF_VERSION_CURRENT = 1,  // EV_CURRENT
};


// Object file type


enum {
    ELF_TYPE_NONE        = 0,  // ET_NONE
    ELF_TYPE_RELOCATABLE = 1,  // ET_REL
    ELF_TYPE_EXECUTABLE  = 2,  // ET_EXEC
    ELF_TYPE_SHARED      = 3,  // ET_DYN
    ELF_TYPE_CORE        = 4,  // ET_CORE
};

// Machine architecture
enum {
    ELF_MACHINE_NONE    = 0,    // EM_NONE
    ELF_MACHINE_386     = 3,    // EM_386
    ELF_MACHINE_ARM     = 40,   // EM_ARM
    ELF_MACHINE_AMD64  = 62,   // EM_AMD64
    ELF_MACHINE_AARCH64 = 183,  // EM_AARCH64
};

// Section header types
enum {
    ELF_SECTION_TYPE_NULL          = 0,   // SHT_NULL
    ELF_SECTION_TYPE_PROGRAM_DATA  = 1,   // SHT_PROGBITS
    ELF_SECTION_TYPE_SYMBOL_TABLE  = 2,   // SHT_SYMTAB
    ELF_SECTION_TYPE_STRING_TABLE  = 3,   // SHT_STRTAB
    ELF_SECTION_TYPE_RELA          = 4,   // SHT_RELA
    ELF_SECTION_TYPE_HASH          = 5,   // SHT_HASH
    ELF_SECTION_TYPE_DYNAMIC       = 6,   // SHT_DYNAMIC
    ELF_SECTION_TYPE_NOTE          = 7,   // SHT_NOTE
    ELF_SECTION_TYPE_NOBITS        = 8,   // SHT_NOBITS
    ELF_SECTION_TYPE_REL           = 9,   // SHT_REL
};

// Section header flags
enum {
    ELF_SECTION_FLAG_WRITE      = 0x1,  // SHF_WRITE
    ELF_SECTION_FLAG_ALLOC      = 0x2,  // SHF_ALLOC
    ELF_SECTION_FLAG_EXECUTABLE = 0x4,  // SHF_EXECINSTR
    ELF_SECTION_FLAG_INFO_LINK  = 0x40, // SHF_INFO_LINK
};

// Special section indices
enum {
    ELF_SECTION_UNDEFINED = 0,  // SHN_UNDEF
};

// Symbol binding
enum {
    ELF_SYMBOL_BINDING_LOCAL  = 0,   // STB_LOCAL
    ELF_SYMBOL_BINDING_GLOBAL = 1,   // STB_GLOBAL
    ELF_SYMBOL_BINDING_WEAK   = 2,   // STB_WEAK
};

// Symbol type
enum {
    ELF_SYMBOL_TYPE_NONE     = 0,   // STT_NOTYPE
    ELF_SYMBOL_TYPE_OBJECT   = 1,   // STT_OBJECT
    ELF_SYMBOL_TYPE_FUNCTION = 2,   // STT_FUNC
    ELF_SYMBOL_TYPE_SECTION  = 3,   // STT_SECTION
    ELF_SYMBOL_TYPE_FILE     = 4,   // STT_FILE
};


// Symbol info packing
//
// The st_info field of a symbol packs binding and type into
// one byte: (binding << 4) | (type & 0xf)
#define ELF_SYMBOL_INFO(binding, type) \
        (((binding) << 4) | ((type) & 0xf))


// Relocation types (AMD64)
enum {
    ELF_RELOCATION_AMD64_NONE   = 0,   // R_AMD64_NONE
    ELF_RELOCATION_AMD64_64     = 1,   // R_AMD64_64
    ELF_RELOCATION_AMD64_PC32   = 2,   // R_AMD64_PC32
    ELF_RELOCATION_AMD64_PLT32  = 4,   // R_AMD64_PLT32
};


// Relocation info packing
//
// The r_info field packs a symbol index and relocation type
// into one 64-bit value: (symbol_index << 32) | type
#define ELF_RELOCATION_INFO(symbol_index, type) \
        (((uint64_t)(symbol_index) << 32) | (uint32_t)(type))

// The ELF file header. Appears at the very start of every ELF file.
typedef struct {
    uint8_t identification[ELF_IDENT_SIZE];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry_point;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_entry_size;
    uint16_t program_header_count;
    uint16_t section_header_entry_size;
    uint16_t section_header_count;
    uint16_t section_name_string_table_index;
} ELF_Header;

// A section header. The section header table is an array of these.
// Each one describes a section in the file: where its data lives,
// how big it is, and what kind of content it holds.
typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t Address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t alignment;
    uint64_t entry_size;
} ELF_SectionHeader;

// A symbol table entry. Symbols represent functions, variables,
// or external references. The name field is an offset into the
// associated string table.
typedef struct {
    uint32_t name;
    uint8_t info;
    uint8_t other;
    uint16_t section_index;
    uint64_t value;
    uint64_t size;
} ELF_Symbol;

// A relocation entry with an explicit add. Tells the linker
// "at this offset in the section, patch in the Address of this
// symbol, adjusted by this add."
typedef struct {
    uint64_t offset;
    uint64_t info;
    int64_t add;
} ELF_RelocationAddend;

_Static_assert(sizeof(ELF_Header) == 64,
               "ELF_Header must be 64 bytes");
_Static_assert(sizeof(ELF_SectionHeader) == 64,
               "ELF_SectionHeader must be 64 bytes");
_Static_assert(sizeof(ELF_Symbol) == 24,
               "ELF_Symbol must be 24 bytes");
_Static_assert(sizeof(ELF_RelocationAddend) == 24,
               "ELF_RelocationAddend must be 24 bytes");

#endif
