#ifndef _UNIT_SET_H
#define _UNIT_SET_H

#include <unit/base.h>
#include <unit/internal/allocation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const void *value;
    int8_t is_populated;
} _UNIT_SetItem;

typedef struct {
    UNIT_Context *context;
    _UNIT_SetItem *items;
    UNIT_Size capacity;
    UNIT_Size len;
} _UNIT_Set;

UNIT_Status
_UNIT_Set_Init(_UNIT_Set *set,
               UNIT_Context *context,
               UNIT_Size initial_capacity);

void
_UNIT_Set_Clear(_UNIT_Set *set);

UNIT_Status
_UNIT_Set_Add(_UNIT_Set *set, const void *value);

int8_t
_UNIT_Set_Contains(_UNIT_Set *set, const void *value);

void
_UNIT_Set_Remove(_UNIT_Set *set, const void *value);

#ifdef __cplusplus
}
#endif

#endif
