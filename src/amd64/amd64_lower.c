#include <stdio.h>
#include <stdlib.h>

#include <unit/platform.h>

#include <unit/internal/basic_block.h>
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

AMD64_Operand
indirect(AMD64_Operand operand) {
    assert(operand.kind == OPERAND_REGISTER);
    return (AMD64_Operand) {
        .kind = OPERAND_INDIRECT,
        .reg = operand.reg
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

static const AMD64_Register *
get_argument_registers(UNIT_ABI abi)
{
    static const AMD64_Register systemv[] = {
        REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
    };
    static const AMD64_Register win64[] = {
        REG_RCX, REG_RDX, REG_R8, REG_R9
    };

    switch (abi) {
        case UNIT_ABI_SYSTEMV:
            return systemv;
        case UNIT_ABI_WIN64:
            return win64;
        default: _UNIT_Unreachable();
    }
}

AMD64_Operand
machine_item_to_operand(const _UNIT_MachineItem *machine_item)
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

#define ONE_ARG_HELPER(name, opcode_name)                                                       \
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


#define TWO_ARG_HELPER(name, opcode_name)                                                       \
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

#define THREE_ARG_HELPER(name, opcode_name)                                                     \
    static inline AMD64_Instruction *                                                           \
    name(UNIT_Context *context, AMD64_Operand dst, AMD64_Operand a, AMD64_Operand b) {          \
        AMD64_Instruction *instruction = _UNIT_Alloc(context,                                   \
                                                     sizeof(AMD64_Instruction));                \
        if (instruction == NULL) {                                                              \
            return NULL;                                                                        \
        }                                                                                       \
        instruction->opcode = opcode_name;                                                      \
        instruction->operands[0] = dst;                                                         \
        instruction->operands[1] = a;                                                           \
        instruction->operands[2] = b;                                                           \
        instruction->operand_count = 3;                                                         \
        return instruction;                                                                     \
    }

NO_ARGS_HELPER(ret, AMD64_RET)
NO_ARGS_HELPER(syscall, AMD64_SYSCALL)
NO_ARGS_HELPER(cqo, AMD64_CQO)
ONE_ARG_HELPER(call_indirect, AMD64_CALL_INDIRECT)
ONE_ARG_HELPER(call_symbol, AMD64_CALL_SYMBOL)
ONE_ARG_HELPER(jmp, AMD64_JUMP)
ONE_ARG_HELPER(_jmp_label, AMD64_JUMP_LABEL)
ONE_ARG_HELPER(jump_if_equal, AMD64_JUMP_IF_EQUAL)
ONE_ARG_HELPER(jump_if_not_equal, AMD64_JUMP_IF_NOT_EQUAL)
ONE_ARG_HELPER(jump_if_greater, AMD64_JUMP_IF_GREATER)
ONE_ARG_HELPER(jump_if_less, AMD64_JUMP_IF_LESS)
ONE_ARG_HELPER(jump_if_greater_equal, AMD64_JUMP_IF_GREATER_EQUAL)
ONE_ARG_HELPER(jump_if_less_equal, AMD64_JUMP_IF_LESS_EQUAL)
TWO_ARG_HELPER(mov, AMD64_MOV)
THREE_ARG_HELPER(movzx, AMD64_MOVZX)
THREE_ARG_HELPER(mov_sized, AMD64_MOV_SIZED)
TWO_ARG_HELPER(load_string, AMD64_LOAD_STRING)
TWO_ARG_HELPER(cmp, AMD64_COMPARE)
TWO_ARG_HELPER(lea, AMD64_LOAD_ADDRESS)
TWO_ARG_HELPER(add, AMD64_ADD)
TWO_ARG_HELPER(sub, AMD64_SUB)
TWO_ARG_HELPER(imul, AMD64_MUL)
ONE_ARG_HELPER(idiv, AMD64_DIV)

#define EMIT(op)                                                             \
    if (UNIT_FAILED(AMD64_encode_instruction(compile_context, op))) {        \
        return _UNIT_FAIL;                                                    \
    }

static UNIT_Status
ensure_register(_UNIT_CompileContext *compile_context,
                _UNIT_MachineItem *item, AMD64_Operand *out_operand)
{
    assert(compile_context != NULL);
    assert(item != NULL);
    assert(out_operand != NULL);
    AMD64_Operand in_operand = machine_item_to_operand(item);
    if (in_operand.kind == OPERAND_REGISTER) {
        *out_operand = in_operand;
        return _UNIT_OK;
    }

    EMIT(mov(compile_context->context, reg(REG_R11), in_operand));
    *out_operand = reg(REG_R11);
    return _UNIT_OK;
}

static UNIT_Status
flush_register(_UNIT_CompileContext *compile_context,
               _UNIT_MachineItem *item,
               AMD64_Operand actual)
{
    AMD64_Operand original = machine_item_to_operand(item);
    if (original.kind != OPERAND_REGISTER && original.kind != OPERAND_IMMEDIATE) {
        EMIT(mov(compile_context->context, original, actual));
    }

    return _UNIT_OK;
}

static UNIT_Status
preserve_register(_UNIT_CompileContext *compile_context,
                  _UNIT_MachineOperation *operation, AMD64_Register to_preserve,
                  UNIT_Size *slot_ptr)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    assert(slot_ptr != NULL);
#define IGNORE_IF_TARGET(name)                                      \
    if (operation->name != NULL                                     \
        && operation->name->type == _UNIT_TYPE_REGISTER             \
        && register_map[operation->name->value] == to_preserve) {   \
        *slot_ptr = -1;                                             \
        return _UNIT_OK;                                             \
    }

    IGNORE_IF_TARGET(destination);
    IGNORE_IF_TARGET(argument_1);
    IGNORE_IF_TARGET(argument_2);

    UNIT_Size slot = _UNIT_StackFrame_AllocateSlot(&compile_context->stack_frame);
    EMIT(mov(compile_context->context, stack_slot(slot), reg(to_preserve)));
    *slot_ptr = slot;

    return _UNIT_OK;

#undef IGNORE_IF_TARGET
}

static UNIT_Status
restore_register(_UNIT_CompileContext *compile_context, AMD64_Register preserved,
                 UNIT_Size slot)
{
    assert(compile_context != NULL);
    if (slot == -1) {
        return _UNIT_OK;
    }

    EMIT(mov(compile_context->context, reg(preserved), stack_slot(slot)));
    _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, slot);
    return _UNIT_OK;
}

static UNIT_Status
translate_operation(_UNIT_CompileContext *compile_context,
                    _UNIT_MachineOperation *operation,
                    UNIT_ABI abi)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    UNIT_Context *ctx = compile_context->context;

#define OP(value) machine_item_to_operand(operation->value)

#define ENSURE_IN_REGISTER(name)                                                            \
    AMD64_Operand name;                                                                     \
    if (UNIT_FAILED(ensure_register(compile_context, operation->name, &name))) {            \
        return _UNIT_FAIL;                                                                   \
    }

#define FLUSH_REGISTER(name)                                                        \
    if (UNIT_FAILED(flush_register(compile_context, operation->name, name))) {      \
        return _UNIT_FAIL;                                                           \
    }

#define PRESERVE_REGISTER(name)                                                             \
    UNIT_Size slot_ ##name;                                                                 \
    if (UNIT_FAILED(preserve_register(compile_context, operation, name, &slot_ ##name))) {  \
        return _UNIT_FAIL;                                                                   \
    }

#define RESTORE_REGISTER(name)                                                              \
    if (UNIT_FAILED(restore_register(compile_context, name, slot_ ##name))) {               \
        return _UNIT_FAIL;                                                                   \
    }

    switch (operation->instruction) {
        case _UNIT_I_MOVE: {
            AMD64_Operand dst = OP(destination);
            AMD64_Operand src = OP(argument_1);
            assert(dst.kind != OPERAND_IMMEDIATE);

            if (dst.kind == OPERAND_STACK && src.kind == OPERAND_STACK) {
                ENSURE_IN_REGISTER(argument_1);
                EMIT(mov(ctx, dst, argument_1));
                FLUSH_REGISTER(argument_1);
            } else if (dst.kind == OPERAND_STACK && src.kind == OPERAND_IMMEDIATE) {
                EMIT(mov(ctx, reg(REG_R11), src));
                EMIT(mov(ctx, dst, reg(REG_R11)));
            } else {
                EMIT(mov(ctx, dst, src));
            }
            break;
        }

        case _UNIT_I_CALL_SYMBOL: {
            assert(operation->argument_2->type == _UNIT_TYPE_CALL_ARGS);
            _UNIT_Vector *arguments = operation->argument_2->call_args;
            UNIT_Size num_arguments = _UNIT_Vector_SIZE(arguments);
            assert(num_arguments <= 6);

            PRESERVE_REGISTER(REG_RAX);
            const AMD64_Register *argument_registers = get_argument_registers(abi);

            // Save argument registers into stack frame slots
            UNIT_Size save_slots[8];
            for (UNIT_Size index = 0; index < 8; ++index) {
                AMD64_Register saved_register = register_map[index];
                // We don't want to preserve registers that are our own target
                if (OP(destination).kind == OPERAND_REGISTER
                    && saved_register == OP(destination).reg) {
                    save_slots[index] = -1;
                    continue;
                }

                save_slots[index] = _UNIT_StackFrame_AllocateSlot(&compile_context->stack_frame);
                assert(save_slots[index] % 8 == 0);
                EMIT(mov(ctx, stack_slot(save_slots[index]), reg(saved_register)));
            }

            // TODO: Handle when there are more than six args
            for (UNIT_Size argument = 0; argument < num_arguments; ++argument) {
                AMD64_Register argument_register = argument_registers[argument];
                _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(arguments, argument);
                AMD64_Operand value = machine_item_to_operand(arg_item);

                // We load from the save slots in order to avoid some circular
                // dependency issues.
                // For example, RDI wants to go to RSI, and RSI wants to go to
                // RDI. If you did a sequential mov rdi, rsi and mov rsi, rdi,
                // then RDI would be clobbered before the second mov.
                if (value.kind == OPERAND_REGISTER) {
                    for (UNIT_Size index = 0; index < 8; ++index) {
                        if (save_slots[index] != (UNIT_Size)-1
                            && register_map[index] == value.reg) {
                            value = stack_slot(save_slots[index]);
                            break;
                        }
                    }
                }

                EMIT(mov(ctx, reg(argument_register), value));
            }

            EMIT(mov(ctx, reg(REG_RAX), immediate(0)));
            EMIT(call_symbol(ctx, OP(argument_1)));
            EMIT(mov(ctx, OP(destination), reg(REG_RAX)));

            for (UNIT_Size index = 0; index < 8; ++index) {
                if (save_slots[index] == -1) {
                    continue;
                }
                AMD64_Register saved_register = register_map[index];
                EMIT(mov(ctx, reg(saved_register), stack_slot(save_slots[index])));
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, save_slots[index]);
            }

            RESTORE_REGISTER(REG_RAX);

            break;
        }

        case _UNIT_I_EXIT: {
            EMIT(mov(ctx, reg(REG_RAX), immediate(60))); // syscall number for exit
            EMIT(mov(ctx, reg(REG_RDI), OP(destination)));
            EMIT(syscall(ctx));
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            EMIT(mov(ctx, reg(REG_RAX), OP(argument_1)));
            // ret() will be emitted after the loop
            break;
        }

        case _UNIT_I_JUMP: {
            EMIT(jmp(ctx, OP(argument_1)));
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

        #define JUMP_CONDITION(inst, op)                        \
            case inst: {                                        \
                AMD64_Operand left = OP(argument_1);            \
                AMD64_Operand right = OP(argument_2);           \
                /* cmp needs at least one register operand */   \
                if (left.kind != OPERAND_REGISTER) {            \
                    ENSURE_IN_REGISTER(argument_1);             \
                    left = argument_1;                          \
                }                                               \
                EMIT(cmp(ctx, left, right));                    \
                EMIT(op(ctx, OP(destination)));                 \
                break;                                          \
            }

        JUMP_CONDITION(_UNIT_I_JUMP_IF_EQUAL, jump_if_equal);
        JUMP_CONDITION(_UNIT_I_JUMP_IF_NOT_EQUAL, jump_if_not_equal);
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS, jump_if_less);
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS_EQUAL, jump_if_less_equal);
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER, jump_if_greater);
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER_EQUAL, jump_if_greater_equal);

        #undef JUMP_CONDITION

        case _UNIT_I_LOAD_STRING: {
            ENSURE_IN_REGISTER(destination);
            EMIT(load_string(ctx, destination, OP(argument_1)));
            FLUSH_REGISTER(destination);
            break;
        }

        case _UNIT_I_ADDRESS_OF: {
            EMIT(lea(ctx, OP(destination), OP(argument_1)));
            break;
        }

        #define BINARY_OP(inst, helper)                                                 \
            case inst: {                                                                \
                ENSURE_IN_REGISTER(destination);                                        \
                EMIT(mov(ctx, destination, OP(argument_1)));                            \
                EMIT(helper(ctx, destination, OP(argument_2)));                         \
                FLUSH_REGISTER(destination);                                            \
                break;                                                                  \
            }

        BINARY_OP(_UNIT_I_ADD, add);
        BINARY_OP(_UNIT_I_SUB, sub);
        BINARY_OP(_UNIT_I_MUL, imul);

        case _UNIT_I_DIV:
        case _UNIT_I_MOD: {
            AMD64_Operand dst = OP(destination);
            AMD64_Operand left = OP(argument_1);
            AMD64_Operand right = OP(argument_2);
            _UNIT_StackFrame *stack_frame = &compile_context->stack_frame;

            PRESERVE_REGISTER(REG_RAX);
            PRESERVE_REGISTER(REG_RDX);

            EMIT(mov(ctx, reg(REG_RAX), left));
            EMIT(cqo(ctx));
            ENSURE_IN_REGISTER(argument_2);
            EMIT(idiv(ctx, argument_2));
            FLUSH_REGISTER(argument_2);

            if (operation->instruction == _UNIT_I_MOD) {
                EMIT(mov(ctx, dst, reg(REG_RDX)));
            } else {
                EMIT(mov(ctx, dst, reg(REG_RAX)));
            }

            RESTORE_REGISTER(REG_RDX);
            RESTORE_REGISTER(REG_RAX);
            break;
        }

        case _UNIT_I_READ_BYTES: {
            ENSURE_IN_REGISTER(argument_1);
            UNIT_Size size = operation->argument_2->value;

            if (size == 8) {
                EMIT(mov(ctx, reg(REG_R11), indirect(argument_1)));
            } else {
                EMIT(movzx(ctx, reg(REG_R11), indirect(argument_1), immediate(size)));
            }
            EMIT(mov(ctx, OP(destination), reg(REG_R11)));
            break;
        }

        case _UNIT_I_WRITE_BYTES: {
            ENSURE_IN_REGISTER(destination);
            AMD64_Operand addr = destination;
            UNIT_Size size = operation->argument_2->value;

            if (OP(argument_1).kind == OPERAND_REGISTER
                || OP(argument_1).kind == OPERAND_IMMEDIATE) {
                EMIT(mov_sized(ctx, indirect(addr), OP(argument_1), immediate(size)));
            } else {
                UNIT_Size slot = _UNIT_StackFrame_AllocateSlot(
                    &compile_context->stack_frame);
                EMIT(mov(ctx, stack_slot(slot), addr));
                EMIT(mov(ctx, reg(REG_R11), OP(argument_1)));
                EMIT(mov(ctx, reg(addr.reg), stack_slot(slot)));
                EMIT(mov_sized(ctx, indirect(reg(addr.reg)), reg(REG_R11), immediate(size)));
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, slot);
            }
            break;
        }

        case _UNIT_I_LOAD_ARGUMENT: {
            const AMD64_Register *argument_registers = get_argument_registers(abi);
            UNIT_Size arg_index = operation->argument_1->value;
            // TODO: Handle more than 6 args
            assert(arg_index < 6);
            AMD64_Register argument_register = argument_registers[arg_index];
            EMIT(mov(ctx, OP(destination), reg(argument_register)));
            break;
        }
    }

    return _UNIT_OK;
#undef OP
}

UNIT_Status
_UNIT_AMD64_Compile(_UNIT_Translation *translation,
                    _UNIT_CompileContext *compile_context,
                    UNIT_ABI abi)
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
            if (UNIT_FAILED(translate_operation(compile_context, operation, abi))) {
                return _UNIT_FAIL;
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
    return _UNIT_OK;
}
