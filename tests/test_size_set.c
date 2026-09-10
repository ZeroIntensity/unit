#include "test_util.h"

static void
test_add_and_contains(UNIT_Context *context)
{
    _UNIT_SizeSet set;
    ASSERT_OK(context, _UNIT_SizeSet_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 42));
    ASSERT(_UNIT_SizeSet_Contains(&set, 42));
    ASSERT(!_UNIT_SizeSet_Contains(&set, 43));
    _UNIT_SizeSet_Clear(&set);
}

static void
test_duplicate_add(UNIT_Context *context)
{
    _UNIT_SizeSet set;
    ASSERT_OK(context, _UNIT_SizeSet_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 42));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 42));
    ASSERT(_UNIT_SizeSet_Contains(&set, 42));
    _UNIT_SizeSet_Clear(&set);
}

static void
test_remove(UNIT_Context *context)
{
    _UNIT_SizeSet set;
    ASSERT_OK(context, _UNIT_SizeSet_Init(&set, context, 8));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 10));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 20));
    _UNIT_SizeSet_Remove(&set, 10);
    ASSERT(!_UNIT_SizeSet_Contains(&set, 10));
    ASSERT(_UNIT_SizeSet_Contains(&set, 20));
    _UNIT_SizeSet_Clear(&set);
}

static void
test_remove_preserves_chain(UNIT_Context *context)
{
    // Insert values that collide, remove the first, ensure the second is still found
    _UNIT_SizeSet set;
    ASSERT_OK(context, _UNIT_SizeSet_Init(&set, context, 4));
    // With capacity 4, values 1 and 5 hash to the same slot (& 3 == 1)
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 1));
    ASSERT_OK(context, _UNIT_SizeSet_Add(&set, 5));
    _UNIT_SizeSet_Remove(&set, 1);
    ASSERT(!_UNIT_SizeSet_Contains(&set, 1));
    ASSERT(_UNIT_SizeSet_Contains(&set, 5));
    _UNIT_SizeSet_Clear(&set);
}

static void
test_many_values(UNIT_Context *context)
{
    _UNIT_SizeSet set;
    ASSERT_OK(context, _UNIT_SizeSet_Init(&set, context, 4));
    for (UNIT_Size i = 0; i < 10000; ++i) {
        ASSERT_OK(context, _UNIT_SizeSet_Add(&set, i));
    }

    for (UNIT_Size i = 0; i < 10000; ++i) {
        ASSERT(_UNIT_SizeSet_Contains(&set, i));
    }

    ASSERT(!_UNIT_SizeSet_Contains(&set, 10000));
    _UNIT_SizeSet_Clear(&set);
}

int
main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_size_set:\n");
    RUN_TEST(test_add_and_contains, &context);
    RUN_TEST(test_duplicate_add, &context);
    RUN_TEST(test_remove, &context);
    RUN_TEST(test_remove_preserves_chain, &context);
    RUN_TEST(test_many_values, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
