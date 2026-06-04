#ifndef _UNIT_VECTOR_H
#define _UNIT_VECTOR_H

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

typedef struct {
    UNIT_Context *context;
    void **items;
    UNIT_Size length;
    UNIT_Size capacity;
    UNIT_Destructor dealloc;
} _UNIT_Vector;

UNIT_Status
_UNIT_Vector_Init(_UNIT_Vector *vector, UNIT_Context *context,
                  UNIT_Size initial_capacity, UNIT_Destructor dealloc);

static inline _UNIT_Vector *
_UNIT_Vector_New(UNIT_Context *context, UNIT_Size initial_capacity,
                 UNIT_Destructor dealloc)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_Vector, context, initial_capacity, dealloc);
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_Vector);

UNIT_Status
_UNIT_Vector_Append(_UNIT_Vector *vector, void *item);

void *
_UNIT_Vector_Pop(_UNIT_Vector *vector);

static inline UNIT_Size
_UNIT_Vector_SIZE(const _UNIT_Vector *vector)
{
    return vector->length;
}

static inline void *
_UNIT_Vector_GET(const _UNIT_Vector *vector, UNIT_Size index)
{
    assert(vector != NULL);
    assert(index >= 0);
    assert(index < _UNIT_Vector_SIZE(vector));
    assert(vector->items[index] != NULL);
    return vector->items[index];
}

/* Like _UNIT_Vector_Append(), but does not attempt to resize.
 * Only use if you're certain that the vector is big enough. */
static inline void
_UNIT_Vector_APPEND(_UNIT_Vector *vector, void *item)
{
    assert(vector != NULL);
    assert(vector->length < vector->capacity);
    vector->items[vector->length++] = item;
}

#endif
