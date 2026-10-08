#include "test_util.h"

#include <unit/internal/compilation/architectures.h>
#include <unit/internal/ir/basic_block.h>

#if defined(__x86_64__) || defined(_M_X64)

static _UNIT_MachineItem
item(_UNIT_MachineItem_Type type, int64_t value)
{
    return (_UNIT_MachineItem) { .type = type, .value = value };
}

static _UNIT_MachineOperation
operation(_UNIT_MachineInstruction instruction,
          _UNIT_MachineItem *destination,
          _UNIT_MachineItem *left,
          _UNIT_MachineItem *right)
{
    return (_UNIT_MachineOperation) {
               .instruction = instruction,
               .destination = destination == NULL ? _UNIT_MachineDestination_NULL
                                                  : _UNIT_MachineDestination_FromDestination(
                   destination),
               .argument_1 = left,
               .argument_2 = right,
    };
}

// Use allocated operations so dead stores and argument permutations remain in
// the input to lowering. Compile for both ABIs, and execute only the host ABI.
static UNIT_ExecutableBuffer *
compile_operations(UNIT_Context *context,
                   UNIT_Platform platform,
                   _UNIT_MachineOperation *operations,
                   UNIT_Size count,
                   UNIT_Size memory_slots,
                   UNIT_Size expected_slots,
                   const char *symbol,
                   void *address)
{
    UNIT_Procedure procedure;
    ASSERT_OK(context, UNIT_Procedure_Init(&procedure, context, "call_register_test"));
    if (symbol != NULL) {
        ASSERT_OK(context, UNIT_Procedure_AddCallName(&procedure, symbol, 0));
    }

    UNIT_CompiledProcedure compiled = { .context = context, .platform = platform };
    _UNIT_Translation *translation = &compiled._translation;
    translation->context = context;
    translation->num_memory_slots = memory_slots;
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
                                        platform));
    ASSERT_OK(context, _UNIT_AMD64_Compile(translation, &compiled._compile_context));
    ASSERT_EQ(compiled._compile_context.stack_frame.next_slot, expected_slots);
    if (symbol != NULL && expected_slots == 0) {
        // A call still needs an aligned stack when it saves no registers.
        _UNIT_CodeBuffer *buffer = &compiled._compile_context.buffer;
        ASSERT_EQ(buffer->data[0], 0x48);
        ASSERT_EQ(buffer->data[1], 0x81);
        ASSERT_EQ(buffer->data[2], 0xec);
        ASSERT_EQ(buffer->data[3], 8);
    }

    UNIT_ExecutableBuffer *buffer = NULL;
    if (platform == UNIT_HOST_PLATFORM) {
        UNIT_SymbolMap symbols;
        ASSERT_OK(context, UNIT_SymbolMap_Init(&symbols, context));
        if (symbol != NULL) {
            ASSERT_OK(context, UNIT_SymbolMap_RegisterSymbol(&symbols, symbol, address));
        }

        buffer = UNIT_CompiledProcedure_JIT(&compiled, &symbols);
        ASSERT(buffer != NULL);
        UNIT_SymbolMap_Clear(&symbols);
    }

    _UNIT_CompileContext_Clear(&compiled._compile_context);
    _UNIT_Vector_Clear(&translation->blocks);
    _UNIT_Vector_Clear(&block.instructions);
    UNIT_Procedure_Clear(&procedure);
    return buffer;
}

static void
assert_result(UNIT_ExecutableBuffer *buffer, int64_t expected)
{
    if (buffer != NULL) {
        int64_t (*function)(void) = (int64_t (*)(void))UNIT_ExecutableBuffer_GetPointer(buffer);
        ASSERT_EQ(function(), expected);
        UNIT_ExecutableBuffer_Free(buffer);
    }
}

// Every allocatable register is caller-saved. Overwrite all of them to make
// preservation tests independent of the host C compiler's register choices.
static UNIT_ExecutableBuffer *
make_clobber_function(UNIT_Context *context)
{
    UNIT_Size count = UNIT_Platform_GET_ABI(UNIT_HOST_PLATFORM) == UNIT_ABI_WIN64 ? 6 : 8;
    _UNIT_MachineItem registers[8];
    _UNIT_MachineItem values[8];
    _UNIT_MachineOperation operations[9];
    for (UNIT_Size index = 0; index < count; ++index) {
        registers[index] = item(_UNIT_TYPE_REGISTER, index);
        values[index] = item(_UNIT_TYPE_CONSTANT, 1000 + index);
        operations[index] = operation(_UNIT_I_LOAD, &registers[index], &values[index], NULL);
    }

    _UNIT_MachineItem result = item(_UNIT_TYPE_CONSTANT, 7);
    operations[count] = operation(_UNIT_I_RETURN_VALUE, NULL, &result, NULL);
    return compile_operations(context, UNIT_HOST_PLATFORM, operations, count + 1, 0, 0, NULL, NULL);
}

static void
test_dead_registers(UNIT_Context *context)
{
    UNIT_ExecutableBuffer *callee = make_clobber_function(context);
    void *address = UNIT_ExecutableBuffer_GetPointer(callee);
    _UNIT_Vector arguments;
    ASSERT_OK(context, _UNIT_Vector_Init(&arguments, context, 1, NULL));
    _UNIT_MachineItem constant = item(_UNIT_TYPE_CONSTANT, 42);
    ASSERT_OK(context, _UNIT_Vector_Append(&arguments, &constant));
    _UNIT_MachineItem args = { .type = _UNIT_TYPE_CALL_ARGS, .call_args = &arguments };
    _UNIT_MachineItem symbol = item(_UNIT_TYPE_CONSTANT, 0);
    _UNIT_MachineItem result = item(_UNIT_TYPE_REGISTER, 0);
    _UNIT_MachineItem temporary = item(_UNIT_TYPE_REGISTER, 1);
    _UNIT_MachineItem dead = item(_UNIT_TYPE_CONSTANT, 99);
    _UNIT_MachineItem value = item(_UNIT_TYPE_CONSTANT, 35);
    _UNIT_MachineOperation operations[] = {
        operation(_UNIT_I_LOAD, &temporary, &dead, NULL),
        operation(_UNIT_I_CALL_SYMBOL, &result, &symbol, &args),
        operation(_UNIT_I_LOAD, &temporary, &value, NULL),
        operation(_UNIT_I_ADD, &result, &result, &temporary),
        operation(_UNIT_I_RETURN_VALUE, NULL, &result, NULL),
    };
    const UNIT_Platform platforms[] = {UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV,
                                       UNIT_ARCH_AMD64 | UNIT_ABI_WIN64};
    for (UNIT_Size index = 0; index < 2; ++index) {
        assert_result(compile_operations(context,
                                         platforms[index],
                                         operations,
                                         5,
                                         0,
                                         0,
                                         "clobber",
                                         address),
                      42);
    }

    _UNIT_Vector_Clear(&arguments);
    UNIT_ExecutableBuffer_Free(callee);
}

static void
test_live_register(UNIT_Context *context)
{
    UNIT_ExecutableBuffer *callee = make_clobber_function(context);
    void *address = UNIT_ExecutableBuffer_GetPointer(callee);
    _UNIT_Vector arguments;
    ASSERT_OK(context, _UNIT_Vector_Init(&arguments, context, 1, NULL));
    _UNIT_MachineItem args = { .type = _UNIT_TYPE_CALL_ARGS, .call_args = &arguments };
    _UNIT_MachineItem symbol = item(_UNIT_TYPE_CONSTANT, 0);
    _UNIT_MachineItem value = item(_UNIT_TYPE_CONSTANT, 35);
    const UNIT_Platform platforms[] = {UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV,
                                       UNIT_ARCH_AMD64 | UNIT_ABI_WIN64};
    for (int spilled = 0; spilled < 2; ++spilled) {
        // A spilled result must be stored before the live RAX value is restored.
        _UNIT_MachineItem result = item(spilled ? _UNIT_TYPE_MEMORY : _UNIT_TYPE_REGISTER, 0);
        _UNIT_MachineItem local = item(_UNIT_TYPE_REGISTER, spilled ? 0 : 2);
        _UNIT_MachineOperation operations[] = {
            operation(_UNIT_I_LOAD, &local, &value, NULL),
            operation(_UNIT_I_CALL_SYMBOL, &result, &symbol, &args),
            operation(_UNIT_I_ADD, &result, &result, &local),
            operation(_UNIT_I_RETURN_VALUE, NULL, &result, NULL),
        };
        for (UNIT_Size index = 0; index < 2; ++index) {
            assert_result(compile_operations(context,
                                             platforms[index],
                                             operations,
                                             4,
                                             spilled,
                                             spilled + 1,
                                             "clobber",
                                             address),
                          42);
        }
    }

    _UNIT_Vector_Clear(&arguments);
    UNIT_ExecutableBuffer_Free(callee);
}

static int64_t
weighted_arguments(int64_t first, int64_t second, int64_t third, int64_t fourth)
{
    return first + 10 * second + 100 * third + 1000 * fourth;
}

static void
test_argument_permutation(UNIT_Context *context)
{
    _UNIT_MachineItem registers[4];
    _UNIT_MachineItem values[4];
    _UNIT_MachineOperation operations[6];
    for (UNIT_Size index = 0; index < 4; ++index) {
        registers[index] = item(_UNIT_TYPE_REGISTER, index);
        values[index] = item(_UNIT_TYPE_CONSTANT, 11 * (index + 1));
        operations[index] = operation(_UNIT_I_LOAD, &registers[index], &values[index], NULL);
    }

    _UNIT_Vector arguments;
    ASSERT_OK(context, _UNIT_Vector_Init(&arguments, context, 4, NULL));
    const UNIT_Size order[] = {2, 1, 3, 0};
    for (UNIT_Size index = 0; index < 4; ++index) {
        ASSERT_OK(context, _UNIT_Vector_Append(&arguments, &registers[order[index]]));
    }

    _UNIT_MachineItem args = { .type = _UNIT_TYPE_CALL_ARGS, .call_args = &arguments };
    _UNIT_MachineItem symbol = item(_UNIT_TYPE_CONSTANT, 0);
    // The result replaces an argument in RAX; restoring it would lose the result.
    operations[4] = operation(_UNIT_I_CALL_SYMBOL, &registers[0], &symbol, &args);
    operations[5] = operation(_UNIT_I_RETURN_VALUE, NULL, &registers[0], NULL);
    const UNIT_Platform platforms[] = {UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV,
                                       UNIT_ARCH_AMD64 | UNIT_ABI_WIN64};
    for (UNIT_Size index = 0; index < 2; ++index) {
        assert_result(compile_operations(context,
                                         platforms[index],
                                         operations,
                                         6,
                                         0,
                                         4,
                                         "weighted",
                                         (void *)weighted_arguments),
                      15653);
    }

    _UNIT_Vector_Clear(&arguments);
}

static void
test_loop_live_registers(UNIT_Context *context)
{
    UNIT_ExecutableBuffer *callee = make_clobber_function(context);
    UNIT_SymbolMap symbols;
    ASSERT_OK(context, UNIT_SymbolMap_Init(&symbols, context));
    ASSERT_OK(context,
              UNIT_SymbolMap_RegisterSymbol(&symbols,
                                            "clobber",
                                            UNIT_ExecutableBuffer_GetPointer(callee)));
    UNIT_Procedure procedure;
    ASSERT_OK(context, UNIT_Procedure_Init(&procedure, context, "loop_call_registers"));
    UNIT_JumpLabel *loop = UNIT_Procedure_CreateJumpLabel(&procedure, "loop");
    UNIT_JumpLabel *done = UNIT_Procedure_CreateJumpLabel(&procedure, "done");
    ASSERT(loop != NULL && done != NULL);
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_STORE_LOCAL, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_STORE_LOCAL, 1));
    ASSERT_OK(context, UNIT_Procedure_UseLabel(&procedure, loop));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_LOCAL, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 5));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_COMPARE_GREATER_EQUAL, 0));
    ASSERT_OK(context, UNIT_Procedure_AddJump(&procedure, UNIT_OP_JUMP_IF_TRUE, done));
    ASSERT_OK(context, UNIT_Procedure_AddCallName(&procedure, "clobber", 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_LOCAL, 1));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_ADD, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_STORE_LOCAL, 1));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_LOCAL, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 1));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_ADD, 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_STORE_LOCAL, 0));
    // Neither local is read again in this block. Both feed the next iteration's PHIs.
    ASSERT_OK(context, UNIT_Procedure_AddCallName(&procedure, "clobber", 0));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_POP, 0));
    ASSERT_OK(context, UNIT_Procedure_AddJump(&procedure, UNIT_OP_JUMP, loop));
    ASSERT_OK(context, UNIT_Procedure_UseLabel(&procedure, done));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_LOCAL, 1));
    ASSERT_OK(context, UNIT_Procedure_AddOperation(&procedure, UNIT_OP_RETURN_VALUE, 0));
    for (int optimize = 0; optimize < 2; ++optimize) {
        UNIT_Procedure_SetFlags(&procedure,
                                optimize ? UNIT_FLAG_NONE
                                         : UNIT_FLAG_NO_OPTIMIZE_TRANSLATION);
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        ASSERT(compiled != NULL);
        UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(compiled, &symbols);
        ASSERT(buffer != NULL);
        assert_result(buffer, 35);
        UNIT_CompiledProcedure_Free(compiled);
    }

    UNIT_Procedure_Clear(&procedure);
    UNIT_SymbolMap_Clear(&symbols);
    UNIT_ExecutableBuffer_Free(callee);
}

#endif

int
main(void)
{
#if defined(__x86_64__) || defined(_M_X64)
    UNIT_Context context;
    ASSERT_OK(&context, UNIT_Context_Init(&context));
    RUN_TEST(test_dead_registers, &context);
    RUN_TEST(test_live_register, &context);
    RUN_TEST(test_argument_permutation, &context);
    RUN_TEST(test_loop_live_registers, &context);
    UNIT_Context_Clear(&context);
#endif
    return 0;
}
