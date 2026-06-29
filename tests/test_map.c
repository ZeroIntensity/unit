#include "test_util.h"
#include <string.h>

static bool
compare_strings(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b) == 0;
}

static UNIT_Size
hash_string(const void *key)
{
    const char *str = (const char *)key;
    UNIT_Size hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Deliberately bad hash to force collisions
static UNIT_Size
hash_constant(const void *key)
{
    (void)key;
    return 42;
}

static bool
compare_ints(const void *a, const void *b)
{
    return *(int *)a == *(int *)b;
}

static UNIT_Size
hash_int(const void *key)
{
    return (UNIT_Size)(*(int *)key);
}

static void
test_set_and_get(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    char *key = "hello";
    int value = 42;
    ASSERT_OK(context, _UNIT_Map_Set(&map, key, &value));
    int *result = _UNIT_Map_Get(&map, "hello");
    ASSERT(result != NULL);
    ASSERT_EQ(*result, 42);
    _UNIT_Map_Clear(&map);
}

static void
test_missing_key(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    void *result = _UNIT_Map_Get(&map, "nonexistent");
    ASSERT(result == NULL);
    _UNIT_Map_Clear(&map);
}

static void
test_overwrite(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    char *key = "hello";
    int val1 = 42, val2 = 99;
    _UNIT_Map_Set(&map, key, &val1);
    _UNIT_Map_Set(&map, key, &val2);
    int *result = _UNIT_Map_Get(&map, "hello");
    ASSERT(result != NULL);
    ASSERT_EQ(*result, 99);
    _UNIT_Map_Clear(&map);
}

static void
test_multiple_keys(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 4, compare_strings,
                                       hash_string, NULL, NULL));
    int a = 1, b = 2, c = 3;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "alpha", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "beta", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "gamma", &c));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "alpha"), 1);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "beta"), 2);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "gamma"), 3);
    _UNIT_Map_Clear(&map);
}

static void
test_collision_all_same_hash(UNIT_Context *context)
{
    // All keys hash to the same value, forcing linear probing
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 10;
    int b = 20;
    int c = 30;
    int d = 40;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "one", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "two", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "three", &c));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "four", &d));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "one"), 10);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "two"), 20);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "three"), 30);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "four"), 40);
    _UNIT_Map_Clear(&map);
}

static void
test_collision_overwrite(UNIT_Context *context)
{
    // Overwrite with all keys colliding
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 10;
    int b = 20;
    int c = 99;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "one", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "two", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "one", &c));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "one"), 99);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "two"), 20);
    _UNIT_Map_Clear(&map);
}

static void
test_collision_missing(UNIT_Context *context)
{
    // Lookup a missing key when all slots in the chain are occupied
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 10, b = 20;
    _UNIT_Map_Set(&map, "one", &a);
    _UNIT_Map_Set(&map, "two", &b);
    ASSERT(_UNIT_Map_Get(&map, "three") == NULL);
    _UNIT_Map_Clear(&map);
}

static void
test_int_keys(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[] = {1, 2, 3, 4, 5};
    int values[] = {100, 200, 300, 400, 500};
    for (int i = 0; i < 5; ++i) {
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    for (int i = 0; i < 5; ++i) {
        int *result = _UNIT_Map_Get(&map, &keys[i]);
        ASSERT(result != NULL);
        ASSERT_EQ(*result, (i + 1) * 100);
    }
    _UNIT_Map_Clear(&map);
}

static void
test_grow_from_small(UNIT_Context *context)
{
    // Start with capacity 2, insert many items to force multiple expansions
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 2, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[50];
    int values[50];
    for (int i = 0; i < 50; ++i) {
        keys[i] = i;
        values[i] = i * 7;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    // Verify all still accessible after growth
    for (int i = 0; i < 50; ++i) {
        int *result = _UNIT_Map_Get(&map, &keys[i]);
        ASSERT(result != NULL);
        ASSERT_EQ(*result, i * 7);
    }
    _UNIT_Map_Clear(&map);
}

static void
test_overwrite_after_grow(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 2, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[20];
    int values[20];
    for (int i = 0; i < 20; ++i) {
        keys[i] = i;
        values[i] = i * 10;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    // Overwrite after growth
    int new_val = 999;
    ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[5], &new_val));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, &keys[5]), 999);
    // Others unchanged
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, &keys[0]), 0);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, &keys[19]), 190);
    _UNIT_Map_Clear(&map);
}

static void
test_empty_string_key(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int value = 77;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "", &value));
    int *result = _UNIT_Map_Get(&map, "");
    ASSERT(result != NULL);
    ASSERT_EQ(*result, 77);
    ASSERT(_UNIT_Map_Get(&map, "notempty") == NULL);
    _UNIT_Map_Clear(&map);
}

static void
test_similar_keys(UNIT_Context *context)
{
    // Keys that are similar but not equal
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    int d = 4;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "abc", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "abd", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "abcd", &c));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "ab", &d));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "abc"), 1);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "abd"), 2);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "abcd"), 3);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "ab"), 4);
    _UNIT_Map_Clear(&map);
}

static void
test_null_value(UNIT_Context *context)
{
    // Storing NULL as a value should be distinguishable from missing
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "key", NULL));
    _UNIT_Map_Clear(&map);
}

static void
test_many_collisions_with_grow(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 2, compare_strings,
                                       hash_constant, NULL, NULL));
    char keys[20][8];
    int values[20];
    for (int i = 0; i < 20; ++i) {
        snprintf(keys[i], sizeof(keys[i]), "key%d", i);
        values[i] = i * 3;
        ASSERT_OK(context, _UNIT_Map_Set(&map, keys[i], &values[i]));
    }
    for (int i = 0; i < 20; ++i) {
        int *result = _UNIT_Map_Get(&map, keys[i]);
        ASSERT(result != NULL);
        ASSERT_EQ(*result, i * 3);
    }
    _UNIT_Map_Clear(&map);
}

static void
test_set_get_set_get(UNIT_Context *context)
{
    // Interleave sets and gets
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 4, compare_strings,
                                       hash_string, NULL, NULL));
    int a = 1, b = 2, c = 3;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "x", &a));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "x"), 1);
    ASSERT_OK(context, _UNIT_Map_Set(&map, "y", &b));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "x"), 1);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "y"), 2);
    ASSERT_OK(context, _UNIT_Map_Set(&map, "x", &c));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "x"), 3);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "y"), 2);
    _UNIT_Map_Clear(&map);
}

static void
test_reuse_after_clear(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int a = 1;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "first", &a));
    ASSERT(*(int *)_UNIT_Map_Get(&map, "first") == 1);
    _UNIT_Map_Clear(&map);

    // Reinitialize and use again
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int b = 2;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "second", &b));
    ASSERT(_UNIT_Map_Get(&map, "first") == NULL);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "second"), 2);
    _UNIT_Map_Clear(&map);
}

static void
test_overwrite_repeatedly(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int values[100];
    for (int i = 0; i < 100; ++i) {
        values[i] = i;
        ASSERT_OK(context, _UNIT_Map_Set(&map, "key", &values[i]));
    }
    int *result = _UNIT_Map_Get(&map, "key");
    ASSERT(result != NULL);
    ASSERT_EQ(*result, 99);
    _UNIT_Map_Clear(&map);
}

static void
test_overwrite_with_collisions(UNIT_Context *context)
{
    // Overwrite keys that all collide
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "x", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "y", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "z", &c));

    int a2 = 10;
    int b2 = 20;
    int c2 = 30;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "x", &a2));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "y", &b2));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "z", &c2));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "x"), 10);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "y"), 20);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "z"), 30);
    _UNIT_Map_Clear(&map);
}

static void test_high_load_factor(UNIT_Context *context)
{
    // Fill to near capacity with small initial size
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 4, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[3];
    int values[3];
    // 3/4 = 75% load — right at the expansion threshold
    for (int i = 0; i < 3; ++i) {
        keys[i] = i;
        values[i] = i * 100;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    for (int i = 0; i < 3; ++i) {
        int *result = _UNIT_Map_Get(&map, &keys[i]);
        ASSERT(result != NULL);
        ASSERT_EQ(*result, i * 100);
    }
    _UNIT_Map_Clear(&map);
}

static void test_expansion_preserves_all(UNIT_Context *context)
{
    // Force multiple expansions and verify nothing is lost
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 2, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[200];
    int values[200];
    for (int i = 0; i < 200; ++i) {
        keys[i] = i;
        values[i] = i * 3;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
        for (int j = 0; j <= i; ++j) {
            int *result = _UNIT_Map_Get(&map, &keys[j]);
            ASSERT(result != NULL);
            ASSERT_EQ(*result, j * 3);
        }
    }
    _UNIT_Map_Clear(&map);
}

static void test_collision_chain_not_broken_by_overwrite(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    int a2 = 99;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "aaa", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "bbb", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "ccc", &c));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "aaa", &a2));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "aaa"), 99);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "bbb"), 2);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "ccc"), 3);
    _UNIT_Map_Clear(&map);
}

static void test_overwrite_middle_of_chain(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    int b2 = 55;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "first", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "second", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "third", &c));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "second", &b2));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "first"), 1);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "second"), 55);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "third"), 3);
    _UNIT_Map_Clear(&map);
}

static void test_overwrite_end_of_chain(UNIT_Context *context)
{
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    int c2 = 77;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "aaa", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "bbb", &b));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "ccc", &c));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "ccc", &c2));
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "aaa"), 1);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "bbb"), 2);
    ASSERT_EQ(*(int *)_UNIT_Map_Get(&map, "ccc"), 77);
    _UNIT_Map_Clear(&map);
}

static void test_missing_after_collisions(UNIT_Context *context)
{
    // All collide, lookup a key that was never inserted
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 4, compare_strings,
                                       hash_constant, NULL, NULL));
    int a = 1;
    int b = 2;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "exists1", &a));
    ASSERT_OK(context, _UNIT_Map_Set(&map, "exists2", &b));
    ASSERT(_UNIT_Map_Get(&map, "nope") == NULL);
    ASSERT(_UNIT_Map_Get(&map, "also_nope") == NULL);
    _UNIT_Map_Clear(&map);
}

static void test_overwrite_preserves_count(UNIT_Context *context)
{
    // Overwriting should not increment len
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 8, compare_strings,
                                       hash_string, NULL, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    ASSERT_OK(context, _UNIT_Map_Set(&map, "key", &a));
    ASSERT_EQ(map.len, 1);
    ASSERT_OK(context, _UNIT_Map_Set(&map, "key", &b));
    ASSERT_EQ(map.len, 1);
    ASSERT_OK(context, _UNIT_Map_Set(&map, "key", &c));
    ASSERT_EQ(map.len, 1);
    _UNIT_Map_Clear(&map);
}

static void test_many_overwrites_with_expansion(UNIT_Context *context)
{
    // Mix of new inserts and overwrites across expansions
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 2, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[50];
    int values[50];
    for (int i = 0; i < 50; ++i) {
        // Only 10 unique keys
        keys[i] = i % 10;
        values[i] = i;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    // Each key 0-9 was overwritten multiple times, last write wins
    // keys[40]=0, keys[41]=1, ..., keys[49]=9
    // values[40]=40, values[41]=41, ..., values[49]=49
    for (int i = 0; i < 10; ++i) {
        int *result = _UNIT_Map_Get(&map, &keys[i]);
        ASSERT(result != NULL);
        ASSERT_EQ(*result, 40 + i);
    }
    ASSERT_EQ(map.len, 10);
    _UNIT_Map_Clear(&map);
}

static void test_get_full_table_missing_key(UNIT_Context *context)
{
    // Ensure Get() terminates even with high load
    _UNIT_Map map;
    ASSERT_OK(context, _UNIT_Map_Init(&map, context, 4, compare_ints,
                                       hash_int, NULL, NULL));
    int keys[2];
    int values[2];
    // 2/4 = 50% load
    for (int i = 0; i < 2; ++i) {
        keys[i] = i;
        values[i] = i;
        ASSERT_OK(context, _UNIT_Map_Set(&map, &keys[i], &values[i]));
    }
    int missing = 999;
    ASSERT(_UNIT_Map_Get(&map, &missing) == NULL);
    _UNIT_Map_Clear(&map);
}

int main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_map:\n");
    RUN_TEST(test_set_and_get, &context);
    RUN_TEST(test_missing_key, &context);
    RUN_TEST(test_overwrite, &context);
    RUN_TEST(test_multiple_keys, &context);
    RUN_TEST(test_collision_all_same_hash, &context);
    RUN_TEST(test_collision_overwrite, &context);
    RUN_TEST(test_collision_missing, &context);
    RUN_TEST(test_int_keys, &context);
    RUN_TEST(test_grow_from_small, &context);
    RUN_TEST(test_overwrite_after_grow, &context);
    RUN_TEST(test_empty_string_key, &context);
    RUN_TEST(test_similar_keys, &context);
    RUN_TEST(test_null_value, &context);
    RUN_TEST(test_many_collisions_with_grow, &context);
    RUN_TEST(test_set_get_set_get, &context);
    RUN_TEST(test_reuse_after_clear, &context);
    RUN_TEST(test_overwrite_repeatedly, &context);
    RUN_TEST(test_overwrite_with_collisions, &context);
    RUN_TEST(test_high_load_factor, &context);
    RUN_TEST(test_expansion_preserves_all, &context);
    RUN_TEST(test_collision_chain_not_broken_by_overwrite, &context);
    RUN_TEST(test_overwrite_middle_of_chain, &context);
    RUN_TEST(test_overwrite_end_of_chain, &context);
    RUN_TEST(test_missing_after_collisions, &context);
    RUN_TEST(test_overwrite_preserves_count, &context);
    RUN_TEST(test_many_overwrites_with_expansion, &context);
    RUN_TEST(test_get_full_table_missing_key, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
