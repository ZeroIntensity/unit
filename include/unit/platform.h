#ifndef UNIT_PLATFORM_H
#define UNIT_PLATFORM_H

#include <unit/base.h>
#include <unit/context.h>

typedef uint32_t UNIT_Platform;

#define _UNIT_ARCH_BITS 0
#define _UNIT_ABI_BITS  8

typedef enum {
    UNIT_ARCH_AMD64 = (1 << _UNIT_ARCH_BITS),
    UNIT_ARCH_AARCH64 = (2 << _UNIT_ARCH_BITS)
} UNIT_Architecture;

typedef enum {
    UNIT_ABI_SYSTEMV = (1 << _UNIT_ABI_BITS),
    UNIT_ABI_WIN64 = (2 << _UNIT_ABI_BITS),
    UNIT_ABI_APPLE = (3 << _UNIT_ABI_BITS)
} UNIT_ABI;

#define _UNIT_ARCH_MASK 0xFF
#define _UNIT_ABI_MASK (0xFF << _UNIT_ABI_BITS)

static inline UNIT_ABI
UNIT_Platform_GET_ABI(UNIT_Platform platform)
{
    assert(platform >= 0);
    UNIT_ABI abi = platform & _UNIT_ABI_MASK;
    assert(abi == UNIT_ABI_SYSTEMV
           || abi == UNIT_ABI_WIN64
           || abi == UNIT_ABI_APPLE);
    return abi;
}

static inline UNIT_Architecture
UNIT_Platform_GET_ARCH(UNIT_Platform platform)
{
    assert(platform >= 0);
    UNIT_Architecture arch = platform & _UNIT_ARCH_MASK;
    assert(arch == UNIT_ARCH_AMD64
           || arch == UNIT_ARCH_AARCH64);
    return arch;
}

UNIT_Status
UNIT_GetCurrentPlatform(UNIT_Context *context, UNIT_Platform *out);

#endif
