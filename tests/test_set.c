#include "test_util.h"

#include <unit/internal/set.h>

// Use small integers cast to pointers as test values
#define PTR(n) ((void *)(uintptr_t)(n))

static void test_add_and_contains(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(42)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(42)));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(43)));
    _UNIT_Set_Clear(&set);
}

static void test_duplicate_add(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(10)));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(10)));
    ASSERT_EQ(set.len, 1);
    ASSERT(_UNIT_Set_Contains(&set, PTR(10)));
    _UNIT_Set_Clear(&set);
}

static void test_multiple_values(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(1)));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(2)));
    ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(3)));
    ASSERT_EQ(set.len, 3);
    ASSERT(_UNIT_Set_Contains(&set, PTR(1)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(2)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(3)));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(4)));
    _UNIT_Set_Clear(&set);
}

static void test_remove(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    _UNIT_Set_Add(&set, PTR(10));
    _UNIT_Set_Add(&set, PTR(20));
    _UNIT_Set_Remove(&set, PTR(10));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(10)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(20)));
    ASSERT_EQ(set.len, 1);
    _UNIT_Set_Clear(&set);
}

static void test_remove_nonexistent(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    _UNIT_Set_Add(&set, PTR(10));
    _UNIT_Set_Remove(&set, PTR(99));
    ASSERT(_UNIT_Set_Contains(&set, PTR(10)));
    ASSERT_EQ(set.len, 1);
    _UNIT_Set_Clear(&set);
}

static void test_remove_preserves_chain(UNIT_Context *context)
{
    // Force collisions by using values that hash to the same slot
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 4));
    _UNIT_Set_Add(&set, PTR(0));
    _UNIT_Set_Add(&set, PTR(4)); // same slot as 0 with capacity 4
    _UNIT_Set_Add(&set, PTR(8)); // same slot again
    _UNIT_Set_Remove(&set, PTR(0));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(0)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(4)));
    ASSERT(_UNIT_Set_Contains(&set, PTR(8)));
    ASSERT_EQ(set.len, 2);
    _UNIT_Set_Clear(&set);
}

static void test_grow(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 4));
    for (uintptr_t i = 1; i <= 100; ++i) {
        ASSERT_OK(context, _UNIT_Set_Add(&set, PTR(i)));
    }
    ASSERT_EQ(set.len, 100);
    for (uintptr_t i = 1; i <= 100; ++i) {
        ASSERT(_UNIT_Set_Contains(&set, PTR(i)));
    }
    ASSERT(!_UNIT_Set_Contains(&set, PTR(101)));
    _UNIT_Set_Clear(&set);
}

static void test_grow_preserves_all(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 2));
    for (uintptr_t i = 1; i <= 50; ++i) {
        _UNIT_Set_Add(&set, PTR(i));
        // Verify all previous values after each insert
        for (uintptr_t j = 1; j <= i; ++j) {
            ASSERT(_UNIT_Set_Contains(&set, PTR(j)));
        }
    }
    _UNIT_Set_Clear(&set);
}

static void test_null_pointer(UNIT_Context *context)
{
    // NULL is a valid pointer value to store
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_Set_Add(&set, NULL));
    ASSERT(_UNIT_Set_Contains(&set, NULL));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(1)));
    _UNIT_Set_Clear(&set);
}

static void test_real_pointers(UNIT_Context *context)
{
    int a, b, c;
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_Set_Add(&set, &a));
    ASSERT_OK(context, _UNIT_Set_Add(&set, &b));
    ASSERT(_UNIT_Set_Contains(&set, &a));
    ASSERT(_UNIT_Set_Contains(&set, &b));
    ASSERT(!_UNIT_Set_Contains(&set, &c));
    ASSERT_EQ(set.len, 2);
    _UNIT_Set_Clear(&set);
}

static void test_remove_then_readd(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    _UNIT_Set_Add(&set, PTR(42));
    _UNIT_Set_Remove(&set, PTR(42));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(42)));
    ASSERT_EQ(set.len, 0);
    _UNIT_Set_Add(&set, PTR(42));
    ASSERT(_UNIT_Set_Contains(&set, PTR(42)));
    ASSERT_EQ(set.len, 1);
    _UNIT_Set_Clear(&set);
}

static void test_empty(UNIT_Context *context)
{
    _UNIT_Set set;
    ASSERT_OK(context, _UNIT_Set_Init(&set, context, 8));
    ASSERT(!_UNIT_Set_Contains(&set, PTR(1)));
    ASSERT_EQ(set.len, 0);
    _UNIT_Set_Clear(&set);
}

int main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_set:\n");
    RUN_TEST(test_add_and_contains, &context);
    RUN_TEST(test_duplicate_add, &context);
    RUN_TEST(test_multiple_values, &context);
    RUN_TEST(test_remove, &context);
    RUN_TEST(test_remove_nonexistent, &context);
    RUN_TEST(test_remove_preserves_chain, &context);
    RUN_TEST(test_grow, &context);
    RUN_TEST(test_grow_preserves_all, &context);
    RUN_TEST(test_null_pointer, &context);
    RUN_TEST(test_real_pointers, &context);
    RUN_TEST(test_remove_then_readd, &context);
    RUN_TEST(test_empty, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
