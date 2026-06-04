#ifndef _UNIT_SIZE_VECTOR_H
#define _UNIT_SIZE_VECTOR_H

#include <unit/base.h>

#include <unit/internal/structure.h>

typedef struct {
    UNIT_Context *context;
    UNIT_Size *items;
    UNIT_Size length;
    UNIT_Size capacity;
} _UNIT_SizeVector;

UNIT_Status
_UNIT_SizeVector_Init(_UNIT_SizeVector *size_vector, UNIT_Context *ctx,
                      UNIT_Size initial_capacity);

static inline _UNIT_SizeVector *
_UNIT_SizeVector_New(UNIT_Context *context, UNIT_Size initial_capacity)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_SizeVector, context, initial_capacity);
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_SizeVector)

UNIT_Status
_UNIT_SizeVector_Append(_UNIT_SizeVector *size_vector, UNIT_Size item);

static inline UNIT_Size
_UNIT_SizeVector_SIZE(const _UNIT_SizeVector *size_vector)
{
    return size_vector->length;
}

static inline UNIT_Size
_UNIT_SizeVector_GET(const _UNIT_SizeVector *size_vector, UNIT_Size index)
{
    assert(size_vector != NULL);
    assert(index >= 0);
    assert(index < _UNIT_SizeVector_SIZE(size_vector));
    return size_vector->items[index];
}

static inline void
_UNIT_SizeVector_SET(_UNIT_SizeVector *size_vector, UNIT_Size index,
                     UNIT_Size item)
{
    assert(size_vector != NULL);
    assert(index >= 0);
    assert(index < _UNIT_SizeVector_SIZE(size_vector));
    size_vector->items[index] = item;
}

static inline void
_UNIT_SizeVector_APPEND(_UNIT_SizeVector *size_vector, UNIT_Size item)
{
    assert(size_vector != NULL);
    assert(size_vector->length < size_vector->capacity);
    size_vector->items[size_vector->length++] = item;
}

#endif
