#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <unit/unit.h>

#include <stdio.h>
#include <stdlib.h>

#define ASSERT(condition)                                           \
    do {                                                            \
        if (!(condition)) {                                         \
            fprintf(stderr, "  %s FAIL: %s:%d: %s\n",               \
                    __func__, __FILE__, __LINE__, #condition);      \
            abort();                                                \
        }                                                           \
    } while (0)

#define ASSERT_EQ(a, b)                                                     \
    do {                                                                    \
        if ((a) != (b)) {                                                   \
            fprintf(stderr, "  %s FAIL: %s:%d: %s != %s (%ld != %ld)\n",    \
                    __func__, __FILE__, __LINE__, #a, #b,                   \
                    (long)(a), (long)(b));                                  \
            abort();                                                        \
        }                                                                   \
    } while (0)

#define ASSERT_OK(ctx, call)                                                \
    do {                                                                    \
        if (UNIT_FAILED(call)) {                                            \
            fprintf(stderr, "  %s FAIL: %s:%d: %s was not successful\n",    \
                    __func__, __FILE__, __LINE__, #call);                   \
            UNIT_PrintError(ctx, stderr);                                   \
            abort();                                                        \
        }                                                                   \
    } while (0)

#define RUN_TEST(fn, ...)                                           \
    do {                                                            \
        printf("  %s...", #fn);                                     \
        fn(__VA_ARGS__);                                            \
        printf(" ok\n");                                            \
    } while (0)

#endif
