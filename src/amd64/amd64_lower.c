#include <stdio.h>
#include <stdlib.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>
#include <unit/internal/architectures.h>

#include "amd64_local.h"

AMD64_Operand
reg(AMD64_Register r) {
    return (AMD64_Operand) {
        .kind = OPERAND_REGISTER,
        .reg = r
    };
}

AMD64_Operand
immediate(uint64_t value) {
    return (AMD64_Operand) {
        .kind = OPERAND_IMMEDIATE,
        .immediate = value
    };
}

AMD64_Operand
stack_slot(uint64_t offset) {
    return (AMD64_Operand) {
        .kind = OPERAND_STACK,
        // This is kind of cheating, but a stack_offset field would complicate
        // things and would be a uint64_t anyway.
        .immediate = offset
    };
}

static const AMD64_Register register_map[] = {
    REG_RAX,
    REG_RCX,
    REG_RDX,
    REG_RSI,
    REG_RDI,
    REG_R8,
    REG_R9,
    REG_R10,
};

AMD64_Operand
machine_item_to_operand(_UNIT_MachineItem *machine_item)
{
    assert(machine_item != NULL);
    if (machine_item->type == _UNIT_TYPE_REGISTER) {
        return reg(register_map[machine_item->value]);
    } else if (machine_item->type == _UNIT_TYPE_CONSTANT) {
        return immediate(machine_item->value);
    } else if (machine_item->type == _UNIT_TYPE_CALL_ARGS) {
        // TODO: Gracefully fail here
        printf("cannot use call args as operand");
        abort();
    } else {
        return stack_slot(machine_item->value * 8);
    }
}

#define SRC_DEST_HELPER(name, opcode_name)                                                      \
    static inline AMD64_Instruction *                                                           \
    name(UNIT_Context *context, AMD64_Operand dst, AMD64_Operand src) {                         \
        AMD64_Instruction *instruction = _UNIT_Alloc(context,                                   \
                                                     sizeof(AMD64_Instruction));                \
        if (instruction == NULL) {                                                              \
            return NULL;                                                                        \
        }                                                                                       \
        instruction->opcode = opcode_name;                                                      \
        instruction->operands[0] = dst;                                                         \
        instruction->operands[1] = src;                                                         \
        instruction->operand_count = 2;                                                         \
        return instruction;                                                                     \
    }

#define NO_ARGS_HELPER(name, opcode_name)                                                       \
    static inline AMD64_Instruction *                                                           \
    name(UNIT_Context *context) {                                                               \
        AMD64_Instruction *instruction = _UNIT_Alloc(context,                                   \
                                                     sizeof(AMD64_Instruction));                \
        if (instruction == NULL) {                                                              \
            return NULL;                                                                        \
        }                                                                                       \
        instruction->opcode = opcode_name;                                                      \
        instruction->operand_count = 0;                                                         \
        return instruction;                                                                     \
    }

#define SINGLE_OPERAND_HELPER(name, opcode_name)                                                \
    static inline AMD64_Instruction *                                                           \
    name(UNIT_Context *context, AMD64_Operand operand) {                                        \
        AMD64_Instruction *instruction = _UNIT_Alloc(context,                                   \
                                                     sizeof(AMD64_Instruction));                \
        if (instruction == NULL) {                                                              \
            return NULL;                                                                        \
        }                                                                                       \
        instruction->opcode = opcode_name;                                                      \
        instruction->operands[0] = operand;                                                     \
        instruction->operand_count = 1;                                                         \
        return instruction;                                                                     \
    }

SRC_DEST_HELPER(mov, AMD64_MOV)
SRC_DEST_HELPER(add, AMD64_ADD)
SRC_DEST_HELPER(sub, AMD64_SUB)
NO_ARGS_HELPER(ret, AMD64_RET)
NO_ARGS_HELPER(syscall, AMD64_SYSCALL)
SINGLE_OPERAND_HELPER(call_indirect, AMD64_CALL_INDIRECT)
SINGLE_OPERAND_HELPER(call_symbol, AMD64_CALL_SYMBOL)
SINGLE_OPERAND_HELPER(jmp, AMD64_JUMP)
SINGLE_OPERAND_HELPER(_jmp_label, AMD64_JUMP_LABEL)
SINGLE_OPERAND_HELPER(jump_if_equal, AMD64_JUMP_IF_EQUAL)
SINGLE_OPERAND_HELPER(jump_if_not_equal, AMD64_JUMP_IF_NOT_EQUAL)
SINGLE_OPERAND_HELPER(jump_if_greater, AMD64_JUMP_IF_GREATER)
SINGLE_OPERAND_HELPER(jump_if_less, AMD64_JUMP_IF_LESS)
SINGLE_OPERAND_HELPER(jump_if_greater_equal, AMD64_JUMP_IF_GREATER_EQUAL)
SINGLE_OPERAND_HELPER(jump_if_less_equal, AMD64_JUMP_IF_LESS_EQUAL)
SRC_DEST_HELPER(load_string, AMD64_LOAD_STRING)
SRC_DEST_HELPER(cmp, AMD64_COMPARE)
SRC_DEST_HELPER(lea, AMD64_LOAD_ADDRESS)

static const AMD64_Register argument_registers[] = {
    REG_RDI,
    REG_RSI,
    REG_RDX,
    REG_RCX,
    REG_R8,
    REG_R9,
};

#define EMIT(op)                                                             \
    if (UNIT_FAILED(AMD64_encode_instruction(compile_context, op))) {        \
        return UNIT_FAIL;                                                    \
    }

static UNIT_Status
translate_operation(_UNIT_CompileContext *compile_context,
                    _UNIT_MachineOperation *operation)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    UNIT_Context *ctx = compile_context->context;

#define OP(value) machine_item_to_operand(operation->value)

    switch (operation->instruction) {
        case _UNIT_I_MOVE: {
            EMIT(mov(ctx, OP(destination), OP(argument_1)));
            break;
        }

        case _UNIT_I_ADD: {
            EMIT(add(ctx, OP(destination), OP(argument_1)));
            break;
        }

        case _UNIT_I_CALL_SYMBOL: {
            assert(operation->argument_2->type == _UNIT_TYPE_CALL_ARGS);
            _UNIT_Vector *arguments = operation->argument_2->call_args;
            UNIT_Size num_arguments = _UNIT_Vector_SIZE(arguments);
            assert(num_arguments <= 6);

            UNIT_Size rax_slot = _UNIT_StackFrame_AllocateSlot(&compile_context->stack_frame);
            EMIT(mov(ctx, stack_slot(rax_slot), reg(REG_RAX)));

            // Save argument registers into stack frame slots
            UNIT_Size save_slots[6];
            for (UNIT_Size argument = 0; argument < num_arguments; ++argument) {
                save_slots[argument] = _UNIT_StackFrame_AllocateSlot(&compile_context->stack_frame);
                assert(save_slots[argument] % 8 == 0);
                AMD64_Register argument_register = argument_registers[argument];
                EMIT(mov(ctx, stack_slot(save_slots[argument]), reg(argument_register)));
            }

            // Set up arguments
            for (UNIT_Size argument = 0; argument < num_arguments; ++argument) {
                AMD64_Register argument_register = argument_registers[argument];
                AMD64_Operand value = machine_item_to_operand(
                    _UNIT_Vector_GET(arguments, argument)
                );
                EMIT(mov(ctx, reg(argument_register), value));
            }

            EMIT(mov(ctx, reg(REG_RAX), immediate(0)));
            EMIT(call_symbol(ctx, OP(argument_1)));

            // Restore argument registers from stack frame slots
            for (UNIT_Size argument = 0; argument < num_arguments; ++argument) {
                AMD64_Register argument_register = argument_registers[argument];
                EMIT(mov(ctx, reg(argument_register), stack_slot(save_slots[argument])));
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, save_slots[argument]);
            }

            EMIT(mov(ctx, reg(REG_RAX), stack_slot(rax_slot)));
            _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, rax_slot);

            break;
        }

        case _UNIT_I_EXIT: {
            EMIT(mov(ctx, reg(REG_RAX), immediate(60))); // syscall number for exit
            EMIT(mov(ctx, reg(REG_RDI), OP(destination)));
            EMIT(syscall(ctx));
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            EMIT(mov(ctx, reg(REG_RAX), OP(destination)));
            // ret() will be emitted after the loop
            break;
        }

        case _UNIT_I_JUMP: {
            EMIT(jmp(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_LABEL: {
            EMIT(_jmp_label(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_COMPARE_EQUAL: {
            EMIT(cmp(ctx, OP(argument_2), OP(argument_1)));
            break;
        }

        case _UNIT_I_JUMP_IF_EQUAL: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_equal(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_IF_NOT_EQUAL: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_not_equal(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_IF_LESS: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_less(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_IF_GREATER: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_greater(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_IF_LESS_EQUAL: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_less_equal(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_JUMP_IF_GREATER_EQUAL: {
            EMIT(cmp(ctx, OP(argument_1), OP(argument_2)));
            EMIT(jump_if_greater_equal(ctx, OP(destination)));
            break;
        }

        case _UNIT_I_LOAD_STRING: {
            EMIT(load_string(ctx, OP(destination), OP(argument_1)));
            break;
        }

        case _UNIT_I_ADDRESS_OF: {
            EMIT(lea(ctx, OP(destination), OP(argument_1)));
            break;
        }
    }

    return UNIT_OK;
#undef OP
}

UNIT_Status
_UNIT_AMD64_Compile(_UNIT_Translation *translation,
                    _UNIT_CompileContext *compile_context)
{
    // Reserve space for the prologue (sub rsp, imm32 = 7 bytes).
    // We'll patch it once we know the final frame size.
    UNIT_Size prologue_offset = _UNIT_CodeBuffer_Reserve(&compile_context->buffer, 7);

    assert(translation != NULL);
    UNIT_Size blocks_size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size block_index = 0; block_index < blocks_size; ++block_index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, block_index);
        assert(block != NULL);
        UNIT_Size instructions_size = _UNIT_Vector_SIZE(&block->instructions);
        for (UNIT_Size index = 0; index < instructions_size; ++index) {
            _UNIT_MachineOperation *operation = _UNIT_Vector_GET(&block->instructions,
                                                                index);
            assert(operation != NULL);
            if (UNIT_FAILED(translate_operation(compile_context, operation))) {
                return UNIT_FAIL;
            }
        }
    }

    // Align frame size before patching anything
    UNIT_Context *ctx = compile_context->context;
    UNIT_Size frame_size = _UNIT_StackFrame_ComputeSize(&compile_context->stack_frame);
    if (frame_size > 0) {
        EMIT(add(ctx, reg(REG_RSP), immediate(frame_size)));
    }
    EMIT(ret(ctx));

    AMD64_PatchPrologue(compile_context, prologue_offset, frame_size);
    AMD64_PatchJumps(compile_context);
    return UNIT_OK;
}
