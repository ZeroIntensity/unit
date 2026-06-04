#include <unit/internal/allocation.h>
#include <unit/internal/vector.h>

UNIT_Status
_UNIT_Vector_Init(_UNIT_Vector *vector, UNIT_Context *context,
                  UNIT_Size initial_capacity, UNIT_Destructor dealloc)
{
    assert(vector != NULL);
    vector->context = context;
    // Even if the initial capacity is zero, we allocate a single slot.
    if (initial_capacity == 0) {
        initial_capacity = 1;
    }
    vector->items = _UNIT_Calloc(context,
                                 sizeof(void *), initial_capacity);
    if (vector->items == NULL) {
        return UNIT_FAIL;
    }
    vector->length = 0;
    vector->capacity = initial_capacity;
    vector->dealloc = dealloc;
    return UNIT_OK;
}

void
_UNIT_Vector_Clear(_UNIT_Vector *vector)
{
    assert(vector != NULL);
    if (vector->dealloc != NULL) {
        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(vector); ++index) {
            void *data = vector->items[index];
            vector->dealloc(vector->context, data);
        }
    }
    _UNIT_Dealloc(vector->context, vector->items);
}

UNIT_Status
_UNIT_Vector_Append(_UNIT_Vector *vector, void *item)
{
    assert(vector != NULL);
    if (vector->length == vector->capacity) {
        vector->capacity *= 3;
        void **new_items = _UNIT_Realloc(vector->context, vector->items,
                                         sizeof(void *) * vector->capacity);
        if (new_items == NULL) {
            --vector->length;
            if (vector->dealloc != NULL) {
                vector->dealloc(vector->context, item);
            }
            return UNIT_FAIL;
        }
        vector->items = new_items;
    }
    vector->items[vector->length++] = item;
    return UNIT_OK;
}

void *
_UNIT_Vector_Pop(_UNIT_Vector *vector)
{
    assert(vector != NULL);
    void *result = vector->items[--vector->length];
    vector->items[vector->length] = NULL;
    return result;
}
