#ifndef UNIT_SIZE_SET_H
#define UNIT_SIZE_SET_H

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

typedef struct {
    UNIT_Size value;
    int8_t is_populated;
} _UNIT_SizeSetItem;

// Hash set of UNIT_Size values.
typedef struct {
    UNIT_Context *context;
    UNIT_Size len;
    UNIT_Size capacity;
    _UNIT_SizeSetItem *items;
} _UNIT_SizeSet;

UNIT_Status
_UNIT_SizeSet_Init(_UNIT_SizeSet *size_set, UNIT_Context *context,
                   UNIT_Size inital_capacity);

static inline _UNIT_SizeSet *
_UNIT_SizeSet_New(UNIT_Context *context, UNIT_Size inital_capacity)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_SizeSet, context, inital_capacity);
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_SizeSet)

int8_t
_UNIT_SizeSet_Contains(const _UNIT_SizeSet *size_set, UNIT_Size value);

UNIT_Status
_UNIT_SizeSet_Add(_UNIT_SizeSet *size_set, UNIT_Size value);

#define _UNIT_SizeSet_ITER(size_set, value_name) \
    for (UNIT_Size _ss_index = 0; _ss_index < (size_set)->capacity; ++_ss_index) {          \
        if (!(size_set)->items[_ss_index].is_populated) {                                   \
            continue;                                                                       \
        }                                                                                   \
        UNIT_Size value_name = (size_set)->items[_ss_index].value;                          \
        {

#define _UNIT_SizeSet_END_ITER() }}

#endif
