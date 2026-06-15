#include "test_util.h"

static void test_set_and_get(UNIT_Context *context)
{
    _UNIT_SizeMap map;
    ASSERT_OK(context, _UNIT_SizeMap_Init(&map, context, 8));

    ASSERT_OK(context, _UNIT_SizeMap_Set(&map, 42, 100));
    UNIT_Size value;
    ASSERT(!UNIT_FAILED(_UNIT_SizeMap_Get(&map, 42, &value)));
    ASSERT_EQ(value, 100);

    _UNIT_SizeMap_Clear(&map);
}

static void test_overwrite(UNIT_Context *context)
{
    _UNIT_SizeMap map;
    ASSERT_OK(context, _UNIT_SizeMap_Init(&map, context, 8));

    ASSERT_OK(context, _UNIT_SizeMap_Set(&map, 42, 100));
    ASSERT_OK(context, _UNIT_SizeMap_Set(&map, 42, 200));
    UNIT_Size value;
    ASSERT_OK(context, _UNIT_SizeMap_Get(&map, 42, &value));
    ASSERT_EQ(value, 200);

    _UNIT_SizeMap_Clear(&map);
}

static void test_remove(UNIT_Context *context)
{
    _UNIT_SizeMap map;
    ASSERT_OK(context, _UNIT_SizeMap_Init(&map, context, 8));

    ASSERT_OK(context, _UNIT_SizeMap_Set(&map, 42, 100));
    _UNIT_SizeMap_Remove(&map, 42);
    UNIT_Size value;
    ASSERT(UNIT_FAILED(_UNIT_SizeMap_Get(&map, 42, &value)));

    _UNIT_SizeMap_Clear(&map);
}

int main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_size_map:\n");

    RUN_TEST(test_set_and_get, &context);
    RUN_TEST(test_overwrite, &context);
    RUN_TEST(test_remove, &context);

    UNIT_Context_Clear(&context);
    return 0;
}
