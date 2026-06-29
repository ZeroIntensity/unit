#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unit/platform.h>

#include <unit/internal/architectures.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

#include "aarch64_local.h"

/* ── Operand constructors ───────────────────────────────────────────── */

static AArch64_Operand
a64_reg(AArch64_Register r)
{
    return (AArch64_Operand) {
        .kind = A64_OPERAND_REGISTER,
        .reg = r
    };
}

static AArch64_Operand
a64_imm(int64_t value)
{
    return (AArch64_Operand) {
        .kind = A64_OPERAND_IMMEDIATE,
        .immediate = value
    };
}

static AArch64_Operand
a64_stack(int64_t offset)
{
    return (AArch64_Operand) {
        .kind = A64_OPERAND_STACK,
        .immediate = offset
    };
}

static AArch64_Operand
a64_indirect(AArch64_Register r)
{
    return (AArch64_Operand) {
        .kind = A64_OPERAND_INDIRECT,
        .reg = r
    };
}

static AArch64_Operand
a64_cond(AArch64_Condition c)
{
    return (AArch64_Operand) {
        .kind = A64_OPERAND_CONDITION,
        .condition = c
    };
}

/* ── Register mapping ───────────────────────────────────────────────── */

/* Map virtual registers to physical registers.
 * X9-X15: caller-saved temporaries (7 regs)
 * X19-X28: callee-saved (10 regs, saved/restored around calls)
 * X16, X17 are reserved as scratch (IP0, IP1).
 * X18 is platform-reserved (off-limits on Apple). */
static const AArch64_Register register_map[] = {
    REG_X9,
    REG_X10,
    REG_X11,
    REG_X12,
    REG_X13,
    REG_X14,
    REG_X15,
    REG_X19,
    REG_X20,
    REG_X21,
    REG_X22,
    REG_X23,
    REG_X24,
    REG_X25,
    REG_X26,
    REG_X27,
    REG_X28,
};

#define NUM_VIRTUAL_REGISTERS \
    ((UNIT_Size)(sizeof(register_map) / sizeof(register_map[0])))

/* Argument registers for AArch64: X0-X7 for all ABIs */
static const AArch64_Register argument_registers[] = {
    REG_X0, REG_X1, REG_X2, REG_X3, REG_X4, REG_X5, REG_X6, REG_X7
};

static int8_t
apple_variadic_fixed_arg_count(const char *name, UNIT_Size *count)
{
    assert(count != NULL);
    if (name == NULL) {
        return 0;
    }
    if (strcmp(name, "printf") == 0 || strcmp(name, "scanf") == 0) {
        *count = 1;
        return 1;
    }
    if (strcmp(name, "sscanf") == 0) {
        *count = 2;
        return 1;
    }
    return 0;
}

static UNIT_Size
outgoing_variadic_arg_slots(_UNIT_MachineOperation *operation, UNIT_ABI abi)
{
    assert(operation != NULL);
    if (abi != UNIT_ABI_APPLE || operation->instruction != _UNIT_I_CALL_SYMBOL) {
        return 0;
    }
    assert(operation->argument_2 != NULL);
    assert(operation->argument_2->type == _UNIT_TYPE_CALL_ARGS);

    UNIT_Size fixed_count;
    if (!apple_variadic_fixed_arg_count(operation->argument_1->hint, &fixed_count)) {
        return 0;
    }

    UNIT_Size arg_count = _UNIT_Vector_SIZE(operation->argument_2->call_args);
    if (arg_count <= fixed_count) {
        return 0;
    }

    return arg_count - fixed_count;
}

static UNIT_Size
max_outgoing_variadic_arg_slots(_UNIT_Translation *translation, UNIT_ABI abi)
{
    assert(translation != NULL);
    UNIT_Size max_slots = 0;
    UNIT_Size block_count = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size block_index = 0; block_index < block_count; ++block_index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks,
                                                    block_index);
        UNIT_Size instruction_count = _UNIT_Vector_SIZE(&block->instructions);
        for (UNIT_Size index = 0; index < instruction_count; ++index) {
            _UNIT_MachineOperation *operation = _UNIT_Vector_GET(
                &block->instructions, index);
            UNIT_Size slots = outgoing_variadic_arg_slots(operation, abi);
            if (slots > max_slots) {
                max_slots = slots;
            }
        }
    }
    return max_slots;
}

/* ── Machine item to operand conversion ─────────────────────────────── */

static AArch64_Operand
machine_item_to_operand(_UNIT_CompileContext *compile_context,
                        const _UNIT_MachineItem *machine_item)
{
    assert(compile_context != NULL);
    assert(machine_item != NULL);
    if (machine_item->type == _UNIT_TYPE_REGISTER) {
        assert(machine_item->value < NUM_VIRTUAL_REGISTERS);
        return a64_reg(register_map[machine_item->value]);
    } else if (machine_item->type == _UNIT_TYPE_CONSTANT) {
        return a64_imm(machine_item->value);
    } else if (machine_item->type == _UNIT_TYPE_CALL_ARGS) {
        printf("cannot use call args as operand\n");
        abort();
    } else {
        assert(machine_item->type == _UNIT_TYPE_MEMORY);
        return a64_stack((machine_item->value
                          + compile_context->stack_frame.slot_offset) * 8);
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

/* ── Per-opcode instruction builders (AMD64-style) ──────────────────── */

#define NO_ARGS_HELPER(name, opcode_name)                                               \
    static inline AArch64_Instruction *                                                 \
    name(UNIT_Context *context) {                                                       \
        assert(context != NULL);                                                        \
        AArch64_Instruction *instruction = _UNIT_Alloc(context,                         \
                                                       sizeof(AArch64_Instruction));    \
        if (instruction == NULL) {                                                      \
            return NULL;                                                                \
        }                                                                               \
        instruction->opcode = opcode_name;                                              \
        instruction->operand_count = 0;                                                 \
        return instruction;                                                             \
    }

#define ONE_ARG_HELPER(name, opcode_name)                                               \
    static inline AArch64_Instruction *                                                 \
    name(UNIT_Context *context, AArch64_Operand a) {                                    \
        assert(context != NULL);                                                        \
        AArch64_Instruction *instruction = _UNIT_Alloc(context,                         \
                                                       sizeof(AArch64_Instruction));    \
        if (instruction == NULL) {                                                      \
            return NULL;                                                                \
        }                                                                               \
        instruction->opcode = opcode_name;                                              \
        instruction->operands[0] = a;                                                   \
        instruction->operand_count = 1;                                                 \
        return instruction;                                                             \
    }

#define TWO_ARG_HELPER(name, opcode_name)                                               \
    static inline AArch64_Instruction *                                                 \
    name(UNIT_Context *context, AArch64_Operand a, AArch64_Operand b) {                 \
        assert(context != NULL);                                                        \
        AArch64_Instruction *instruction = _UNIT_Alloc(context,                         \
                                                       sizeof(AArch64_Instruction));    \
        if (instruction == NULL) {                                                      \
            return NULL;                                                                \
        }                                                                               \
        instruction->opcode = opcode_name;                                              \
        instruction->operands[0] = a;                                                   \
        instruction->operands[1] = b;                                                   \
        instruction->operand_count = 2;                                                 \
        return instruction;                                                             \
    }

#define THREE_ARG_HELPER(name, opcode_name)                                             \
    static inline AArch64_Instruction *                                                 \
    name(UNIT_Context *context, AArch64_Operand a, AArch64_Operand b,                   \
         AArch64_Operand c) {                                                           \
        assert(context != NULL);                                                        \
        AArch64_Instruction *instruction = _UNIT_Alloc(context,                         \
                                                       sizeof(AArch64_Instruction));    \
        if (instruction == NULL) {                                                      \
            return NULL;                                                                \
        }                                                                               \
        instruction->opcode = opcode_name;                                              \
        instruction->operands[0] = a;                                                   \
        instruction->operands[1] = b;                                                   \
        instruction->operands[2] = c;                                                   \
        instruction->operand_count = 3;                                                 \
        return instruction;                                                             \
    }

#define FOUR_ARG_HELPER(name, opcode_name)                                              \
    static inline AArch64_Instruction *                                                 \
    name(UNIT_Context *context, AArch64_Operand a, AArch64_Operand b,                   \
         AArch64_Operand c, AArch64_Operand d) {                                        \
        assert(context != NULL);                                                        \
        AArch64_Instruction *instruction = _UNIT_Alloc(context,                         \
                                                       sizeof(AArch64_Instruction));    \
        if (instruction == NULL) {                                                      \
            return NULL;                                                                \
        }                                                                               \
        instruction->opcode = opcode_name;                                              \
        instruction->operands[0] = a;                                                   \
        instruction->operands[1] = b;                                                   \
        instruction->operands[2] = c;                                                   \
        instruction->operands[3] = d;                                                   \
        instruction->operand_count = 4;                                                 \
        return instruction;                                                             \
    }

/* Moves */
TWO_ARG_HELPER(a64_mov_reg, AArch64_MOV_REG)
TWO_ARG_HELPER(a64_mov_imm16, AArch64_MOV_IMM)
TWO_ARG_HELPER(a64_mov_wide, AArch64_MOV_WIDE)

/* Loads/Stores */
TWO_ARG_HELPER(a64_ldr, AArch64_LDR)
TWO_ARG_HELPER(a64_str, AArch64_STR)
TWO_ARG_HELPER(a64_ldrb, AArch64_LDRB)
TWO_ARG_HELPER(a64_ldrh, AArch64_LDRH)
TWO_ARG_HELPER(a64_ldrw, AArch64_LDRW)
TWO_ARG_HELPER(a64_strb, AArch64_STRB)
TWO_ARG_HELPER(a64_strh, AArch64_STRH)
TWO_ARG_HELPER(a64_strw, AArch64_STRW)
TWO_ARG_HELPER(a64_ldrsb, AArch64_LDRSB)
TWO_ARG_HELPER(a64_ldrsh, AArch64_LDRSH)
TWO_ARG_HELPER(a64_ldrsw, AArch64_LDRSW)

/* Calls */
ONE_ARG_HELPER(a64_bl_symbol, AArch64_BL_SYMBOL)
ONE_ARG_HELPER(a64_blr, AArch64_BLR)

/* Branches */
ONE_ARG_HELPER(a64_b, AArch64_B)
TWO_ARG_HELPER(a64_b_cond, AArch64_B_COND)
ONE_ARG_HELPER(a64_b_label, AArch64_B_LABEL)

/* Comparisons (encoder auto-selects reg/imm form) */
TWO_ARG_HELPER(a64_cmp, AArch64_CMP)

/* Arithmetic (encoder auto-selects reg/imm form for ADD/SUB) */
THREE_ARG_HELPER(a64_add, AArch64_ADD)
THREE_ARG_HELPER(a64_sub, AArch64_SUB)
THREE_ARG_HELPER(a64_mul, AArch64_MUL)
THREE_ARG_HELPER(a64_sdiv, AArch64_SDIV)
FOUR_ARG_HELPER(a64_msub, AArch64_MSUB)

/* Misc */
TWO_ARG_HELPER(a64_load_string, AArch64_LOAD_STRING)
NO_ARGS_HELPER(a64_ret, AArch64_RET)
ONE_ARG_HELPER(a64_svc, AArch64_SVC)
TWO_ARG_HELPER(a64_add_sp_imm, AArch64_ADD_SP_IMM)

/* Extension */
TWO_ARG_HELPER(a64_uxtb, AArch64_UXTB)
TWO_ARG_HELPER(a64_uxth, AArch64_UXTH)
TWO_ARG_HELPER(a64_uxtw, AArch64_UXTW)
TWO_ARG_HELPER(a64_sxtb, AArch64_SXTB)
TWO_ARG_HELPER(a64_sxth, AArch64_SXTH)
TWO_ARG_HELPER(a64_sxtw, AArch64_SXTW)

/* ── EMIT macro ─────────────────────────────────────────────────────── */

#define EMIT(op)                                                             \
    if (UNIT_FAILED(AArch64_encode_instruction(compile_context, op))) {      \
        return _UNIT_FAIL;                                                   \
    }

/* ── Stack load/store helpers ───────────────────────────────────────── */

static UNIT_Status
emit_load_stack(_UNIT_CompileContext *compile_context, AArch64_Register dest,
                UNIT_Size stack_offset)
{
    assert(compile_context != NULL);
    EMIT(a64_ldr(compile_context->context, a64_reg(dest),
                 a64_stack(stack_offset)));
    return _UNIT_OK;
}

static UNIT_Status
emit_store_stack(_UNIT_CompileContext *compile_context, AArch64_Register src,
                 UNIT_Size stack_offset)
{
    assert(compile_context != NULL);
    EMIT(a64_str(compile_context->context, a64_reg(src),
                 a64_stack(stack_offset)));
    return _UNIT_OK;
}

/* Load an immediate value into a register, choosing the best encoding. */
static UNIT_Status
emit_mov_imm(_UNIT_CompileContext *compile_context, AArch64_Register dest,
             int64_t value)
{
    assert(compile_context != NULL);
    uint64_t uval = (uint64_t)value;
    if (uval <= 0xFFFF) {
        EMIT(a64_mov_imm16(compile_context->context, a64_reg(dest),
                           a64_imm(value)));
    } else {
        EMIT(a64_mov_wide(compile_context->context, a64_reg(dest),
                          a64_imm(value)));
    }
    return _UNIT_OK;
}

/* ── ENSURE_IN_REGISTER / WRITEBACK_IF_STACK macros ─────────────────── */

/* Ensure an operand is in a register.  If it is already a register operand,
 * `out_name` is set to that register.  Otherwise the value is loaded into
 * `scratch` and `out_name` is set to `scratch`.
 * Evaluates to an early-return on failure. */
#define ENSURE_IN_REGISTER(operand, scratch, out_name)                       \
    AArch64_Register out_name;                                               \
    do {                                                                     \
        AArch64_Operand _eir_op = (operand);                                 \
        if (_eir_op.kind == A64_OPERAND_REGISTER) {                          \
            out_name = _eir_op.reg;                                          \
        } else if (_eir_op.kind == A64_OPERAND_STACK) {                      \
            if (UNIT_FAILED(emit_load_stack(compile_context, (scratch),       \
                                           (UNIT_Size)_eir_op.immediate))) { \
                return _UNIT_FAIL;                                           \
            }                                                                \
            out_name = (scratch);                                            \
        } else if (_eir_op.kind == A64_OPERAND_IMMEDIATE) {                  \
            if (UNIT_FAILED(emit_mov_imm(compile_context, (scratch),          \
                                         _eir_op.immediate))) {              \
                return _UNIT_FAIL;                                           \
            }                                                                \
            out_name = (scratch);                                            \
        } else {                                                             \
            _UNIT_Unreachable();                                             \
        }                                                                    \
    } while (0)

/* Write a register value back to the original operand location if it was
 * a stack slot.  No-op for register operands. */
#define WRITEBACK_IF_STACK(original, current_reg)                             \
    do {                                                                      \
        AArch64_Operand _wb_op = (original);                                  \
        if (_wb_op.kind == A64_OPERAND_STACK) {                               \
            if (UNIT_FAILED(emit_store_stack(compile_context, (current_reg),   \
                                            (UNIT_Size)_wb_op.immediate))) {  \
                return _UNIT_FAIL;                                            \
            }                                                                 \
        }                                                                     \
    } while (0)

/* ── Lowering ───────────────────────────────────────────────────────── */

static UNIT_Status
translate_operation(_UNIT_CompileContext *compile_context,
                    _UNIT_MachineOperation *operation,
                    UNIT_ABI abi,
                    _UNIT_SizeVector *epilogue_patches)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    assert(epilogue_patches != NULL);
    UNIT_Context *ctx = compile_context->context;
    assert(ctx != NULL);

#define OP(value) machine_item_to_operand(compile_context, ENSURE_VALID_ITEM(operation->value))

    switch (operation->instruction) {

        case _UNIT_I_LOAD: {
            AArch64_Operand dst = OP(destination);
            AArch64_Operand src = OP(argument_1);

            ENSURE_IN_REGISTER(src, REG_X16, src_reg);

            if (dst.kind == A64_OPERAND_REGISTER) {
                if (dst.reg != src_reg) {
                    EMIT(a64_mov_reg(ctx, a64_reg(dst.reg),
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
            assert(operation->argument_2 != NULL);
            assert(operation->argument_2->type == _UNIT_TYPE_CALL_ARGS);
            _UNIT_Vector *arguments = operation->argument_2->call_args;
            assert(arguments != NULL);
            UNIT_Size num_arguments = _UNIT_Vector_SIZE(arguments);
            assert(num_arguments <= 8);

            UNIT_Size fixed_arguments = num_arguments;
            UNIT_Size apple_fixed_arguments;
            int8_t apple_variadic = 0;
            if (abi == UNIT_ABI_APPLE
                && apple_variadic_fixed_arg_count(operation->argument_1->hint,
                                                  &apple_fixed_arguments)
                && num_arguments > apple_fixed_arguments) {
                fixed_arguments = apple_fixed_arguments;
                apple_variadic = 1;
            }
            assert(fixed_arguments <= 8);

            /* Save all virtual registers to stack slots before the call. */
            UNIT_Size save_slots[NUM_VIRTUAL_REGISTERS];
            AArch64_Operand dst_op = OP(destination);
            for (UNIT_Size i = 0; i < NUM_VIRTUAL_REGISTERS; ++i) {
                AArch64_Register saved_reg = register_map[i];
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
                _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(arguments, arg);
                assert(arg_item != NULL);
                AArch64_Operand value = machine_item_to_operand(compile_context,
                                                                arg_item);

                /* If the value is in a virtual register that we saved,
                 * load from the save slot instead to avoid clobbered values. */
                if (value.kind == A64_OPERAND_REGISTER) {
                    for (UNIT_Size i = 0; i < NUM_VIRTUAL_REGISTERS; ++i) {
                        if (save_slots[i] != (UNIT_Size)-1
                            && register_map[i] == value.reg) {
                            value = a64_stack(save_slots[i]);
                            break;
                        }
                    }
                }

                ENSURE_IN_REGISTER(value, REG_X16, src_reg);
                if (apple_variadic && arg >= fixed_arguments) {
                    UNIT_Size vararg_offset = (arg - fixed_arguments) * 8;
                    if (UNIT_FAILED(emit_store_stack(compile_context, src_reg,
                                                     vararg_offset))) {
                        return _UNIT_FAIL;
                    }
                } else {
                    AArch64_Register arg_reg = argument_registers[arg];
                    if (arg_reg != src_reg) {
                        EMIT(a64_mov_reg(ctx, a64_reg(arg_reg),
                                         a64_reg(src_reg)));
                    }
                }
            }

            /* bl <symbol> */
            EMIT(a64_bl_symbol(ctx, OP(argument_1)));

            /* Move result from X0 to destination */
            if (dst_op.kind == A64_OPERAND_REGISTER) {
                if (dst_op.reg != REG_X0) {
                    EMIT(a64_mov_reg(ctx, a64_reg(dst_op.reg),
                                     a64_reg(REG_X0)));
                }
            } else if (dst_op.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, REG_X0,
                                                 (UNIT_Size)dst_op.immediate))) {
                    return _UNIT_FAIL;
                }
            }

            /* Restore saved registers */
            for (UNIT_Size i = 0; i < NUM_VIRTUAL_REGISTERS; ++i) {
                if (save_slots[i] == (UNIT_Size)-1) {
                    continue;
                }
                AArch64_Register saved_reg = register_map[i];
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
            AArch64_Operand dst = OP(destination);
            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(a64_load_string(ctx, a64_reg(dst_reg), OP(argument_1)));

            if (dst.kind == A64_OPERAND_STACK) {
                if (UNIT_FAILED(emit_store_stack(compile_context, dst_reg,
                                                 (UNIT_Size)dst.immediate))) {
                    return _UNIT_FAIL;
                }
            }
            break;
        }

        case _UNIT_I_EXIT: {
            AArch64_Operand exit_code = OP(destination);
            ENSURE_IN_REGISTER(exit_code, REG_X0, exit_reg);
            if (exit_reg != REG_X0) {
                EMIT(a64_mov_reg(ctx, a64_reg(REG_X0), a64_reg(exit_reg)));
            }
            if (abi == UNIT_ABI_APPLE) {
                if (UNIT_FAILED(emit_mov_imm(compile_context, REG_X16, 1))) {
                    return _UNIT_FAIL;
                }
                EMIT(a64_svc(ctx, a64_imm(0x80)));
            } else {
                if (UNIT_FAILED(emit_mov_imm(compile_context, REG_X8, 93))) {
                    return _UNIT_FAIL;
                }
                EMIT(a64_svc(ctx, a64_imm(0)));
            }
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            AArch64_Operand src = OP(argument_1);
            ENSURE_IN_REGISTER(src, REG_X16, src_reg);
            if (src_reg != REG_X0) {
                EMIT(a64_mov_reg(ctx, a64_reg(REG_X0), a64_reg(src_reg)));
            }
            /* Reserve 16 bytes for the epilogue (4 instructions) */
            UNIT_Size patch_offset = _UNIT_CodeBuffer_Reserve(
                &compile_context->buffer, 16);
            if (UNIT_FAILED(_UNIT_SizeVector_Append(epilogue_patches,
                                                    patch_offset))) {
                return _UNIT_FAIL;
            }
            EMIT(a64_ret(ctx));
            break;
        }

        case _UNIT_I_LOAD_ARGUMENT: {
            assert(operation->argument_1 != NULL);
            UNIT_Size arg_index = operation->argument_1->value;
            assert(arg_index < 8);
            AArch64_Register arg_reg = argument_registers[arg_index];
            AArch64_Operand dst = OP(destination);
            if (dst.kind == A64_OPERAND_REGISTER) {
                if (dst.reg != arg_reg) {
                    EMIT(a64_mov_reg(ctx, a64_reg(dst.reg),
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
            AArch64_Operand left = OP(argument_2);
            AArch64_Operand right = OP(argument_1);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(a64_cmp(ctx, a64_reg(left_reg), right));
            } else {
                ENSURE_IN_REGISTER(right, REG_X17, right_reg);
                EMIT(a64_cmp(ctx, a64_reg(left_reg), a64_reg(right_reg)));
            }
            break;
        }

        case _UNIT_I_JUMP: {
            AArch64_Operand target = OP(argument_1);
            assert(target.kind == A64_OPERAND_IMMEDIATE);
            EMIT(a64_b(ctx, target));
            break;
        }

        case _UNIT_I_JUMP_LABEL: {
            AArch64_Operand label = OP(destination);
            assert(label.kind == A64_OPERAND_IMMEDIATE);
            EMIT(a64_b_label(ctx, label));
            break;
        }

#define JUMP_CONDITION(inst, cond_code)                                     \
        case inst: {                                                        \
            AArch64_Operand left = OP(argument_1);                          \
            AArch64_Operand right = OP(argument_2);                         \
            ENSURE_IN_REGISTER(left, REG_X16, left_reg);                    \
            if (right.kind == A64_OPERAND_IMMEDIATE                         \
                && right.immediate >= 0 && right.immediate < 4096) {        \
                EMIT(a64_cmp(ctx, a64_reg(left_reg), right));               \
            } else {                                                        \
                ENSURE_IN_REGISTER(right, REG_X17, right_reg);              \
                EMIT(a64_cmp(ctx, a64_reg(left_reg),                        \
                             a64_reg(right_reg)));                          \
            }                                                               \
            EMIT(a64_b_cond(ctx, OP(destination), a64_cond(cond_code)));    \
            break;                                                          \
        }

        JUMP_CONDITION(_UNIT_I_JUMP_IF_EQUAL, AArch64_COND_EQ)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_NOT_EQUAL, AArch64_COND_NE)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS, AArch64_COND_LT)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS_EQUAL, AArch64_COND_LE)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER, AArch64_COND_GT)
        JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER_EQUAL, AArch64_COND_GE)

#undef JUMP_CONDITION

        case _UNIT_I_ADD: {
            AArch64_Operand dst = OP(destination);
            AArch64_Operand left = OP(argument_1);
            AArch64_Operand right = OP(argument_2);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(a64_add(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                             right));
            } else {
                ENSURE_IN_REGISTER(right, REG_X17, right_reg);
                EMIT(a64_add(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                             a64_reg(right_reg)));
            }

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_SUB: {
            AArch64_Operand dst = OP(destination);
            AArch64_Operand left = OP(argument_1);
            AArch64_Operand right = OP(argument_2);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            if (right.kind == A64_OPERAND_IMMEDIATE
                && right.immediate >= 0 && right.immediate < 4096) {
                EMIT(a64_sub(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                             right));
            } else {
                ENSURE_IN_REGISTER(right, REG_X17, right_reg);
                EMIT(a64_sub(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                             a64_reg(right_reg)));
            }

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_MUL: {
            AArch64_Operand dst = OP(destination);
            AArch64_Operand left = OP(argument_1);
            AArch64_Operand right = OP(argument_2);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);
            ENSURE_IN_REGISTER(right, REG_X17, right_reg);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(a64_mul(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                         a64_reg(right_reg)));

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_DIV: {
            AArch64_Operand dst = OP(destination);
            AArch64_Operand left = OP(argument_1);
            AArch64_Operand right = OP(argument_2);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);
            ENSURE_IN_REGISTER(right, REG_X17, right_reg);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(a64_sdiv(ctx, a64_reg(dst_reg), a64_reg(left_reg),
                          a64_reg(right_reg)));

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_MOD: {
            /* remainder = dividend - (dividend / divisor) * divisor */
            AArch64_Operand dst = OP(destination);
            AArch64_Operand left = OP(argument_1);
            AArch64_Operand right = OP(argument_2);

            ENSURE_IN_REGISTER(left, REG_X16, left_reg);
            ENSURE_IN_REGISTER(right, REG_X17, right_reg);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            /* Use X8 as temp for quotient */
            EMIT(a64_sdiv(ctx, a64_reg(REG_X8), a64_reg(left_reg),
                          a64_reg(right_reg)));
            /* MSUB dst, quotient, divisor, dividend => dividend - quotient*divisor */
            EMIT(a64_msub(ctx, a64_reg(dst_reg), a64_reg(REG_X8),
                          a64_reg(right_reg), a64_reg(left_reg)));

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_CONVERT: {
            AArch64_Operand src = OP(argument_1);
            assert(operation->argument_2 != NULL);
            UNIT_IntegerType target = operation->argument_2->value;

            ENSURE_IN_REGISTER(src, REG_X16, src_reg);

            AArch64_Operand dst = OP(destination);
            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            switch (target) {
                case UNIT_TYPE_UINT8:
                    EMIT(a64_uxtb(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT8:
                    EMIT(a64_sxtb(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT16:
                    EMIT(a64_uxth(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT16:
                    EMIT(a64_sxth(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT32:
                    EMIT(a64_uxtw(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_INT32:
                    EMIT(a64_sxtw(ctx, a64_reg(dst_reg), a64_reg(src_reg)));
                    break;
                case UNIT_TYPE_UINT64:
                case UNIT_TYPE_INT64:
                    if (dst_reg != src_reg) {
                        EMIT(a64_mov_reg(ctx, a64_reg(dst_reg),
                                         a64_reg(src_reg)));
                    }
                    break;
            }

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_READ_BYTES: {
            AArch64_Operand addr = OP(argument_1);
            assert(operation->argument_2 != NULL);
            UNIT_Size size = operation->argument_2->value;

            ENSURE_IN_REGISTER(addr, REG_X16, addr_reg);

            AArch64_Operand dst = OP(destination);
            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X17;
            }

            switch (size) {
                case 1:
                    EMIT(a64_ldrb(ctx, a64_reg(dst_reg), a64_reg(addr_reg)));
                    break;
                case 2:
                    EMIT(a64_ldrh(ctx, a64_reg(dst_reg), a64_reg(addr_reg)));
                    break;
                case 4:
                    EMIT(a64_ldrw(ctx, a64_reg(dst_reg), a64_reg(addr_reg)));
                    break;
                case 8:
                    EMIT(a64_ldr(ctx, a64_reg(dst_reg),
                                 a64_indirect(addr_reg)));
                    break;
                default:
                    _UNIT_Unreachable();
            }

            WRITEBACK_IF_STACK(dst, dst_reg);
            break;
        }

        case _UNIT_I_WRITE_BYTES: {
            AArch64_Operand addr = OP(destination);
            AArch64_Operand value = OP(argument_1);
            assert(operation->argument_2 != NULL);
            UNIT_Size size = operation->argument_2->value;

            ENSURE_IN_REGISTER(addr, REG_X16, addr_reg);
            ENSURE_IN_REGISTER(value, REG_X17, val_reg);

            switch (size) {
                case 1:
                    EMIT(a64_strb(ctx, a64_reg(val_reg), a64_reg(addr_reg)));
                    break;
                case 2:
                    EMIT(a64_strh(ctx, a64_reg(val_reg), a64_reg(addr_reg)));
                    break;
                case 4:
                    EMIT(a64_strw(ctx, a64_reg(val_reg), a64_reg(addr_reg)));
                    break;
                case 8:
                    EMIT(a64_str(ctx, a64_reg(val_reg),
                                 a64_indirect(addr_reg)));
                    break;
                default:
                    _UNIT_Unreachable();
            }
            break;
        }

        case _UNIT_I_ADDRESS_OF: {
            AArch64_Operand src = OP(argument_1);
            AArch64_Operand dst = OP(destination);
            assert(src.kind == A64_OPERAND_STACK);

            AArch64_Register dst_reg;
            if (dst.kind == A64_OPERAND_REGISTER) {
                dst_reg = dst.reg;
            } else {
                dst_reg = REG_X16;
            }

            EMIT(a64_add_sp_imm(ctx, a64_reg(dst_reg), src));

            WRITEBACK_IF_STACK(dst, dst_reg);
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
        AArch64_PatchEpilogue(compile_context, epilogue_offset, frame_size);
    }
}

UNIT_Status
_UNIT_AARCH64_Compile(_UNIT_Translation *translation,
                      _UNIT_CompileContext *compile_context,
                      UNIT_ABI abi)
{
    assert(translation != NULL);
    assert(compile_context != NULL);

    /* Reserve 16 bytes for the prologue (4 ARM64 instructions) */
    UNIT_Size prologue_offset = _UNIT_CodeBuffer_Reserve(&compile_context->buffer, 16);

    _UNIT_SizeVector epilogue_patches;
    if (UNIT_FAILED(_UNIT_SizeVector_Init(&epilogue_patches,
                                          compile_context->context, 4))) {
        return _UNIT_FAIL;
    }

    UNIT_Size outgoing_slots = max_outgoing_variadic_arg_slots(translation, abi);
    compile_context->stack_frame.slot_offset = outgoing_slots;
    compile_context->stack_frame.next_slot += outgoing_slots;
    compile_context->stack_frame.reserved_slots += outgoing_slots;

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
    if (frame_size % 16 != 0) {
        frame_size = (frame_size + 15) & ~(UNIT_Size)15;
    }
    AArch64_PatchPrologue(compile_context, prologue_offset, frame_size);
    patch_epilogues(compile_context, &epilogue_patches, frame_size);
    AArch64_PatchJumps(compile_context);

    _UNIT_SizeVector_Clear(&epilogue_patches);
    return _UNIT_OK;
}
