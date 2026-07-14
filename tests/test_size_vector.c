
#include "test_util.h"

#include <unit/internal/size_vector.h>

static void
test_append_and_get(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 10));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 20));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 30));
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 3);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 10);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 1), 20);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 2), 30);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_pop(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 10));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 20));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 30));
    ASSERT_EQ(_UNIT_SizeVector_Pop(&size_vector), 30);
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 2);
    ASSERT_EQ(_UNIT_SizeVector_Pop(&size_vector), 20);
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 1);
    ASSERT_EQ(_UNIT_SizeVector_Pop(&size_vector), 10);
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 0);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_set(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 10));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 20));
    _UNIT_SizeVector_SET(&size_vector, 0, 99);
    _UNIT_SizeVector_SET(&size_vector, 1, 42);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 99);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 1), 42);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_append_unchecked(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    _UNIT_SizeVector_APPEND(&size_vector, 100);
    _UNIT_SizeVector_APPEND(&size_vector, 200);
    _UNIT_SizeVector_APPEND(&size_vector, 300);
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 3);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 100);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 1), 200);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 2), 300);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_grow(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 2));
    for (UNIT_Size i = 0; i < 100; ++i) {
        ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, i * 7));
    }
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 100);
    for (UNIT_Size i = 0; i < 100; ++i) {
        ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, i), i * 7);
    }
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_empty(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 0);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_reuse_after_clear(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 42));
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 42);
    _UNIT_SizeVector_Clear(&size_vector);

    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 99));
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 1);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 99);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_push_pop_interleaved(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 1));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 2));
    ASSERT_EQ(_UNIT_SizeVector_Pop(&size_vector), 2);
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 3));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 4));
    ASSERT_EQ(_UNIT_SizeVector_SIZE(&size_vector), 3);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), 1);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 1), 3);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 2), 4);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_large_values(UNIT_Context *context)
{
    _UNIT_SizeVector size_vector;
    ASSERT_OK(context, _UNIT_SizeVector_Init(&size_vector, context, 4));
    UNIT_Size large = (UNIT_Size) - 1;
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, large));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, 0));
    ASSERT_OK(context, _UNIT_SizeVector_Append(&size_vector, large - 1));
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 0), large);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 1), 0);
    ASSERT_EQ(_UNIT_SizeVector_GET(&size_vector, 2), large - 1);
    _UNIT_SizeVector_Clear(&size_vector);
}

static void
test_new_and_free(UNIT_Context *context)
{
    _UNIT_SizeVector *size_vector = _UNIT_SizeVector_New(context, 4);
    ASSERT(size_vector != NULL);
    ASSERT_OK(context, _UNIT_SizeVector_Append(size_vector, 10));
    ASSERT_OK(context, _UNIT_SizeVector_Append(size_vector, 20));
    ASSERT_EQ(_UNIT_SizeVector_SIZE(size_vector), 2);
    ASSERT_EQ(_UNIT_SizeVector_GET(size_vector, 0), 10);
    ASSERT_EQ(_UNIT_SizeVector_GET(size_vector, 1), 20);
    _UNIT_SizeVector_Free(size_vector);
}

int
main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_size_vector:\n");
    RUN_TEST(test_append_and_get, &context);
    RUN_TEST(test_pop, &context);
    RUN_TEST(test_set, &context);
    RUN_TEST(test_append_unchecked, &context);
    RUN_TEST(test_grow, &context);
    RUN_TEST(test_empty, &context);
    RUN_TEST(test_reuse_after_clear, &context);
    RUN_TEST(test_push_pop_interleaved, &context);
    RUN_TEST(test_large_values, &context);
    RUN_TEST(test_new_and_free, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
