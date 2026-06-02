#ifndef _UNIT_ALLOCATION_H
#define _UNIT_ALLOCATION_H

#include <unit/base.h>
#include <unit/context.h>

// Enable this for debugging with valgrind
#if 0
#include <stdlib.h>
#include <string.h>

#define _UNIT_Alloc(ctx, amount) malloc(amount)
static inline void
_UNIT_Dealloc(UNIT_Context *ctx, void *ptr)
{
    free(ptr);
}
#define _UNIT_Realloc(ctx, ptr, newsize) realloc(ptr, newsize)
#define _UNIT_Calloc(ctx, count, size) calloc(count, size)
#define _UNIT_StrDup(ctx, src) strdup(src)
#define _UNIT_ClearFreelists(ctx)

#else
void *
_UNIT_Alloc(UNIT_Context *context, UNIT_Size size);

void
_UNIT_Dealloc(UNIT_Context *context, void *ptr);

void *
_UNIT_Calloc(UNIT_Context *context, UNIT_Size count, UNIT_Size size);

void *
_UNIT_Realloc(UNIT_Context *context, void *ptr, UNIT_Size new_size);

char *
_UNIT_StrDup(UNIT_Context *context, const char *src);

void
_UNIT_ClearFreelists(UNIT_Context *context);
#endif

#endif
