#include <unit/internal/set.h>

static UNIT_Size
hash_ptr(const void *ptr)
{
    UNIT_Size val = (UNIT_Size)(uintptr_t)ptr;
    val ^= val >> 16;
    val *= 0x45d9f3b;
    val ^= val >> 16;
    return val;
}

UNIT_Status
_UNIT_Set_Init(_UNIT_Set *set,
               UNIT_Context *context,
               UNIT_Size initial_capacity)
{
    assert(set != NULL);
    assert(context != NULL);
    assert(initial_capacity >= 0);
    if (initial_capacity == 0) {
        initial_capacity = 1;
    }

    set->context = context;
    set->len = 0;
    set->capacity = initial_capacity;
    set->items = _UNIT_Calloc(context,
                              initial_capacity,
                              sizeof(_UNIT_SetItem));
    if (set->items == NULL) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

int8_t
_UNIT_Set_Contains(_UNIT_Set *set,
                   const void *value)
{
    assert(set != NULL);
    UNIT_Size index = hash_ptr(value) & (set->capacity - 1);
    UNIT_Size start = index;
    do {
        _UNIT_SetItem *item = &set->items[index];
        if (!item->is_populated) {
            return 0;
        }
        if (item->value == value) {
            return 1;
        }

        ++index;
        if (index == set->capacity) {
            index = 0;
        }
    } while (index != start);
    return 0;
}

UNIT_Status
_UNIT_Set_Add(_UNIT_Set *set,
              const void *value)
{
    assert(set != NULL);
    if (set->len >= (set->capacity * 3) / 4) {
        UNIT_Size new_capacity = set->capacity * 2;
        _UNIT_SetItem *new_items = _UNIT_Calloc(set->context,
                                                new_capacity,
                                                sizeof(_UNIT_SetItem));
        if (new_items == NULL) {
            return _UNIT_FAIL;
        }
        for (UNIT_Size i = 0; i < set->capacity; ++i) {
            if (set->items[i].is_populated) {
                UNIT_Size idx = hash_ptr(set->items[i].value)
                                & (new_capacity - 1);
                while (new_items[idx].is_populated) {
                    idx++;
                    if (idx == new_capacity) {
                        idx = 0;
                    }
                }
                new_items[idx].value = set->items[i].value;
                new_items[idx].is_populated = 1;
            }
        }
        _UNIT_Dealloc(set->context, set->items);
        set->items = new_items;
        set->capacity = new_capacity;
    }

    UNIT_Size index = hash_ptr(value) & (set->capacity - 1);
    while (set->items[index].is_populated) {
        if (set->items[index].value == value) {
            return _UNIT_OK;
        }

        ++index;
        if (index == set->capacity) {
            index = 0;
        }
    }

    set->items[index].value = value;
    set->items[index].is_populated = 1;
    set->len++;
    return _UNIT_OK;
}

void
_UNIT_Set_Remove(_UNIT_Set *set,
                 const void *value)
{
    assert(set != NULL);
    UNIT_Size index = hash_ptr(value) & (set->capacity - 1);
    UNIT_Size start = index;

    do {
        if (!set->items[index].is_populated) {
            return;
        }

        if (set->items[index].value == value) {
            break;
        }

        ++index;
        if (index == set->capacity) {
            index = 0;
        }
    } while (index != start);

    if (!set->items[index].is_populated) {
        return;
    }

    set->items[index].is_populated = 0;
    --set->len;

    // Rehash subsequent entries
    UNIT_Size next = index + 1;
    if (next == set->capacity) {
        next = 0;
    }
    while (set->items[next].is_populated) {
        _UNIT_SetItem item = set->items[next];
        set->items[next].is_populated = 0;
        set->len--;

        UNIT_Status result = _UNIT_Set_Add(set, item.value);
        (void) result;
        assert(!UNIT_FAILED(result));

        if (++next == set->capacity) {
            next = 0;
        }
    }
}

void
_UNIT_Set_Clear(_UNIT_Set *set)
{
    assert(set != NULL);
    _UNIT_Dealloc(set->context, set->items);
}
