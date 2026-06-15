#include "test_util.h"

static void test_append_and_get(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 4, NULL));
    int a = 10;
    int b = 20;
    int c = 30;
    ASSERT_OK(context, _UNIT_Vector_Append(&vec, &a));
    ASSERT_OK(context, _UNIT_Vector_Append(&vec, &b));
    ASSERT_OK(context, _UNIT_Vector_Append(&vec, &c));
    ASSERT_EQ(_UNIT_Vector_SIZE(&vec), 3);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 0), 10);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 1), 20);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 2), 30);
    _UNIT_Vector_Clear(&vec);
}

static void test_pop(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 4, NULL));
    int a = 10;
    int b = 20;
    _UNIT_Vector_Append(&vec, &a);
    _UNIT_Vector_Append(&vec, &b);
    int *popped = _UNIT_Vector_Pop(&vec);
    ASSERT_EQ(*popped, 20);
    ASSERT_EQ(_UNIT_Vector_SIZE(&vec), 1);
    _UNIT_Vector_Clear(&vec);
}

static void test_set(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 4, NULL));
    int a = 10, b = 20, c = 99;
    _UNIT_Vector_Append(&vec, &a);
    _UNIT_Vector_Append(&vec, &b);
    _UNIT_Vector_SET(&vec, 1, &c);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 1), 99);
    _UNIT_Vector_Clear(&vec);
}

static void test_grow(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 2, NULL));
    int values[500];
    for (int i = 0; i < 500; ++i) {
        values[i] = i * 10;
        ASSERT_OK(context, _UNIT_Vector_Append(&vec, &values[i]));
    }
    ASSERT_EQ(_UNIT_Vector_SIZE(&vec), 500);
    for (int i = 0; i < 500; ++i) {
        ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, i), i * 10);
    }
    _UNIT_Vector_Clear(&vec);
}

static void test_reverse(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 4, NULL));
    int a = 1;
    int b = 2;
    int c = 3;
    _UNIT_Vector_Append(&vec, &a);
    _UNIT_Vector_Append(&vec, &b);
    _UNIT_Vector_Append(&vec, &c);
    _UNIT_Vector_Reverse(&vec);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 0), 3);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 1), 2);
    ASSERT_EQ(*(int *)_UNIT_Vector_GET(&vec, 2), 1);
    _UNIT_Vector_Clear(&vec);
}

static void test_empty(UNIT_Context *context)
{
    _UNIT_Vector vec;
    ASSERT_OK(context, _UNIT_Vector_Init(&vec, context, 4, NULL));
    ASSERT_EQ(_UNIT_Vector_SIZE(&vec), 0);
    _UNIT_Vector_Clear(&vec);
}

int main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_vector:\n");
    RUN_TEST(test_append_and_get, &context);
    RUN_TEST(test_pop, &context);
    RUN_TEST(test_set, &context);
    RUN_TEST(test_grow, &context);
    RUN_TEST(test_reverse, &context);
    RUN_TEST(test_empty, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
