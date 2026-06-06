#include <unit/internal/allocation.h>
#include <unit/internal/size_map.h>

UNIT_Status
_UNIT_SizeMap_Init(_UNIT_SizeMap *size_map, UNIT_Context *context,
                   UNIT_Size inital_capacity)
{
    assert(size_map != NULL);
    assert(context != NULL);
    assert(inital_capacity > 0);
    size_map->context = context;
    size_map->len = 0;
    size_map->capacity = inital_capacity;
    size_map->items = _UNIT_Calloc(context, inital_capacity,
                                   sizeof(_UNIT_SizeMapPair));
    if (size_map->items == NULL) {
        return _UNIT_FAIL;
    }
    return _UNIT_OK;
}

static int8_t
set_size_map_entry(
    _UNIT_SizeMap *size_map,
    UNIT_Size key,
    UNIT_Size value
) {
    assert(size_map != NULL);
    UNIT_Size index = key & (size_map->capacity - 1);
    UNIT_Size current_index = index;

    do {
        _UNIT_SizeMapPair *pair = &size_map->items[current_index];
        if (!pair->is_populated) {
            pair->key = key;
            pair->value = value;
            pair->is_populated = 1;
            return 1;
        }
        if (pair->key == key) {
            pair->value = value;
            return 0;
        }
        current_index++;
        if (current_index == size_map->capacity) {
            current_index = 0;
        }
    } while (current_index != index);

    _UNIT_Unreachable();
}

static UNIT_Status
expand(_UNIT_SizeMap *size_map) {
    // TODO: Check for overflow
    UNIT_Size new_capacity = size_map->capacity * 2;
    _UNIT_SizeMapPair *new_items = _UNIT_Calloc(size_map->context,
                                                new_capacity, sizeof(_UNIT_SizeMapPair));
    if (new_items == NULL) {
        return _UNIT_FAIL;
    }

    _UNIT_SizeMapPair *old_items = size_map->items;
    UNIT_Size old_capacity = size_map->capacity;
    size_map->items = new_items;
    size_map->capacity = new_capacity;

    for (UNIT_Size index = 0; index < old_capacity; ++index) {
        _UNIT_SizeMapPair *item = &old_items[index];
        if (item->is_populated) {
            set_size_map_entry(size_map, item->key, item->value);
        }
    }
    _UNIT_Dealloc(size_map->context, old_items);

    return _UNIT_OK;
}

UNIT_Status
_UNIT_SizeMap_Set(_UNIT_SizeMap *size_map, UNIT_Size key, UNIT_Size value)
{
    assert(size_map != NULL);

    // 75% load factor
    if (size_map->len * 4 >= size_map->capacity * 3) {
        if (UNIT_FAILED(expand(size_map))) {
            return _UNIT_FAIL;
        }
    }


    if (set_size_map_entry(size_map, key, value) == 1) {
        // A new item was added
        ++size_map->len;
    }

    return _UNIT_OK;
}

UNIT_Status
_UNIT_SizeMap_Get(const _UNIT_SizeMap *size_map, UNIT_Size key, UNIT_Size *value)
{
    assert(size_map != NULL);
    UNIT_Size index = (UNIT_Size)(key & (uint64_t)(size_map->capacity - 1));
    UNIT_Size current_index = index;

    do {
        _UNIT_SizeMapPair *pair = &size_map->items[current_index];
        if (!pair->is_populated) {
            return _UNIT_FAIL;
        }
        if (pair->key == key) {
            *value = pair->value;
            return _UNIT_OK;
        }
        current_index++;
        if (current_index == size_map->capacity) {
            current_index = 0;
        }
    } while (current_index != index);

    _UNIT_Unreachable();
}

void
_UNIT_SizeMap_Clear(_UNIT_SizeMap *size_map)
{
    assert(size_map != NULL);
    _UNIT_Dealloc(size_map->context, size_map->items);
}
