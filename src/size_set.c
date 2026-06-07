// Based on the _UNIT_SizeMap implementation
#include <unit/internal/allocation.h>
#include <unit/internal/size_set.h>

UNIT_Status
_UNIT_SizeSet_Init(_UNIT_SizeSet *size_set, UNIT_Context *context,
                   UNIT_Size inital_capacity)
{
    size_set->context = context;
    size_set->len = 0;
    size_set->capacity = inital_capacity;
    size_set->items = _UNIT_Calloc(context, inital_capacity, sizeof(_UNIT_SizeSetItem));
    if (size_set->items == NULL) {
        return _UNIT_FAIL;
    }
    return _UNIT_OK;
}

static int8_t
set_size_set_entry(
    _UNIT_SizeSet *size_set,
    UNIT_Size value
) {
    assert(size_set != NULL);
    UNIT_Size index = value & (size_set->capacity - 1);
    UNIT_Size current_index = index;

    do {
        _UNIT_SizeSetItem *item = &size_set->items[current_index];
        if (!item->is_populated) {
            item->value = value;
            item->is_populated = 1;
            return 1;
        }
        if (item->value == value) {
            assert(item->is_populated == 1);
            return 0;
        }
        current_index++;
        if (current_index == size_set->capacity) {
            current_index = 0;
        }
    } while (current_index != index);

    _UNIT_Unreachable();
}

static UNIT_Status
expand(_UNIT_SizeSet *size_set) {
    assert(size_set != NULL);
    // TODO: Check for overflow
    UNIT_Size new_capacity = size_set->capacity * 2;
    _UNIT_SizeSetItem *new_items = _UNIT_Calloc(size_set->context,
                                                new_capacity, sizeof(_UNIT_SizeSetItem));
    if (new_items == NULL) {
        return _UNIT_FAIL;
    }

    _UNIT_SizeSetItem *old_items = size_set->items;
    UNIT_Size old_capacity = size_set->capacity;
    size_set->items = new_items;
    size_set->capacity = new_capacity;

    for (UNIT_Size index = 0; index < old_capacity; ++index) {
        _UNIT_SizeSetItem *item = &old_items[index];
        if (item->is_populated) {
            set_size_set_entry(size_set, item->value);
        }
    }
    _UNIT_Dealloc(size_set->context, old_items);

    return _UNIT_OK;
}

UNIT_Status
_UNIT_SizeSet_Add(_UNIT_SizeSet *size_set, UNIT_Size value)
{
    assert(size_set != NULL);

    // 75% load factor
    if (size_set->len * 4 >= size_set->capacity * 3) {
        if (UNIT_FAILED(expand(size_set))) {
            return _UNIT_FAIL;
        }
    }

    if (set_size_set_entry(size_set, value) == 1) {
        // A new item was added
        ++size_set->len;
    }

    return _UNIT_OK;
}

int8_t
_UNIT_SizeSet_Contains(const _UNIT_SizeSet *size_set, UNIT_Size value)
{
    assert(size_set != NULL);
    UNIT_Size index = (UNIT_Size)(value & (uint64_t)(size_set->capacity - 1));
    UNIT_Size current_index = index;

    do {
        _UNIT_SizeSetItem *item = &size_set->items[current_index];
        if (!item->is_populated) {
            return 0;
        }
        if (item->value == value) {
            return 1;
        }
        current_index++;
        if (current_index == size_set->capacity) {
            current_index = 0;
        }
    } while (current_index != index);

    _UNIT_Unreachable();
}

void
_UNIT_SizeSet_Clear(_UNIT_SizeSet *size_set)
{
    assert(size_set != NULL);
    _UNIT_Dealloc(size_set->context, size_set->items);
}

void
_UNIT_SizeSet_Remove(_UNIT_SizeSet *size_set, UNIT_Size value)
{
    assert(size_set != NULL);
    UNIT_Size index = (UNIT_Size)(value & (uint64_t)(size_set->capacity - 1));
    UNIT_Size current = index;

    do {
        _UNIT_SizeSetItem *item = &size_set->items[current];
        if (!item->is_populated) {
            return;
        }
        if (item->value == value) {
            break;
        }
        current++;
        if (current == size_set->capacity) {
            current = 0;
        }
    } while (current != index);

    size_set->items[current].is_populated = 0;
    size_set->len--;

    UNIT_Size next = current + 1;
    if (next == size_set->capacity) {
        next = 0;
    }

    while (size_set->items[next].is_populated) {
        _UNIT_SizeSetItem item = size_set->items[next];
        size_set->items[next].is_populated = 0;
        set_size_set_entry(size_set, item.value);
        next++;
        if (next == size_set->capacity) {
            next = 0;
        }
    }
}
