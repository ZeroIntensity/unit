#ifndef UNIT_CONTEXT_H
#define UNIT_CONTEXT_H

#include <unit/base.h>

// These have to be defined in context.h instead of errors.h to avoid circular
// dependency issues.
typedef enum {
    UNIT_ERROR_NONE,
    UNIT_ERROR_NO_MEMORY,
    UNIT_ERROR_INVALID_USAGE,
    UNIT_ERROR_OS_FAILURE,
    UNIT_ERROR_UNSUPPORTED_PLATFORM
} UNIT_ErrorCode;

#define SIZE_CLASS(size) \
    _UNIT_Freelist *freelist_ ##size

typedef struct _UNIT_Freelist _UNIT_Freelist;

struct _UNIT_Freelist {
    _UNIT_Freelist *next;
};

typedef struct {
    struct {
        UNIT_ErrorCode code;
        char message[256];
    } error;
    struct {
        SIZE_CLASS(8);
        SIZE_CLASS(16);
        SIZE_CLASS(32);
        SIZE_CLASS(64);
        SIZE_CLASS(128);
        SIZE_CLASS(256);
    } allocator;
} _UNIT_ContextInternal;

#undef SIZE_CLASS

typedef struct {
    _UNIT_ContextInternal _internal;
} UNIT_Context;

UNIT_Status
UNIT_Context_Init(UNIT_Context *context);

UNIT_Context *
UNIT_Context_New(void);

void
UNIT_Context_Clear(UNIT_Context *context);

void
UNIT_Context_Free(UNIT_Context *context);

typedef void (*UNIT_Destructor)(UNIT_Context *ctx, void *ptr);

#endif
