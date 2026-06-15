#include "test_util.h"

static void test_emit8(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit8(&buf, 0x48));
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit8(&buf, 0xC3));
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 2);
    ASSERT_EQ(buf.data[0], 0x48);
    ASSERT_EQ(buf.data[1], 0xC3);
    _UNIT_CodeBuffer_Clear(&buf);
}

static void test_emit32(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit32(&buf, 0xDEADBEEF));
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 4);
    // Little-endian
    ASSERT_EQ(buf.data[0], 0xEF);
    ASSERT_EQ(buf.data[1], 0xBE);
    ASSERT_EQ(buf.data[2], 0xAD);
    ASSERT_EQ(buf.data[3], 0xDE);
    _UNIT_CodeBuffer_Clear(&buf);
}

static void test_emit64(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit64(&buf, 0x0102030405060708ULL));
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 8);
    ASSERT_EQ(buf.data[0], 0x08);
    ASSERT_EQ(buf.data[7], 0x01);
    _UNIT_CodeBuffer_Clear(&buf);
}

static void test_patch32(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit32(&buf, 0x00000000));
    _UNIT_CodeBuffer_Patch32(&buf, 0, 0x12345678);
    ASSERT_EQ(buf.data[0], 0x78);
    ASSERT_EQ(buf.data[1], 0x56);
    ASSERT_EQ(buf.data[2], 0x34);
    ASSERT_EQ(buf.data[3], 0x12);
    _UNIT_CodeBuffer_Clear(&buf);
}

static void test_reserve(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    UNIT_Size offset = _UNIT_CodeBuffer_Reserve(&buf, 7);
    ASSERT_EQ(offset, 0);
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 7);
    ASSERT_OK(context, _UNIT_CodeBuffer_Emit8(&buf, 0xAA));
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 8);
    ASSERT_EQ(buf.data[7], 0xAA);
    _UNIT_CodeBuffer_Clear(&buf);
}

static void test_grow(UNIT_Context *context)
{
    _UNIT_CodeBuffer buf;
    ASSERT_OK(context, _UNIT_CodeBuffer_Init(&buf, context));
    for (int i = 0; i < 100000; ++i) {
        ASSERT_OK(context, _UNIT_CodeBuffer_Emit8(&buf, (uint8_t)(i & 0xFF)));
    }
    ASSERT_EQ(_UNIT_CodeBuffer_CurrentIndex(&buf), 100000);
    ASSERT_EQ(buf.data[0], 0);
    ASSERT_EQ(buf.data[99], 99);
    _UNIT_CodeBuffer_Clear(&buf);
}

int main(void)
{
    UNIT_Context context;
    ASSERT(!UNIT_FAILED(UNIT_Context_Init(&context)));
    printf("test_code_buffer:\n");
    RUN_TEST(test_emit8, &context);
    RUN_TEST(test_emit32, &context);
    RUN_TEST(test_emit64, &context);
    RUN_TEST(test_patch32, &context);
    RUN_TEST(test_reserve, &context);
    RUN_TEST(test_grow, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
