#ifndef UNIT_MAP_H
#define UNIT_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *key;
    void *value;
    uint64_t hash;
} _UNIT_MapPair;

typedef UNIT_Size (*_UNIT_Map_Hash)(const void *);
typedef bool (*_UNIT_Map_Compare)(const void *, const void *);

typedef struct {
    UNIT_Context *context;
    UNIT_Size len;
    UNIT_Size capacity;
    _UNIT_MapPair *items;
    _UNIT_Map_Compare compare_key;
    _UNIT_Map_Hash hash_key;
    UNIT_Destructor dealloc_key;
    UNIT_Destructor dealloc_value;
} _UNIT_Map;

UNIT_Size
_UNIT_Map_HashString(const void *key);

UNIT_Size
_UNIT_Map_HashDirect(const void *key);

bool
_UNIT_Map_CompareEqual(const void *a, const void *b);

bool
_UNIT_Map_CompareString(const void *a, const void *b);

UNIT_Status
_UNIT_Map_Init(_UNIT_Map *map, UNIT_Context *context,
               UNIT_Size inital_capacity, _UNIT_Map_Compare compare_key,
               _UNIT_Map_Hash hash_key, UNIT_Destructor dealloc_key,
               UNIT_Destructor dealloc_value);

static inline _UNIT_Map *
_UNIT_Map_New(UNIT_Context *context,
              UNIT_Size inital_capacity, _UNIT_Map_Compare compare_key,
              _UNIT_Map_Hash hash_key, UNIT_Destructor dealloc_key,
              UNIT_Destructor dealloc_value)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_Map, context, inital_capacity, compare_key,
                             hash_key, dealloc_key, dealloc_value);
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_Map);

void *
_UNIT_Map_Get(const _UNIT_Map *map, const void *key);

UNIT_Status
_UNIT_Map_Set(_UNIT_Map *map, void *key, void *value);


#define _UNIT_Map_ITER(map, key_name, value_name)                           \
    for (UNIT_Size _m_index = 0; _m_index < (map)->capacity; ++_m_index) {  \
        void *key_name = (map)->items[_m_index].key;                        \
        void *value_name = (map)->items[_m_index].value;                    \
        if (key_name == NULL) {                                             \
            continue;                                                       \
        }                                                                   \
        {

#define _UNIT_Map_END_ITER() }}

#ifdef __cplusplus
}
#endif

#endif
