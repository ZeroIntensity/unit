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
    // Casting is necessary for C++ compatibility
    UNIT_ABI abi = (UNIT_ABI)(platform & _UNIT_ABI_MASK);
    assert(abi == UNIT_ABI_SYSTEMV
           || abi == UNIT_ABI_WIN64
           || abi == UNIT_ABI_APPLE);
    return abi;
}

static inline UNIT_Architecture
UNIT_Platform_GET_ARCH(UNIT_Platform platform)
{
    assert(platform >= 0);
    UNIT_Architecture arch = (UNIT_Architecture)(platform & _UNIT_ARCH_MASK);
    assert(arch == UNIT_ARCH_AMD64
           || arch == UNIT_ARCH_AARCH64);
    return arch;
}

#if defined(__x86_64__) || defined(_M_X64)
    #if defined(_WIN32)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AMD64 | UNIT_ABI_WIN64)
    #elif defined(__APPLE__)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV)
    #elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
          || defined(__NetBSD__) || defined(__DragonFly__) || defined(__sun)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV)
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #if defined(_WIN32)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AARCH64 | UNIT_ABI_WIN64)
    #elif defined(__APPLE__)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AARCH64 | UNIT_ABI_APPLE)
    #elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
          || defined(__NetBSD__) || defined(__DragonFly__) || defined(__sun)
        #define UNIT_HOST_PLATFORM (UNIT_ARCH_AARCH64 | UNIT_ABI_SYSTEMV)
    #endif
#endif

#endif
