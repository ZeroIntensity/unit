#ifndef UNIT_SIZE_MAP_H
#define UNIT_SIZE_MAP_H

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

typedef struct {
    UNIT_Size key;
    UNIT_Size value;
    int8_t is_populated;
} _UNIT_SizeMapPair;

// Hash map of always size-to-size pairs.
typedef struct {
    UNIT_Context *context;
    UNIT_Size len;
    UNIT_Size capacity;
    _UNIT_SizeMapPair *items;
} _UNIT_SizeMap;

UNIT_Status
_UNIT_SizeMap_Init(_UNIT_SizeMap *size_map, UNIT_Context *ctx,
                   UNIT_Size inital_capacity);

static inline _UNIT_SizeMap *
_UNIT_SizeMap_New(UNIT_Context *context, UNIT_Size inital_capacity)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_SizeMap, context, inital_capacity)
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_SizeMap)

UNIT_Status
_UNIT_SizeMap_Set(_UNIT_SizeMap *size_map, UNIT_Size key, UNIT_Size value);

UNIT_Status
_UNIT_SizeMap_Get(const _UNIT_SizeMap *size_map, UNIT_Size key, UNIT_Size *value);

static inline UNIT_Size
_UNIT_SizeMap_GET(const _UNIT_SizeMap *size_map, UNIT_Size key)
{
    UNIT_Size value;
    UNIT_Status status = _UNIT_SizeMap_Get(size_map, key, &value);
    assert(!UNIT_FAILED(status));
    return value;
}

void
_UNIT_SizeMap_Remove(_UNIT_SizeMap *size_map, UNIT_Size key);

#define _UNIT_SizeMap_ITER(size_map, key_name, value_name) \
    for (UNIT_Size _sm_index = 0; _sm_index < (size_map)->capacity; ++_sm_index) {          \
        if (!(size_map)->items[_sm_index].is_populated) {                                   \
            continue;                                                                       \
        }                                                                                   \
        UNIT_Size key_name = (size_map)->items[_sm_index].key;                              \
        UNIT_Size value_name = (size_map)->items[_sm_index].value;                          \
        {

#define _UNIT_SizeMap_END_ITER() }}

#endif
