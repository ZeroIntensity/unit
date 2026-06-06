#ifndef UNIT_BASE_H
#define UNIT_BASE_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef ptrdiff_t UNIT_Size;
#define UNIT_SIZE_MAX PTRDIFF_MAX

typedef struct {
    int8_t _status;
} UNIT_Status;

#define _UNIT_OK ((UNIT_Status){ ._status = 0 })
#define _UNIT_FAIL ((UNIT_Status){ ._status = -1 })
#define UNIT_FAILED(status) (((UNIT_Status)(status))._status == -1)

#if defined(__GNUC__) || defined(__clang__)
    #define _UNIT_Unreachable()                                         \
        do {                                                            \
            assert(0 && "unreachable");                                 \
            __builtin_unreachable();                                    \
        } while (0)
#elif defined(_MSC_VER)
    #define _UNIT_Unreachable()                                         \
        do {                                                            \
            assert(0 && "unreachable");                                 \
            __assume(0);                                                \
        } while (0)
#else
    #define _UNIT_Unreachable()                                         \
        do {                                                            \
            assert(0 && "unreachable");                                 \
            abort();                                                    \
        } while (0)
#endif

#endif
