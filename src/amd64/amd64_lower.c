#include <stdio.h>
#include <stdlib.h>

#include <unit/platform.h>

#include <unit/internal/architectures.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

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
               .stack_offset = offset
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

static const AMD64_Register *
get_platform_register_map(UNIT_Platform platform)
{
    static const AMD64_Register systemv[] = {
        REG_RAX, REG_RCX, REG_RDX, REG_RSI, REG_RDI,
        REG_R8, REG_R9, REG_R10,
    };

    static const AMD64_Register win64[] = {
        REG_RAX, REG_RCX, REG_RDX,
        REG_R8, REG_R9, REG_R10,
    };

    UNIT_ABI abi = UNIT_Platform_GET_ABI(platform);
    if (abi == UNIT_ABI_SYSTEMV) {
        return systemv;
    } else {
        assert(abi == UNIT_ABI_WIN64);
        return win64;
    }
}

static const AMD64_Register REG_SCRATCH = REG_R11;

static const AMD64_Register *
get_argument_registers(UNIT_Platform platorm)
{
    static const AMD64_Register systemv[] = {
        REG_RDI, REG_RSI, REG_RDX, REG_RCX, REG_R8, REG_R9
    };
    static const AMD64_Register win64[] = {
        REG_RCX, REG_RDX, REG_R8, REG_R9
    };
    UNIT_ABI abi = UNIT_Platform_GET_ABI(platorm);

    if (abi == UNIT_ABI_SYSTEMV) {
        return systemv;
    } else {
        assert(abi == UNIT_ABI_WIN64);
        return win64;
    }
}

AMD64_Operand
machine_item_to_operand(const _UNIT_CompileContext *compile_context,
                        const _UNIT_MachineItem *machine_item)
{
    assert(compile_context != NULL);
    assert(machine_item != NULL);
    if (machine_item->type == _UNIT_TYPE_REGISTER) {
        const AMD64_Register *map = get_platform_register_map(compile_context->target);
        return reg(map[machine_item->value]);
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

static _UNIT_MachineItem *
generic_item_passthrough(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    return item;
}

#define ENSURE_VALID_ITEM(item)                                  \
        _Generic((item),                                         \
                 _UNIT_MachineItem * : generic_item_passthrough, \
                 _UNIT_MachineDestination:                       \
                 _UNIT_MachineDestination_GetPointerNullable     \
        )(item)

#define EMIT(expr)                      \
        do {                            \
            if (UNIT_FAILED((expr))) {  \
                return _UNIT_FAIL;      \
            }                           \
        } while (0);

#define EMIT_MOVE(dst, src) EMIT(AMD64_Emit_Move(ctx, dst, src));

static UNIT_Status
lower_operation(_UNIT_CompileContext *compile_context,
                _UNIT_MachineOperation *operation,
                _UNIT_SizeVector *epilogue_patches)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    assert(epilogue_patches != NULL);
    UNIT_Context *ctx = compile_context->context;

#define OP(value) machine_item_to_operand(compile_context, ENSURE_VALID_ITEM(operation->value))

    const AMD64_Register *register_map = get_platform_register_map(compile_context->target);
    UNIT_ABI abi = UNIT_Platform_GET_ABI(compile_context->target);

    switch (operation->instruction) {
        /* General instructions */

        case _UNIT_I_LOAD: {
            AMD64_Operand dst = OP(destination);
            AMD64_Operand src = OP(argument_1);
            assert(dst.kind != OPERAND_IMMEDIATE);

            if (dst.kind == OPERAND_STACK && src.kind == OPERAND_STACK) {
                EMIT_MOVE(REG_SCRATCH, src);
                EMIT_MOVE(dst, REG_SCRATCH);
                //DISPATCH(AMD64_Emit_Move(ctx, dst, argument_1));
            } else if (dst.kind == OPERAND_STACK &&
                       src.kind == OPERAND_IMMEDIATE) {
                EMIT(mov(ctx, reg(REG_SCRATCH), src));
                EMIT(mov(ctx, dst, reg(REG_SCRATCH)));
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
            const AMD64_Register *argument_registers =
                get_argument_registers(compile_context->target);

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
                EMIT(mov(ctx,
                         stack_slot(save_slots[index]),
                         reg(saved_register)));
            }

            // TODO: Handle when there are more than six args
            for (UNIT_Size argument = 0; argument < num_arguments;
                 ++argument) {
                AMD64_Register argument_register =
                    argument_registers[argument];
                _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(arguments,
                                                               argument);
                AMD64_Operand value = machine_item_to_operand(compile_context, arg_item);

                // We load from the save slots in order to avoid some circular
                // dependency issues.
                // For example, RDI wants to go to RSI, and RSI wants to go to
                // RDI. If you did a sequential mov rdi, rsi and mov rsi, rdi,
                // then RDI would be clobbered before the second mov.
                if (value.kind == OPERAND_REGISTER) {
                    for (UNIT_Size index = 0; index < 8; ++index) {
                        if (save_slots[index] != (UNIT_Size) - 1
                            && register_map[index] == value.reg) {
                            value = stack_slot(save_slots[index]);
                            break;
                        }
                    }
                }

                EMIT(mov(ctx, reg(argument_register), value));
            }

            // Win64 requires "shadow space"
            if (abi == UNIT_ABI_WIN64) {
                EMIT(sub(ctx, reg(REG_RSP), immediate(32)));
            }

            EMIT(mov(ctx, reg(REG_RAX), immediate(0)));
            EMIT(call_symbol(ctx, OP(argument_1)));
            EMIT(mov(ctx, OP(destination), reg(REG_RAX)));

            if (abi == UNIT_ABI_WIN64) {
                EMIT(add(ctx, reg(REG_RSP), immediate(32)));
            }

            for (UNIT_Size index = 0; index < 8; ++index) {
                if (save_slots[index] == -1) {
                    continue;
                }

                AMD64_Register saved_register = register_map[index];
                EMIT(mov(ctx,
                         reg(saved_register),
                         stack_slot(save_slots[index])));
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame,
                                          save_slots[index]);
            }

            RESTORE_REGISTER(REG_RAX);

            break;
        }

        case _UNIT_I_LOAD_STRING: {
            USE_SCRATCH_REGISTER(destination);
            EMIT(load_string(ctx, destination, OP(argument_1)));
            UNDO_SCRATCH_REGISTER(destination);
            break;
        }

        /* Functions */

        case _UNIT_I_EXIT: {
            EMIT(mov(ctx, reg(REG_RAX), immediate(60))); // syscall number for exit
            EMIT(mov(ctx, reg(REG_RDI), OP(destination)));
            EMIT(syscall(ctx));
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            EMIT(mov(ctx, reg(REG_RAX), OP(argument_1)));
            // Reserve 7 bytes for the epilogue
            UNIT_Size patch_offset =
                _UNIT_CodeBuffer_Reserve(&compile_context->buffer, 7);
            if (UNIT_FAILED(_UNIT_SizeVector_Append(epilogue_patches,
                                                    patch_offset))) {
                return _UNIT_FAIL;
            }

            EMIT(ret(ctx));
            break;
        }

        case _UNIT_I_LOAD_ARGUMENT: {
            const AMD64_Register *argument_registers =
                get_argument_registers(abi);
            UNIT_Size arg_index = operation->argument_1->value;
            // TODO: Handle more than 6 args
            assert(arg_index < 6);
            AMD64_Register argument_register = argument_registers[arg_index];
            EMIT(mov(ctx, OP(destination), reg(argument_register)));
            break;
        }

        /* Comparisons */
        case _UNIT_I_COMPARE_EQUAL: {
            EMIT(cmp(ctx, OP(argument_2), OP(argument_1)));
            break;
        }

        /* Jumps */

        case _UNIT_I_JUMP: {
            EMIT(jmp(ctx, OP(argument_1)));
            break;
        }

        case _UNIT_I_JUMP_LABEL: {
            EMIT(_jmp_label(ctx, OP(destination)));
            break;
        }

        #define JUMP_CONDITION(inst, op)                              \
                case inst: {                                          \
                        AMD64_Operand left = OP(argument_1);          \
                        AMD64_Operand right = OP(argument_2);         \
                        /* cmp needs at least one register operand */ \
                        if (left.kind != OPERAND_REGISTER) {          \
                            USE_SCRATCH_REGISTER(argument_1);         \
                            left = argument_1;                        \
                        }                                             \
                        EMIT(cmp(ctx, left, right));                  \
                        EMIT(op(ctx, OP(destination)));               \
                        break;                                        \
                }

            JUMP_CONDITION(_UNIT_I_JUMP_IF_EQUAL, jump_if_equal);
            JUMP_CONDITION(_UNIT_I_JUMP_IF_NOT_EQUAL, jump_if_not_equal);
            JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS, jump_if_less);
            JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS_EQUAL, jump_if_less_equal);
            JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER, jump_if_greater);
            JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER_EQUAL,
                           jump_if_greater_equal);

        #undef JUMP_CONDITION

            /* Arithmetic */

        #define BINARY_OP(inst, helper) \
                case inst: {            \
                        /* When the destination and righthand side are the same register, they
                         * clobber one another in the normal path. To avoid that, we load the RHS
                         * into the scratch register to perform the operation. */ \
                        if (operands_equal(OP(destination), OP(argument_2))       \
                            && !operands_equal(OP(destination),                   \
                                               OP(argument_1))) {                 \
                            EMIT(mov(ctx, reg(REG_R11), OP(argument_2)));         \
                            EMIT(mov(ctx, OP(destination), OP(argument_1)));      \
                            EMIT(helper(ctx, OP(destination), reg(REG_R11)));     \
                        } else {                                                  \
                            USE_SCRATCH_REGISTER(destination);                    \
                            EMIT(mov(ctx, destination, OP(argument_1)));          \
                            EMIT(helper(ctx, destination, OP(argument_2)));       \
                            UNDO_SCRATCH_REGISTER(destination);                   \
                        }                                                         \
                        break;                                                    \
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
            USE_SCRATCH_REGISTER(argument_2);
            EMIT(idiv(ctx, argument_2));
            UNDO_SCRATCH_REGISTER(argument_2);

            if (operation->instruction == _UNIT_I_MOD) {
                EMIT(mov(ctx, dst, reg(REG_RDX)));
            } else {
                EMIT(mov(ctx, dst, reg(REG_RAX)));
            }

            RESTORE_REGISTER(REG_RDX);
            RESTORE_REGISTER(REG_RAX);
            break;
        }

        /* Casting */

        case _UNIT_I_CONVERT: {
            USE_SCRATCH_REGISTER(argument_1);
            UNIT_IntegerType target = operation->argument_2->value;

            switch (target) {
                case UNIT_TYPE_UINT8: {
                    EMIT(movzx8(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_INT8: {
                    EMIT(movsx8(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_UINT16: {
                    EMIT(movzx16(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_INT16: {
                    EMIT(movsx16(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_UINT32: {
                    EMIT(mov32(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_INT32: {
                    EMIT(movsxd(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
                case UNIT_TYPE_UINT64:
                case UNIT_TYPE_INT64: {
                    EMIT(mov(ctx, reg(REG_SCRATCH), argument_1));
                    break;
                }
            }

            EMIT(mov(ctx, OP(destination), reg(REG_SCRATCH)));
            break;
        }

        /* Pointers */

        case _UNIT_I_READ_BYTES: {
            USE_SCRATCH_REGISTER(argument_1);
            UNIT_Size size = operation->argument_2->value;

            if (size == 8) {
                EMIT(mov(ctx, reg(REG_SCRATCH), indirect(argument_1)));
            } else {
                EMIT(movzx(ctx,
                           reg(REG_SCRATCH),
                           indirect(argument_1),
                           immediate(size)));
            }

            EMIT(mov(ctx, OP(destination), reg(REG_SCRATCH)));
            break;
        }

        case _UNIT_I_WRITE_BYTES: {
            USE_SCRATCH_REGISTER(destination);
            AMD64_Operand addr = destination;
            UNIT_Size size = operation->argument_2->value;

            if (OP(argument_1).kind == OPERAND_REGISTER
                || OP(argument_1).kind == OPERAND_IMMEDIATE) {
                EMIT(mov_sized(ctx,
                               indirect(addr),
                               OP(argument_1),
                               immediate(size)));
            } else {
                UNIT_Size slot =
                    _UNIT_StackFrame_AllocateSlot(
                        &compile_context->stack_frame);
                EMIT(mov(ctx, stack_slot(slot), addr));
                EMIT(mov(ctx, reg(REG_SCRATCH), OP(argument_1)));
                EMIT(mov(ctx, reg(addr.reg), stack_slot(slot)));
                EMIT(mov_sized(ctx,
                               indirect(reg(addr.reg)),
                               reg(REG_SCRATCH),
                               immediate(size)));
                _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, slot);
            }

            break;
        }

        case _UNIT_I_ADDRESS_OF: {
            EMIT(lea(ctx, OP(destination), OP(argument_1)));
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
    assert(frame_size >= 0);
    UNIT_Size size = _UNIT_SizeVector_SIZE(epilogue_patches);
    for (UNIT_Size index = 0; index < size; ++index) {
        UNIT_Size epilogue_offset = _UNIT_SizeVector_GET(epilogue_patches,
                                                         index);
        AMD64_PatchEpilogue(compile_context, epilogue_offset, frame_size);
    }
}

UNIT_Status
_UNIT_AMD64_Compile(_UNIT_Translation *translation,
                    _UNIT_CompileContext *compile_context)
{
    // Reserve space for the prologue (sub rsp, imm32 = 7 bytes).
    // We'll patch it once we know the final frame size.
    UNIT_Size prologue_offset =
        _UNIT_CodeBuffer_Reserve(&compile_context->buffer, 7);

    // Same thing for the epilogue, but there can be multiple places that need patching.
    _UNIT_SizeVector epilogue_patches;
    if (UNIT_FAILED(_UNIT_SizeVector_Init(&epilogue_patches,
                                          compile_context->context,
                                          4))) {
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
            _UNIT_MachineOperation *operation =
                _UNIT_Vector_GET(&block->instructions,
                                 index);
            assert(operation != NULL);
            if (UNIT_FAILED(lower_operation(compile_context,
                                            operation,
                                            &epilogue_patches))) {
                _UNIT_SizeVector_Clear(&epilogue_patches);
                return _UNIT_FAIL;
            }
        }
    }

    UNIT_Size frame_size =
        _UNIT_StackFrame_ComputeSize(&compile_context->stack_frame);
    AMD64_PatchPrologue(compile_context, prologue_offset, frame_size);
    patch_epilogues(compile_context, &epilogue_patches, frame_size);
    AMD64_PatchJumps(compile_context);

    _UNIT_SizeVector_Clear(&epilogue_patches);
    return _UNIT_OK;
}
