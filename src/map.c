#include <stdint.h>
#include <string.h>

#include <unit/internal/allocation.h>
#include <unit/internal/map.h>

#if (defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 2)))
#  define UNLIKELY(value) __builtin_expect((value), 0)
#  define LIKELY(value) __builtin_expect((value), 1)
#else
#  define UNLIKELY(value) (value)
#  define LIKELY(value) (value)
#endif

// wyhash final3 - https://github.com/wangyi-fudan/wyhash
// Public domain
static inline uint64_t _wyrot(uint64_t x) { return (x >> 32) | (x << 32); }
static inline void _wymix(uint64_t *a, uint64_t *b) {
    *a ^= *b;
    __uint128_t r = (__uint128_t)(*a) * (*b);
    *a = (uint64_t)r;
    *b = (uint64_t)(r >> 64);
}
static inline uint64_t _wyr8(const uint8_t *p) {
    uint64_t v; memcpy(&v, p, 8); return v;
}
static inline uint64_t _wyr4(const uint8_t *p) {
    uint32_t v; memcpy(&v, p, 4); return v;
}
static inline uint64_t _wyr3(const uint8_t *p, size_t k) {
    return (((uint64_t)p[0]) << 16) | (((uint64_t)p[k >> 1]) << 8) | p[k - 1];
}

#define WY_SECRET0 0xa0761d6478bd642full
#define WY_SECRET1 0xe7037ed1a0b428dbull
#define WY_SECRET2 0x8ebc6af09c88c6e3ull
#define WY_SECRET3 0x589965cc75374cc3ull

UNIT_Size
_UNIT_Map_HashString(const void *key)
{
    const uint8_t *p = (const uint8_t *)key;
    uint64_t seed = WY_SECRET0;
    uint64_t a, b;
    uint64_t len = (uint64_t)strlen(key);

    if (LIKELY(len <= 16)) {
        if (LIKELY(len >= 4)) {
            a = (_wyr4(p) << 32) | _wyr4(p + ((len >> 3) << 2));
            b = (_wyr4(p + len - 4) << 32) | _wyr4(p + len - 4 - ((len >> 3) << 2));
        } else if (LIKELY(len > 0)) {
            a = _wyr3(p, len);
            b = 0;
        } else {
            a = b = 0;
        }
    } else {
        size_t i = len;
        if (UNLIKELY(i > 48)) {
            uint64_t see1 = seed, see2 = seed;
            do {
                uint64_t r0 = _wyr8(p),      r1 = _wyr8(p + 8);
                uint64_t r2 = _wyr8(p + 16), r3 = _wyr8(p + 24);
                uint64_t r4 = _wyr8(p + 32), r5 = _wyr8(p + 40);
                seed ^= WY_SECRET0; see1 ^= WY_SECRET1; see2 ^= WY_SECRET2;
                _wymix(&r0, &r1);  seed ^= r0 ^ r1;
                _wymix(&r2, &r3);  see1 ^= r2 ^ r3;
                _wymix(&r4, &r5);  see2 ^= r4 ^ r5;
                p += 48; i -= 48;
            } while (LIKELY(i > 48));
            seed ^= see1 ^ see2;
        }
        while (UNLIKELY(i > 16)) {
            uint64_t r0 = _wyr8(p), r1 = _wyr8(p + 8);
            seed ^= WY_SECRET0;
            _wymix(&r0, &r1);
            seed ^= r0 ^ r1;
            p += 16; i -= 16;
        }
        a = _wyr8(p + i - 16);
        b = _wyr8(p + i - 8);
    }

    a ^= WY_SECRET1;
    b ^= seed;
    _wymix(&a, &b);
    a ^= WY_SECRET0 ^ len;
    b ^= WY_SECRET1;
    _wymix(&a, &b);
    return a ^ b;
}

UNIT_Size
_UNIT_Map_HashDirect(const void *key)
{
    return (UNIT_Size)key;
}

bool
_UNIT_Map_CompareEqual(const void *a, const void *b)
{
    return a == b;
}

bool
_UNIT_Map_CompareString(const void *a, const void *b)
{
    return !strcmp(a, b);
}

UNIT_Status
_UNIT_Map_Init(_UNIT_Map *map, UNIT_Context *context,
               UNIT_Size inital_capacity, _UNIT_Map_Compare compare_key,
               _UNIT_Map_Hash hash_key, UNIT_Destructor dealloc_key,
               UNIT_Destructor dealloc_value)
{
    map->context = context;
    map->len = 0;
    map->capacity = inital_capacity;
    map->dealloc_key = dealloc_key;
    map->dealloc_value = dealloc_value;
    map->compare_key = compare_key;
    map->hash_key = hash_key;
    map->items = _UNIT_Calloc(context, inital_capacity, sizeof(_UNIT_MapPair));
    if (UNLIKELY(map->items == NULL)) {
        return _UNIT_FAIL;
    }
    return _UNIT_OK;
}

static int8_t
set_entry(
    _UNIT_Map *map,
    _UNIT_MapPair *items,
    UNIT_Size new_capacity,
    void *key,
    void *value,
    uint64_t hash
) {
    assert(map != NULL);
    assert(items != NULL);
    assert(key != NULL);
    UNIT_Size index = (UNIT_Size)(hash & (uint64_t)(new_capacity - 1));
    UNIT_Size start = index;

    do {
        _UNIT_MapPair *pair = &items[index];

        if (pair->key == NULL) {
            pair->key = key;
            pair->value = value;
            pair->hash = hash;
            return 1;
        }

        if (pair->hash == hash
            && map->compare_key(key, pair->key)) {
            if (map->dealloc_key != NULL) {
                map->dealloc_key(map->context, key);
            }
            if (map->dealloc_value != NULL) {
                map->dealloc_value(map->context, pair->value);
            }
            pair->value = value;
            return 0;
        }

        index++;
        if (index == new_capacity) {
            index = 0;
        }
    } while (index != start);

    _UNIT_Unreachable();
}

static UNIT_Status
expand(_UNIT_Map *map) {
    assert(map != NULL);
    UNIT_Size new_capacity = map->capacity * 2;
    _UNIT_MapPair *new_items = _UNIT_Calloc(map->context, new_capacity,
                                            sizeof(_UNIT_MapPair));
    if (UNLIKELY(new_items == NULL)) {
        return _UNIT_FAIL;
    }

    for (UNIT_Size index = 0; index < map->capacity; index++) {
        _UNIT_MapPair *item = &map->items[index];
        if (item->key != NULL) {
            set_entry(
                map,
                new_items,
                new_capacity,
                item->key,
                item->value,
                item->hash
            );
        }
    }

    _UNIT_Dealloc(map->context, map->items);
    map->items = new_items;
    map->capacity = new_capacity;
    return _UNIT_OK;
}

void *
_UNIT_Map_Get(const _UNIT_Map *map, const void *key)
{
    assert(map != NULL);
    assert(key != NULL);
    uint64_t hash = map->hash_key(key);
    UNIT_Size index = (UNIT_Size)(hash & (uint64_t)(map->capacity - 1));
    UNIT_Size start = index;

    do {
        if (map->items[index].key == NULL) {
            return NULL;
        }
        _UNIT_MapPair *pair = &map->items[index];
        if (pair->hash == hash
            && map->compare_key(key, pair->key)) {
            return pair->value;
        }
        index++;
        if (index == map->capacity) {
            index = 0;
        }
    } while (index != start);

    return NULL;
}

UNIT_Status
_UNIT_Map_Set(_UNIT_Map *map, void *key, void *value) {
    assert(map != NULL);
    assert(key != NULL);

    if (map->len >= (map->capacity * 3) / 4) {
        if (UNIT_FAILED(expand(map))) {
            return _UNIT_FAIL;
        }
    }

    uint64_t hash = map->hash_key(key);
    UNIT_Size result = set_entry(
        map,
        map->items,
        map->capacity,
        key,
        value,
        hash
    );
    assert(result == 0 || result == 1);
    if (result == 1) {
        ++map->len;
    }

    return _UNIT_OK;
}

void
_UNIT_Map_Clear(_UNIT_Map *map)
{
    assert(map != NULL);
    for (UNIT_Size index = 0; index < map->capacity; index++) {
        _UNIT_MapPair *item = &map->items[index];
        if (item->key != NULL) {
            if (map->dealloc_key != NULL) {
                map->dealloc_key(map->context, item->key);
            }
            if (map->dealloc_value != NULL) {
                map->dealloc_value(map->context, item->value);
            }
        }
    }
    _UNIT_Dealloc(map->context, map->items);
    map->items = NULL;
    map->len = 0;
    map->capacity = 0;
}
