#include "test_util.h"

#include <unit/internal/architectures.h>
#include <unit/internal/basic_block.h>

#if defined(__x86_64__) || defined(_M_X64)

static _UNIT_MachineItem
item(_UNIT_MachineItem_Type type, int64_t value)
{
    return (_UNIT_MachineItem) { .type = type, .value = value };
}

static _UNIT_MachineOperation
operation(_UNIT_MachineInstruction instruction,
          _UNIT_MachineItem *dst,
          _UNIT_MachineItem *left,
          _UNIT_MachineItem *right)
{
    return (_UNIT_MachineOperation) {
               .instruction = instruction,
               .destination = dst == NULL ? _UNIT_MachineDestination_NULL
                                          : _UNIT_MachineDestination_FromDestination(dst),
               .argument_1 = left,
               .argument_2 = right,
    };
}

// Feed allocated machine operations directly to lowering so the optimizer cannot
// fold away the operand combinations under test.
static int64_t
execute(UNIT_Context *context, _UNIT_MachineOperation *operations, UNIT_Size count)
{
    UNIT_Procedure procedure;
    ASSERT_OK(context, UNIT_Procedure_Init(&procedure, context, "lower_test"));
    UNIT_CompiledProcedure compiled = { .context = context, .platform = UNIT_HOST_PLATFORM };
    _UNIT_Translation *translation = &compiled._translation;
    translation->num_memory_slots = 32;
    _UNIT_BasicBlock block = {0};
    ASSERT_OK(context, _UNIT_Vector_Init(&block.instructions, context, count, NULL));
    for (UNIT_Size index = 0; index < count; ++index) {
        ASSERT_OK(context, _UNIT_Vector_Append(&block.instructions, &operations[index]));
    }

    ASSERT_OK(context, _UNIT_Vector_Init(&translation->blocks, context, 1, NULL));
    ASSERT_OK(context, _UNIT_Vector_Append(&translation->blocks, &block));
    ASSERT_OK(context,
              _UNIT_CompileContext_Init(&compiled._compile_context,
                                        context,
                                        &procedure,
                                        translation,
                                        UNIT_HOST_PLATFORM));
    ASSERT_OK(context, _UNIT_AMD64_Compile(translation, &compiled._compile_context));
    UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(&compiled, NULL);
    ASSERT(buffer != NULL);
    int64_t (*function)(void) = (int64_t (*)(void))UNIT_ExecutableBuffer_GetPointer(buffer);
    int64_t result = function();
    UNIT_ExecutableBuffer_Free(buffer);
    _UNIT_CompileContext_Clear(&compiled._compile_context);
    _UNIT_Vector_Clear(&translation->blocks);
    _UNIT_Vector_Clear(&block.instructions);
    UNIT_Procedure_Clear(&procedure);
    return result;
}

static void
test_binary_operands(UNIT_Context *context)
{
    _UNIT_MachineItem lhs_value = item(_UNIT_TYPE_CONSTANT, 23);
    const int64_t rhs_values[] = {5, -5, INT32_MAX, (int64_t) UINT32_MAX, INT64_C(1) << 40};
    const _UNIT_MachineInstruction instructions[] = {_UNIT_I_ADD, _UNIT_I_SUB, _UNIT_I_MUL};
    for (UNIT_Size value = 0; value < 5; ++value) {
        _UNIT_MachineItem rhs_value = item(_UNIT_TYPE_CONSTANT, rhs_values[value]);
        _UNIT_MachineItem left[] = {item(_UNIT_TYPE_REGISTER, 0), item(_UNIT_TYPE_MEMORY, 0),
                                    lhs_value};
        _UNIT_MachineItem right[] = {item(_UNIT_TYPE_REGISTER, 2), item(_UNIT_TYPE_MEMORY, 20),
                                     rhs_value};
        _UNIT_MachineItem destinations[] = {left[0], right[0], left[1], right[1]};
        for (UNIT_Size inst = 0; inst < 3; ++inst) {
            int64_t expected = inst == 0 ? 23 + rhs_values[value]
                                         : inst ==
                               1 ? 23 - rhs_values[value] : 23 * rhs_values[value];
            for (UNIT_Size l = 0; l < 3; ++l) {
                for (UNIT_Size r = 0; r < 3; ++r) {
                    for (UNIT_Size d = 0; d < 4; ++d) {
                        _UNIT_MachineOperation ops[] = {
                            operation(_UNIT_I_LOAD, &left[0], &lhs_value, NULL),
                            operation(_UNIT_I_LOAD, &left[1], &lhs_value, NULL),
                            operation(_UNIT_I_LOAD, &right[0], &rhs_value, NULL),
                            operation(_UNIT_I_LOAD, &right[1], &rhs_value, NULL),
                            operation(instructions[inst], &destinations[d], &left[l], &right[r]),
                            operation(_UNIT_I_RETURN_VALUE, NULL, &destinations[d], NULL),
                        };
                        ASSERT_EQ(execute(context, ops, 6), expected);
                    }
                }
            }
        }
    }
}

static void
test_division_registers(UNIT_Context *context)
{
    _UNIT_MachineItem rax = item(_UNIT_TYPE_REGISTER, 0);
    _UNIT_MachineItem rdx = item(_UNIT_TYPE_REGISTER, 2);
    _UNIT_MachineItem lhs = item(_UNIT_TYPE_CONSTANT, -23);
    _UNIT_MachineItem rhs = item(_UNIT_TYPE_CONSTANT, 5);
    _UNIT_MachineItem destinations[] = {rax, rdx, item(_UNIT_TYPE_MEMORY, 20)};
    for (int reverse = 0; reverse < 2; ++reverse) {
        _UNIT_MachineItem *left = reverse ? &rdx : &rax;
        _UNIT_MachineItem *right = reverse ? &rax : &rdx;
        for (int modulo = 0; modulo < 2; ++modulo) {
            for (UNIT_Size d = 0; d < 3; ++d) {
                _UNIT_MachineOperation ops[] = {
                    operation(_UNIT_I_LOAD, left, &lhs, NULL),
                    operation(_UNIT_I_LOAD, right, &rhs, NULL),
                    operation(modulo ? _UNIT_I_MOD : _UNIT_I_DIV, &destinations[d], left, right),
                    operation(_UNIT_I_RETURN_VALUE, NULL, &destinations[d], NULL),
                };
                ASSERT_EQ(execute(context, ops, 4), modulo ? -3 : -4);
            }
        }
    }
}

static void
test_sized_memory(UNIT_Context *context)
{
    for (int64_t size = 1; size <= 8; size *= 2) {
        uint64_t memory = UINT64_C(0x1122334455667788);
        _UNIT_MachineItem address_value = item(_UNIT_TYPE_CONSTANT, (intptr_t) &memory);
        _UNIT_MachineItem value = item(_UNIT_TYPE_CONSTANT, INT64_C(0x123456789abcdef0));
        _UNIT_MachineItem address = item(_UNIT_TYPE_MEMORY, 20);
        _UNIT_MachineItem source = item(_UNIT_TYPE_MEMORY, 21);
        _UNIT_MachineItem width = item(_UNIT_TYPE_CONSTANT, size);
        _UNIT_MachineItem result = item(_UNIT_TYPE_MEMORY, 22);
        _UNIT_MachineOperation ops[] = {
            operation(_UNIT_I_LOAD, &address, &address_value, NULL),
            operation(_UNIT_I_LOAD, &source, &value, NULL),
            operation(_UNIT_I_WRITE_BYTES, &address, &source, &width),
            operation(_UNIT_I_READ_BYTES, &result, &address, &width),
            operation(_UNIT_I_RETURN_VALUE, NULL, &result, NULL),
        };
        uint64_t mask = UINT64_MAX >> ((8 - size) * 8);
        ASSERT_EQ((uint64_t) execute(context, ops, 5), (uint64_t) value.value & mask);
        ASSERT_EQ(memory, (UINT64_C(0x1122334455667788) & ~mask) | ((uint64_t) value.value & mask));
    }
}

static void
test_comparison_immediates(UNIT_Context *context)
{
    const int64_t values[] = {-129, -1, 127, 128, 255, 256, INT64_C(1) << 40};
    for (UNIT_Size index = 0; index < 7; ++index) {
        _UNIT_MachineItem left = item(_UNIT_TYPE_CONSTANT, values[index] - 1);
        _UNIT_MachineItem right = item(_UNIT_TYPE_CONSTANT, values[index]);
        _UNIT_MachineItem label = item(_UNIT_TYPE_CONSTANT, 0);
        _UNIT_MachineItem zero = item(_UNIT_TYPE_CONSTANT, 0);
        _UNIT_MachineItem one = item(_UNIT_TYPE_CONSTANT, 1);
        _UNIT_MachineOperation ops[] = {
            operation(_UNIT_I_JUMP_IF_LESS, &label, &left, &right),
            operation(_UNIT_I_RETURN_VALUE, NULL, &zero, NULL),
            operation(_UNIT_I_JUMP_LABEL, &label, NULL, NULL),
            operation(_UNIT_I_RETURN_VALUE, NULL, &one, NULL),
        };
        ASSERT_EQ(execute(context, ops, 4), 1);
    }
}

#endif

int
main(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    UNIT_Context context;
    ASSERT_OK(&context, UNIT_Context_Init(&context));
    RUN_TEST(test_binary_operands, &context);
    RUN_TEST(test_division_registers, &context);
    RUN_TEST(test_sized_memory, &context);
    RUN_TEST(test_comparison_immediates, &context);
    UNIT_Context_Clear(&context);
#endif
    return 0;
}
