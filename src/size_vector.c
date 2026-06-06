#include <assert.h>

#include <unit/internal/allocation.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/structure.h>

UNIT_Status
_UNIT_SizeVector_Init(_UNIT_SizeVector *size_vector,
                      UNIT_Context *context,
                      UNIT_Size initial_capacity)
{
    assert(size_vector != NULL);
    size_vector->items = _UNIT_Calloc(context,
                                      sizeof(UNIT_Size), initial_capacity);
    if (size_vector->items == NULL) {
        return _UNIT_FAIL;
    }
    size_vector->length = 0;
    size_vector->capacity = initial_capacity;
    return _UNIT_OK;
}

void
_UNIT_SizeVector_Clear(_UNIT_SizeVector *size_vector)
{
    assert(size_vector != NULL);
    _UNIT_Dealloc(size_vector->context, size_vector->items);
}

UNIT_Status
_UNIT_SizeVector_Append(_UNIT_SizeVector *size_vector, UNIT_Size item)
{
    assert(size_vector != NULL);
    if (size_vector->length == size_vector->capacity) {
        size_vector->capacity *= 3;
        void **new_items = _UNIT_Realloc(size_vector->context, size_vector->items,
                                         sizeof(UNIT_Size) * size_vector->capacity);
        if (new_items == NULL) {
            --size_vector->length;
            return _UNIT_FAIL;
        }
    }
    size_vector->items[size_vector->length++] = item;
    return _UNIT_OK;
}
