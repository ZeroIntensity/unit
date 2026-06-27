#include <stdio.h>
#include <stdlib.h>

#include <unit/platform.h>

#include <unit/internal/architectures.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

#include "aarch64_local.h"

static AARCH64_Operand
a64_reg(AARCH64_Register r) {
    return (AARCH64_Operand) {
        .kind = A64_OPERAND_REGISTER,
        .reg = r
    };
}

static AARCH64_Operand
a64_imm(int64_t value) {
    return (AARCH64_Operand) {
        .kind = A64_OPERAND_IMMEDIATE,
        .immediate = value
    };
}

static AARCH64_Operand
a64_stack(int64_t offset) {
    return (AARCH64_Operand) {
        .kind = A64_OPERAND_STACK,
        .immediate = offset
    };
}

static AARCH64_Operand
a64_indirect(AARCH64_Register r) {
    return (AARCH64_Operand) {
        .kind = A64_OPERAND_INDIRECT,
        .reg = r
    };
}

static AARCH64_Operand
a64_cond(AARCH64_Condition c) {
    return (AARCH64_Operand) {
        .kind = A64_OPERAND_CONDITION,
        .condition = c
    };
}

/* Map virtual registers to physical registers.
 * We use X9-X15 and X19 for 8 virtual registers.
 * X9-X15 are caller-saved temporaries.
 * X19 is callee-saved so we save/restore it around calls.
 * X16, X17 are scratch (IP0, IP1). */
static const AARCH64_Register register_map[] = {
    REG_X9,
    REG_X10,
    REG_X11,
    REG_X12,
    REG_X13,
    REG_X14,
    REG_X15,
    REG_X19,
};

/* Argument registers for AArch64: X0-X7 for all ABIs */
static const AARCH64_Register argument_registers[] = {
    REG_X0, REG_X1, REG_X2, REG_X3, REG_X4, REG_X5, REG_X6, REG_X7
};

static AARCH64_Operand
machine_item_to_operand(const _UNIT_MachineItem *machine_item)
{
    assert(machine_item != NULL);
    if (machine_item->type == _UNIT_TYPE_REGISTER) {
        return a64_reg(register_map[machine_item->value]);
    } else if (machine_item->type == _UNIT_TYPE_CONSTANT) {
        return a64_imm(machine_item->value);
    } else if (machine_item->type == _UNIT_TYPE_CALL_ARGS) {
        printf("cannot use call args as operand\n");
        abort();
    } else {
        /* Memory location => stack slot (offset from SP) */
        return a64_stack(machine_item->value * 8);
    }
}

static _UNIT_MachineItem *
generic_item_passthrough(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    return item;
}

#define ENSURE_VALID_ITEM(item)                                                         \
    _Generic((item),                                                                    \
             _UNIT_MachineItem*: generic_item_passthrough,                              \
             _UNIT_MachineDestination: _UNIT_MachineDestination_GetPointerNullable      \
            )(item)

/* Helper macros for creating instructions */
#define MAKE_INST_0(opcode_val) \
    make_instruction_0(compile_context->context, opcode_val)

#define MAKE_INST_1(opcode_val, a) \
    make_instruction_1(compile_context->context, opcode_val, a)

#define MAKE_INST_2(opcode_val, a, b) \
    make_instruction_2(compile_context->context, opcode_val, a, b)

#define MAKE_INST_3(opcode_val, a, b, c) \
    make_instruction_3(compile_context->context, opcode_val, a, b, c)

#define MAKE_INST_4(opcode_val, a, b, c, d) \
    make_instruction_4(compile_context->context, opcode_val, a, b, c, d)

static AARCH64_Instruction *
make_instruction_0(UNIT_Context *ctx, AARCH64_Opcode opcode)
{
    AARCH64_Instruction *inst = _UNIT_Alloc(ctx, sizeof(AARCH64_Instruction));
    if (inst == NULL) return NULL;
    inst->opcode = opcode;
    inst->operand_count = 0;
    return inst;
}

static AARCH64_Instruction *
make_instruction_1(UNIT_Context *ctx, AARCH64_Opcode opcode, AARCH64_Operand a)
{
    AARCH64_Instruction *inst = _UNIT_Alloc(ctx, sizeof(AARCH64_Instruction));
    if (inst == NULL) return NULL;
    inst->opcode = opcode;
    inst->operands[0] = a;
    inst->operand_count = 1;
    return inst;
}

static AARCH64_Instruction *
make_instruction_2(UNIT_Context *ctx, AARCH64_Opcode opcode,
                   AARCH64_Operand a, AARCH64_Operand b)
{
    AARCH64_Instruction *inst = _UNIT_Alloc(ctx, sizeof(AARCH64_Instruction));
    if (inst == NULL) return NULL;
    inst->opcode = opcode;
    inst->operands[0] = a;
    inst->operands[1] = b;
    inst->operand_count = 2;
    return inst;
}

static AARCH64_Instruction *
make_instruction_3(UNIT_Context *ctx, AARCH64_Opcode opcode,
                   AARCH64_Operand a, AARCH64_Operand b, AARCH64_Operand c)
{
    AARCH64_Instruction *inst = _UNIT_Alloc(ctx, sizeof(AARCH64_Instruction));
    if (inst == NULL) return NULL;
    inst->opcode = opcode;
    inst->operands[0] = a;
    inst->operands[1] = b;
    inst->operands[2] = c;
    inst->operand_count = 3;
    return inst;
}

static AARCH64_Instruction *
make_instruction_4(UNIT_Context *ctx, AARCH64_Opcode opcode,
                   AARCH64_Operand a, AARCH64_Operand b,
                   AARCH64_Operand c, AARCH64_Operand d)
{
    AARCH64_Instruction *inst = _UNIT_Alloc(ctx, sizeof(AARCH64_Instruction));
    if (inst == NULL) return NULL;
    inst->opcode = opcode;
    inst->operands[0] = a;
    inst->operands[1] = b;
    inst->operands[2] = c;
    inst->operands[3] = d;
    inst->operand_count = 4;
    return inst;
}

#define EMIT(op)                                                             \
    if (UNIT_FAILED(AARCH64_encode_instruction(compile_context, op))) {      \
        return _UNIT_FAIL;                                                   \
    }

/* Load a value from a stack slot into a register */
static UNIT_Status
emit_load_stack(_UNIT_CompileContext *compile_context, AARCH64_Register dest,
                UNIT_Size stack_offset)
{
    /* ldr Xd, [sp, #offset] */
    EMIT(MAKE_INST_2(AARCH64_LDR, a64_reg(dest), a64_stack(stack_offset)));
    return _UNIT_OK;
}

/* Store a register value to a stack slot */
static UNIT_Status
emit_store_stack(_UNIT_CompileContext *compile_context, AARCH64_Register src,
                 UNIT_Size stack_offset)
{
    /* str Xs, [sp, #offset] */
    EMIT(MAKE_INST_2(AARCH64_STR, a64_reg(src), a64_stack(stack_offset)));
    return _UNIT_OK;
}

/* Load an immediate value into a register, choosing the best encoding */
static UNIT_Status
emit_mov_imm(_UNIT_CompileContext *compile_context, AARCH64_Register dest,
             int64_t value)
{
    uint64_t uval = (uint64_t)value;
    if (uval <= 0xFFFF) {
        EMIT(MAKE_INST_2(AARCH64_MOV_IMM, a64_reg(dest), a64_imm(value)));
    } else {
        EMIT(MAKE_INST_2(AARCH64_MOV_WIDE, a64_reg(dest), a64_imm(value)));
    }
    return _UNIT_OK;
}

/* Ensure an operand is in a register. If it's a stack slot or immediate,
 * load it into the scratch register (X16). Returns the register it's in. */
static UNIT_Status
ensure_in_register(_UNIT_CompileContext *compile_context,
                   AARCH64_Operand operand, AARCH64_Register scratch,
                   AARCH64_Register *out_reg)
{
    if (operand.kind == A64_OPERAND_REGISTER) {
        *out_reg = operand.reg;
        return _UNIT_OK;
    } else if (operand.kind == A64_OPERAND_STACK) {
        if (UNIT_FAILED(emit_load_stack(compile_context, scratch,
                                        (UNIT_Size)operand.immediate))) {
            return _UNIT_FAIL;
        }
        *out_reg = scratch;
        return _UNIT_OK;
    } else if (operand.kind == A64_OPERAND_IMMEDIATE) {
        if (UNIT_FAILED(emit_mov_imm(compile_context, scratch,
                                     operand.immediate))) {
            return _UNIT_FAIL;
        }
        *out_reg = scratch;
        return _UNIT_OK;
    }
    _UNIT_Unreachable();
    return _UNIT_FAIL;
}

/* Write a register value back to the original operand location
 * if it was a stack slot */
static UNIT_Status
writeback_if_stack(_UNIT_CompileContext *compile_context,
                   AARCH64_Operand original, AARCH64_Register current_reg)
{
    if (original.kind == A64_OPERAND_STACK) {
        return emit_store_stack(compile_context, current_reg,
                                (UNIT_Size)original.immediate);
    }
    return _UNIT_OK;
}

static UNIT_Status
translate_operation(_UNIT_CompileContext *compile_context,
                    _UNIT_MachineOperation *operation,
                    UNIT_ABI abi,
                    _UNIT_SizeVector *epilogue_patches)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    assert(epilogue_patches != NULL);

#define OP(value) machine_item_to_operand(ENSURE_VALID_ITEM(operation->value))

    switch (operation->instruction) {

        case _UNIT_I_MOVE: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand src = OP(argument_1);

            AARCH64_Register src_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, src, REG_X16,
                                               &src_reg))) {
                return _UNIT_FAIL;
            }

            if (dst.kind == A64_OPERAND_REGISTER) {
                if (dst.reg != src_reg) {
                    EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(dst.reg),
                                     a64_reg(src_reg)));
                }
            } else if (dst.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, src_reg,
                                                 (UNIT_Size)dst.immediate))) {
                    return _UNIT_FAIL;
                }
            } else {
                _UNIT_Unreachable();
            }
            break;
        }

        case _UNIT_I_CALL_SYMBOL: {
            assert(operation->argument_2->type == _UNIT_TYPE_CALL_ARGS);
            _UNIT_Vector *arguments = operation->argument_2->call_args;
            UNIT_Size num_arguments = _UNIT_Vector_SIZE(arguments);
            assert(num_arguments <= 8);

            /* Save all virtual registers to stack slots before the call,
             * since X9-X15 are caller-saved. X19 is callee-saved. */
            UNIT_Size save_slots[8];
            AARCH64_Operand dst_op = OP(destination);
            for (UNIT_Size i = 0; i < 8; ++i) {
                AARCH64_Register saved_reg = register_map[i];
                /* Don't save our own destination register */
                if (dst_op.kind == A64_OPERAND_REGISTER
                    && saved_reg == dst_op.reg) {
                    save_slots[i] = (UNIT_Size)-1;
                    continue;
                }
                save_slots[i] = _UNIT_StackFrame_AllocateSlot(
                    &compile_context->stack_frame);
                if (UNIT_FAILED(emit_store_stack(compile_context, saved_reg,
                                                 save_slots[i]))) {
                    return _UNIT_FAIL;
                }
            }

            /* Move arguments into X0-X7, loading from save slots to
             * avoid circular dependency issues. */
            for (UNIT_Size arg = 0; arg < num_arguments; ++arg) {
                AARCH64_Register arg_reg = argument_registers[arg];
                _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(arguments, arg);
                AARCH64_Operand value = machine_item_to_operand(arg_item);

                /* If the value is in a virtual register that we saved,
                 * load from the save slot instead to avoid clobbered values. */
                if (value.kind == A64_OPERAND_REGISTER) {
                    for (UNIT_Size i = 0; i < 8; ++i) {
                        if (save_slots[i] != (UNIT_Size)-1
                            && register_map[i] == value.reg) {
                            value = a64_stack(save_slots[i]);
                            break;
                        }
                    }
                }

                AARCH64_Register src_reg;
                if (UNIT_FAILED(ensure_in_register(compile_context, value,
                                                   REG_X16, &src_reg))) {
                    return _UNIT_FAIL;
                }
                if (arg_reg != src_reg) {
                    EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(arg_reg),
                                     a64_reg(src_reg)));
                }
            }

            /* bl <symbol> */
            EMIT(MAKE_INST_1(AARCH64_BL_SYMBOL, OP(argument_1)));

            /* Move result from X0 to destination */
            if (dst_op.kind == A64_OPERAND_REGISTER) {
                if (dst_op.reg != REG_X0) {
                    EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(dst_op.reg),
                                     a64_reg(REG_X0)));
                }
            } else if (dst_op.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, REG_X0,
                                                 (UNIT_Size)dst_op.immediate))) {
                    return _UNIT_FAIL;
                }
            }

            /* Restore saved registers */
            for (UNIT_Size i = 0; i < 8; ++i) {
                if (save_slots[i] == (UNIT_Size)-1) continue;
                AARCH64_Register saved_reg = register_map[i];
                if (UNIT_FAILED(emit_load_stack(compile_context, saved_reg,
                                                save_slots[i]))) {
                    return _UNIT_FAIL;
                }
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame,
                                          save_slots[i]);
            }

            break;
        }

        case _UNIT_I_LOAD_STRING: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            /* LOAD_STRING pseudo-instruction: emits ADRP+ADD with relocation */
            EMIT(MAKE_INST_2(AARCH64_LOAD_STRING, a64_reg(dst_reg),
                             OP(argument_1)));

            if (dst.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, dst_reg,
                                                 (UNIT_Size)dst.immediate))) {
                    return _UNIT_FAIL;
                }
            }
            break;
        }

        case _UNIT_I_EXIT: {
            AARCH64_Operand exit_code = OP(destination);
            AARCH64_Register exit_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, exit_code,
                                               REG_X0, &exit_reg))) {
                return _UNIT_FAIL;
            }
            if (exit_reg != REG_X0) {
                EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(REG_X0),
                                 a64_reg(exit_reg)));
            }
            /* On AArch64 Linux: mov x8, #93 (exit); svc #0 */
            /* On Apple: mov x16, #1 (exit); svc #0x80 */
            if (abi == UNIT_ABI_APPLE) {
                if (UNIT_FAILED(emit_mov_imm(compile_context, REG_X16, 1))) {
                    return _UNIT_FAIL;
                }
                EMIT(MAKE_INST_1(AARCH64_SVC, a64_imm(0x80)));
            } else {
                if (UNIT_FAILED(emit_mov_imm(compile_context, REG_X8, 93))) {
                    return _UNIT_FAIL;
                }
                EMIT(MAKE_INST_1(AARCH64_SVC, a64_imm(0)));
            }
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            AARCH64_Operand src = OP(argument_1);
            AARCH64_Register src_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, src, REG_X16,
                                               &src_reg))) {
                return _UNIT_FAIL;
            }
            if (src_reg != REG_X0) {
                EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(REG_X0),
                                 a64_reg(src_reg)));
            }
            /* Reserve 16 bytes for the epilogue (4 instructions) */
            UNIT_Size patch_offset = _UNIT_CodeBuffer_Reserve(
                &compile_context->buffer, 16);
            if (UNIT_FAILED(_UNIT_SizeVector_Append(epilogue_patches,
                                                    patch_offset))) {
                return _UNIT_FAIL;
            }
            EMIT(MAKE_INST_0(AARCH64_RET));
            break;
        }

        case _UNIT_I_LOAD_ARGUMENT: {
            UNIT_Size arg_index = operation->argument_1->value;
            assert(arg_index < 8);
            AARCH64_Register arg_reg = argument_registers[arg_index];
            AARCH64_Operand dst = OP(destination);
            if (dst.kind == A64_OPERAND_REGISTER) {
                if (dst.reg != arg_reg) {
                    EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(dst.reg),
                                     a64_reg(arg_reg)));
                }
            } else if (dst.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, arg_reg,
                                                 (UNIT_Size)dst.immediate))) {
                    return _UNIT_FAIL;
                }
            }
            break;
        }

        case _UNIT_I_COMPARE_EQUAL: {
            AARCH64_Operand left = OP(argument_2);
            AARCH64_Operand right = OP(argument_1);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(MAKE_INST_2(AARCH64_CMP_IMM, a64_reg(left_reg), right));
            } else {
                AARCH64_Register right_reg;
                if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                                   REG_X17, &right_reg))) {
                    return _UNIT_FAIL;
                }
                EMIT(MAKE_INST_2(AARCH64_CMP_REG, a64_reg(left_reg),
                                 a64_reg(right_reg)));
            }
            break;
        }

        case _UNIT_I_JUMP: {
            AARCH64_Operand target = OP(argument_1);
            EMIT(MAKE_INST_1(AARCH64_B, target));
            break;
        }

        case _UNIT_I_JUMP_LABEL: {
            AARCH64_Operand label = OP(destination);
            EMIT(MAKE_INST_1(AARCH64_B_LABEL, label));
            break;
        }

#define JUMP_CONDITION(inst, cond_code)                                     \
        case inst: {                                                        \
            AARCH64_Operand left = OP(argument_1);                          \
            AARCH64_Operand right = OP(argument_2);                         \
            AARCH64_Register left_reg;                                      \
            if (UNIT_FAILED(ensure_in_register(compile_context, left,       \
                                               REG_X16, &left_reg))) {     \
                return _UNIT_FAIL;                                          \
            }                                                               \
            if (right.kind == A64_OPERAND_IMMEDIATE                         \
                && right.immediate >= 0 && right.immediate < 4096) {        \
                EMIT(MAKE_INST_2(AARCH64_CMP_IMM, a64_reg(left_reg),       \
                                 right));                                   \
            } else {                                                        \
                AARCH64_Register right_reg;                                  \
                if (UNIT_FAILED(ensure_in_register(compile_context, right,  \
                                                   REG_X17, &right_reg))) {\
                    return _UNIT_FAIL;                                       \
                }                                                           \
                EMIT(MAKE_INST_2(AARCH64_CMP_REG, a64_reg(left_reg),       \
                                 a64_reg(right_reg)));                      \
            }                                                               \
            EMIT(MAKE_INST_2(AARCH64_B_COND, OP(destination),               \
                             a64_cond(cond_code)));                         \
            break;                                                          \
        }

        JUMP_CONDITION(_UNIT_I_JUMP_IF_EQUAL, AARCH64_COND_EQ)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_NOT_EQUAL, AARCH64_COND_NE)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS, AARCH64_COND_LT)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS_EQUAL, AARCH64_COND_LE)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER, AARCH64_COND_GT)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER_EQUAL, AARCH64_COND_GE)

#undef JUMP_CONDITION

        case _UNIT_I_ADD: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand left = OP(argument_1);
            AARCH64_Operand right = OP(argument_2);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(MAKE_INST_3(AARCH64_ADD_IMM, a64_reg(dst_reg),
                                 a64_reg(left_reg), right));
            } else {
                AARCH64_Register right_reg;
                if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                                   REG_X17, &right_reg))) {
                    return _UNIT_FAIL;
                }
                EMIT(MAKE_INST_3(AARCH64_ADD_REG, a64_reg(dst_reg),
                                 a64_reg(left_reg), a64_reg(right_reg)));
            }

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_SUB: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand left = OP(argument_1);
            AARCH64_Operand right = OP(argument_2);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(MAKE_INST_3(AARCH64_SUB_IMM, a64_reg(dst_reg),
                                 a64_reg(left_reg), right));
            } else {
                AARCH64_Register right_reg;
                if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                                   REG_X17, &right_reg))) {
                    return _UNIT_FAIL;
                }
                EMIT(MAKE_INST_3(AARCH64_SUB_REG, a64_reg(dst_reg),
                                 a64_reg(left_reg), a64_reg(right_reg)));
            }

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_MUL: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand left = OP(argument_1);
            AARCH64_Operand right = OP(argument_2);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }
            AARCH64_Register right_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                               REG_X17, &right_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(MAKE_INST_3(AARCH64_MUL, a64_reg(dst_reg),
                             a64_reg(left_reg), a64_reg(right_reg)));

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_DIV: {
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand left = OP(argument_1);
            AARCH64_Operand right = OP(argument_2);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }
            AARCH64_Register right_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                               REG_X17, &right_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(MAKE_INST_3(AARCH64_SDIV, a64_reg(dst_reg),
                             a64_reg(left_reg), a64_reg(right_reg)));

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_MOD: {
            /* remainder = dividend - (dividend / divisor) * divisor
             * AArch64: SDIV + MSUB */
            AARCH64_Operand dst = OP(destination);
            AARCH64_Operand left = OP(argument_1);
            AARCH64_Operand right = OP(argument_2);

            AARCH64_Register left_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, left,
                                               REG_X16, &left_reg))) {
                return _UNIT_FAIL;
            }
            AARCH64_Register right_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, right,
                                               REG_X17, &right_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            /* Use X8 as temp for quotient (it's a temp register) */
            EMIT(MAKE_INST_3(AARCH64_SDIV, a64_reg(REG_X8),
                             a64_reg(left_reg), a64_reg(right_reg)));
            /* MSUB dst, quotient, divisor, dividend => dividend - quotient*divisor */
            EMIT(MAKE_INST_4(AARCH64_MSUB, a64_reg(dst_reg),
                             a64_reg(REG_X8), a64_reg(right_reg),
                             a64_reg(left_reg)));

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_CONVERT: {
            AARCH64_Operand src = OP(argument_1);
            UNIT_IntegerType target = operation->argument_2->value;

            AARCH64_Register src_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, src,
                                               REG_X16, &src_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Operand dst = OP(destination);
            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            switch (target) {
                case UNIT_TYPE_UINT8:
                    EMIT(MAKE_INST_2(AARCH64_UXTB, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT8:
                    EMIT(MAKE_INST_2(AARCH64_SXTB, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT16:
                    EMIT(MAKE_INST_2(AARCH64_UXTH, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT16:
                    EMIT(MAKE_INST_2(AARCH64_SXTH, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT32:
                    EMIT(MAKE_INST_2(AARCH64_UXTW, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT32:
                    EMIT(MAKE_INST_2(AARCH64_SXTW, a64_reg(dst_reg),
                                     a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT64:
                case UNIT_TYPE_INT64:
                    if (dst_reg != src_reg) {
                        EMIT(MAKE_INST_2(AARCH64_MOV_REG, a64_reg(dst_reg),
                                         a64_reg(src_reg)));
                    }
                    break;
            }

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_READ_BYTES: {
            AARCH64_Operand addr = OP(argument_1);
            UNIT_Size size = operation->argument_2->value;

            AARCH64_Register addr_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, addr,
                                               REG_X16, &addr_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Operand dst = OP(destination);
            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X17;
            }

            switch (size) {
                case 1:
                    EMIT(MAKE_INST_2(AARCH64_LDRB, a64_reg(dst_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 2:
                    EMIT(MAKE_INST_2(AARCH64_LDRH, a64_reg(dst_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 4:
                    EMIT(MAKE_INST_2(AARCH64_LDRW, a64_reg(dst_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 8:
                    EMIT(MAKE_INST_2(AARCH64_LDR, a64_reg(dst_reg),
                                     a64_indirect(addr_reg)));
                    break;
                default:
                    _UNIT_Unreachable();
            }

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }

        case _UNIT_I_WRITE_BYTES: {
            AARCH64_Operand addr = OP(destination);
            AARCH64_Operand value = OP(argument_1);
            UNIT_Size size = operation->argument_2->value;

            AARCH64_Register addr_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, addr,
                                               REG_X16, &addr_reg))) {
                return _UNIT_FAIL;
            }

            AARCH64_Register val_reg;
            if (UNIT_FAILED(ensure_in_register(compile_context, value,
                                               REG_X17, &val_reg))) {
                return _UNIT_FAIL;
            }

            switch (size) {
                case 1:
                    EMIT(MAKE_INST_2(AARCH64_STRB, a64_reg(val_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 2:
                    EMIT(MAKE_INST_2(AARCH64_STRH, a64_reg(val_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 4:
                    EMIT(MAKE_INST_2(AARCH64_STRW, a64_reg(val_reg),
                                     a64_reg(addr_reg)));
                    break;
                case 8:
                    EMIT(MAKE_INST_2(AARCH64_STR, a64_reg(val_reg),
                                     a64_indirect(addr_reg)));
                    break;
                default:
                    _UNIT_Unreachable();
            }
            break;
        }

        case _UNIT_I_ADDRESS_OF: {
            AARCH64_Operand src = OP(argument_1);
            AARCH64_Operand dst = OP(destination);
            assert(src.kind == A64_OPERAND_STACK);

            AARCH64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            /* add Xd, sp, #offset */
            EMIT(MAKE_INST_2(AARCH64_ADD_SP_IMM, a64_reg(dst_reg), src));

            if (UNIT_FAILED(writeback_if_stack(compile_context, dst, dst_reg))) {
                return _UNIT_FAIL;
            }
            break;
        }
    }

    return _UNIT_OK;
#undef OP
}

static void
patch_epilogues(_UNIT_CompileContext *compile_context,
                _UNIT_SizeVector *epilogue_patches,
                UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(epilogue_patches != NULL);
    UNIT_Size size = _UNIT_SizeVector_SIZE(epilogue_patches);
    for (UNIT_Size index = 0; index < size; ++index) {
        UNIT_Size epilogue_offset = _UNIT_SizeVector_GET(epilogue_patches, index);
        AARCH64_PatchEpilogue(compile_context, epilogue_offset, frame_size);
    }
}

UNIT_Status
_UNIT_AARCH64_Compile(_UNIT_Translation *translation,
                      _UNIT_CompileContext *compile_context,
                      UNIT_ABI abi)
{
    /* Reserve 16 bytes for the prologue (4 ARM64 instructions):
     * stp x29, x30, [sp, #-frame]!
     * mov x29, sp
     * sub sp, sp, #extra  (or NOP)
     * NOP */
    UNIT_Size prologue_offset = _UNIT_CodeBuffer_Reserve(&compile_context->buffer, 16);

    _UNIT_SizeVector epilogue_patches;
    if (UNIT_FAILED(_UNIT_SizeVector_Init(&epilogue_patches,
                                          compile_context->context, 4))) {
        return _UNIT_FAIL;
    }

    assert(translation != NULL);
    UNIT_Size blocks_size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size block_index = 0; block_index < blocks_size; ++block_index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks,
                                                    block_index);
        assert(block != NULL);
        UNIT_Size instructions_size = _UNIT_Vector_SIZE(&block->instructions);
        for (UNIT_Size index = 0; index < instructions_size; ++index) {
            _UNIT_MachineOperation *operation = _UNIT_Vector_GET(
                &block->instructions, index);
            assert(operation != NULL);
            if (UNIT_FAILED(translate_operation(compile_context, operation,
                                               abi, &epilogue_patches))) {
                _UNIT_SizeVector_Clear(&epilogue_patches);
                return _UNIT_FAIL;
            }
        }
    }

    UNIT_Size frame_size = _UNIT_StackFrame_ComputeSize(&compile_context->stack_frame);
    AARCH64_PatchPrologue(compile_context, prologue_offset, frame_size);
    patch_epilogues(compile_context, &epilogue_patches, frame_size);
    AARCH64_PatchJumps(compile_context);

    _UNIT_SizeVector_Clear(&epilogue_patches);
    return _UNIT_OK;
}
