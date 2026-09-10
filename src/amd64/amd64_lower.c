#include <unit/platform.h>

#include <unit/internal/architectures.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

#include "amd64_local.h"

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

#define EMIT(expr)                     \
        do {                           \
            if (UNIT_FAILED((expr))) { \
                return _UNIT_FAIL;     \
            }                          \
        } while (0)

static AMD64_Register
item_register(const _UNIT_CompileContext *context, const _UNIT_MachineItem *item)
{
    assert(item != NULL && item->type == _UNIT_TYPE_REGISTER);
    return get_platform_register_map(context->target)[item->value];
}

static AMD64_StackSlot
item_stack_slot(const _UNIT_MachineItem *item)
{
    assert(item != NULL && item->type == _UNIT_TYPE_MEMORY);
    return (AMD64_StackSlot) { .offset = item->value * 8 };
}

static UNIT_Status
load_register(_UNIT_CompileContext *context,
              AMD64_Register dst,
              const _UNIT_MachineItem *src)
{
    assert(src != NULL);
    switch (src->type) {
        case _UNIT_TYPE_REGISTER: {
            return AMD64_Move_RegReg(&context->buffer, dst, item_register(context, src));
        }
        case _UNIT_TYPE_CONSTANT: {
            return AMD64_Move_RegImmediate(&context->buffer,
                                           dst,
                                           (AMD64_Immediate) { .immediate = src->value });
        }
        case _UNIT_TYPE_MEMORY: {
            return AMD64_Move_RegStack(&context->buffer, dst, item_stack_slot(src));
        }
        default: {
            _UNIT_Unreachable();
        }
    }
}

static UNIT_Status
store_register(_UNIT_CompileContext *context,
               const _UNIT_MachineItem *dst,
               AMD64_Register src)
{
    assert(dst != NULL);
    if (dst->type == _UNIT_TYPE_REGISTER) {
        return AMD64_Move_RegReg(&context->buffer, item_register(context, dst), src);
    }

    return AMD64_Move_StackReg(&context->buffer, item_stack_slot(dst), src);
}

static UNIT_Status
save_register(_UNIT_CompileContext *context, AMD64_Register reg, AMD64_StackSlot *slot)
{
    slot->offset = _UNIT_StackFrame_AllocateSlot(&context->stack_frame);
    return AMD64_Move_StackReg(&context->buffer, *slot, reg);
}

static UNIT_Status
restore_register(_UNIT_CompileContext *context, AMD64_Register reg, AMD64_StackSlot slot)
{
    EMIT(AMD64_Move_RegStack(&context->buffer, reg, slot));
    _UNIT_StackFrame_FreeSlot(&context->stack_frame, slot.offset);
    return _UNIT_OK;
}

static UNIT_Status
append_relocation(_UNIT_CompileContext *context, _UNIT_Relocation *relocation)
{
    if (relocation == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&context->symbol_table.relocations, relocation))) {
        _UNIT_Relocation_Free(context->context, relocation);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static UNIT_Status
append_jump(_UNIT_CompileContext *context, UNIT_Size offset, UNIT_Size label)
{
    _UNIT_PendingJump *jump = _UNIT_PendingJump_New(context->context, offset, label);
    if (jump == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&context->jump_table.pending_jumps, jump))) {
        _UNIT_PendingJump_Free(context->context, jump);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static UNIT_Status
compare_items(_UNIT_CompileContext *context,
              const _UNIT_MachineItem *left,
              const _UNIT_MachineItem *right)
{
    _UNIT_CodeBuffer *buffer = &context->buffer;
    EMIT(load_register(context, REG_SCRATCH, left));
    if (right->type == _UNIT_TYPE_REGISTER) {
        return AMD64_Compare_RegReg(buffer, REG_SCRATCH, item_register(context, right));
    }

    if (right->type == _UNIT_TYPE_MEMORY) {
        return AMD64_Compare_RegStack(buffer, REG_SCRATCH, item_stack_slot(right));
    }

    // This API encodes a sign-extended byte, so larger constants need a register.
    if (right->value >= 0 && right->value <= INT8_MAX) {
        return AMD64_Compare_RegImmediate(buffer,
                                          REG_SCRATCH,
                                          (AMD64_Immediate) { .immediate = right->value });
    }

    AMD64_StackSlot saved;
    EMIT(save_register(context, REG_R10, &saved));
    EMIT(load_register(context, REG_R10, right));
    EMIT(AMD64_Compare_RegReg(buffer, REG_SCRATCH, REG_R10));
    // MOV preserves the comparison flags.
    return restore_register(context, REG_R10, saved);
}

static UNIT_Status
lower_operation(_UNIT_CompileContext *compile_context,
                _UNIT_MachineOperation *operation,
                _UNIT_SizeVector *epilogue_patches)
{
    assert(compile_context != NULL);
    assert(operation != NULL);
    assert(epilogue_patches != NULL);
    UNIT_Context *ctx = compile_context->context;
    _UNIT_CodeBuffer *buffer = &compile_context->buffer;
    _UNIT_MachineItem *destination =
        _UNIT_MachineDestination_GetPointerNullable(operation->destination);
    _UNIT_MachineItem *left = operation->argument_1;
    _UNIT_MachineItem *right = operation->argument_2;
    UNIT_ABI abi = UNIT_Platform_GET_ABI(compile_context->target);

    switch (operation->instruction) {
        case _UNIT_I_LOAD: {
            if (destination->type == _UNIT_TYPE_REGISTER) {
                EMIT(load_register(compile_context,
                                   item_register(compile_context, destination),
                                   left));
            } else {
                EMIT(load_register(compile_context, REG_SCRATCH, left));
                EMIT(store_register(compile_context, destination, REG_SCRATCH));
            }

            break;
        }

        case _UNIT_I_CALL_SYMBOL: {
            assert(right->type == _UNIT_TYPE_CALL_ARGS);
            _UNIT_Vector *arguments = right->call_args;
            UNIT_Size num_arguments = _UNIT_Vector_SIZE(arguments);
            assert(num_arguments <= (abi == UNIT_ABI_WIN64 ? 4 : 6));
            const AMD64_Register *register_map = get_platform_register_map(compile_context->target);
            const AMD64_Register *argument_registers =
                get_argument_registers(compile_context->target);
            UNIT_Size num_registers = abi == UNIT_ABI_WIN64 ? 6 : 8;
            AMD64_StackSlot saved[8];
            // Save every source before arranging arguments, including the result register.
            for (UNIT_Size index = 0; index < num_registers; ++index) {
                EMIT(save_register(compile_context, register_map[index], &saved[index]));
            }

            for (UNIT_Size index = 0; index < num_arguments; ++index) {
                _UNIT_MachineItem *argument = _UNIT_Vector_GET(arguments, index);
                if (argument->type == _UNIT_TYPE_REGISTER) {
                    EMIT(AMD64_Move_RegStack(buffer,
                                             argument_registers[index],
                                             saved[argument->value]));
                } else {
                    EMIT(load_register(compile_context, argument_registers[index], argument));
                }
            }

            if (abi == UNIT_ABI_WIN64) {
                EMIT(AMD64_Sub_RegImmediate(buffer, REG_RSP, (AMD64_Immediate) {32}));
            }

            EMIT(AMD64_Move_RegImmediate(buffer, REG_RAX, (AMD64_Immediate) {0}));
            UNIT_Size offset;
            EMIT(AMD64_CallSymbol(buffer, &offset));
            EMIT(append_relocation(compile_context,
                                   _UNIT_Relocation_NewCall(ctx, offset, left->value)));
            if (abi == UNIT_ABI_WIN64) {
                EMIT(AMD64_Add_RegImmediate(buffer, REG_RSP, (AMD64_Immediate) {32}));
            }

            if (destination != NULL) {
                EMIT(store_register(compile_context, destination, REG_RAX));
            }

            for (UNIT_Size index = 0; index < num_registers; ++index) {
                if (destination != NULL && destination->type == _UNIT_TYPE_REGISTER
                    && item_register(compile_context, destination) == register_map[index]) {
                    _UNIT_StackFrame_FreeSlot(&compile_context->stack_frame, saved[index].offset);
                } else {
                    EMIT(restore_register(compile_context, register_map[index], saved[index]));
                }
            }

            break;
        }

        case _UNIT_I_LOAD_STRING: {
            UNIT_Size byte_offset = _UNIT_SizeMap_GET(&compile_context->string_data.string_offsets,
                                                      left->value);
            UNIT_Size offset;
            EMIT(AMD64_LoadEffectiveAddress_RegRel(buffer, REG_SCRATCH, &offset));
            EMIT(append_relocation(compile_context,
                                   _UNIT_Relocation_NewData(ctx, offset, byte_offset)));
            EMIT(store_register(compile_context, destination, REG_SCRATCH));
            break;
        }

        case _UNIT_I_EXIT: {
            EMIT(load_register(compile_context, REG_RDI, destination));
            EMIT(AMD64_Move_RegImmediate(buffer, REG_RAX, (AMD64_Immediate) {60}));
            EMIT(AMD64_Syscall(buffer));
            break;
        }

        case _UNIT_I_RETURN_VALUE: {
            EMIT(load_register(compile_context, REG_RAX, left));
            UNIT_Size offset = _UNIT_CodeBuffer_Reserve(buffer, 7);
            EMIT(_UNIT_SizeVector_Append(epilogue_patches, offset));
            EMIT(AMD64_Return(buffer));
            break;
        }

        case _UNIT_I_LOAD_ARGUMENT: {
            const AMD64_Register *argument_registers =
                get_argument_registers(compile_context->target);
            assert(left->value >= 0 && left->value < (abi == UNIT_ABI_WIN64 ? 4 : 6));
            EMIT(store_register(compile_context, destination, argument_registers[left->value]));
            break;
        }

        case _UNIT_I_COMPARE_EQUAL: {
            EMIT(compare_items(compile_context, left, right));
            break;
        }

        case _UNIT_I_JUMP: {
            UNIT_Size offset;
            EMIT(AMD64_Jump_Rel(buffer, &offset));
            EMIT(append_jump(compile_context, offset, left->value));
            break;
        }

        case _UNIT_I_JUMP_LABEL: {
            EMIT(_UNIT_SizeMap_Set(&compile_context->jump_table.label_offsets,
                                   destination->value,
                                   _UNIT_CodeBuffer_CurrentIndex(buffer)));
            break;

#define JUMP_CONDITION(inst, helper)                                            \
        case inst: {                                                            \
                UNIT_Size offset;                                               \
                EMIT(compare_items(compile_context, left, right));              \
                EMIT(helper(buffer, &offset));                                  \
                EMIT(append_jump(compile_context, offset, destination->value)); \
                break;                                                          \
        }
            JUMP_CONDITION(_UNIT_I_JUMP_IF_EQUAL, AMD64_JumpEqual_Rel)
            JUMP_CONDITION(_UNIT_I_JUMP_IF_NOT_EQUAL, AMD64_JumpNotEqual_Rel)
            JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS, AMD64_JumpLess_Rel)
            JUMP_CONDITION(_UNIT_I_JUMP_IF_LESS_EQUAL, AMD64_JumpLessEqual_Rel)
            JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER, AMD64_JumpGreater_Rel)
            JUMP_CONDITION(_UNIT_I_JUMP_IF_GREATER_EQUAL, AMD64_JumpGreaterEqual_Rel)
#undef JUMP_CONDITION

#define BINARY_OP(inst, helper)                                                      \
        case inst: {                                                                 \
                EMIT(load_register(compile_context, REG_SCRATCH, left));             \
                if (right->type == _UNIT_TYPE_REGISTER) {                            \
                    EMIT(helper ## _RegReg(buffer,                                   \
                                           REG_SCRATCH,                              \
                                           item_register(compile_context, right)));  \
                } else if (right->type == _UNIT_TYPE_CONSTANT                        \
                           && right->value >= 0 && right->value <= INT32_MAX) {      \
                    EMIT(helper ## _RegImmediate(buffer,                             \
                                                 REG_SCRATCH,                        \
                                                 (AMD64_Immediate) {right->value})); \
                } else {                                                             \
                    AMD64_StackSlot saved;                                           \
                    EMIT(save_register(compile_context, REG_R10, &saved));           \
                    EMIT(load_register(compile_context, REG_R10, right));            \
                    EMIT(helper ## _RegReg(buffer, REG_SCRATCH, REG_R10));           \
                    EMIT(restore_register(compile_context, REG_R10, saved));         \
                }                                                                    \
                EMIT(store_register(compile_context, destination, REG_SCRATCH));     \
                break;                                                               \
        }
            BINARY_OP(_UNIT_I_ADD, AMD64_Add)
            BINARY_OP(_UNIT_I_SUB, AMD64_Sub)
            BINARY_OP(_UNIT_I_MUL, AMD64_IntMul)}
#undef BINARY_OP

        case _UNIT_I_DIV:
        case _UNIT_I_MOD: {
            AMD64_StackSlot saved_rax, saved_rdx;
            EMIT(save_register(compile_context, REG_RAX, &saved_rax));
            EMIT(save_register(compile_context, REG_RDX, &saved_rdx));
            // Capture the divisor before RAX and RDX are overwritten.
            EMIT(load_register(compile_context, REG_SCRATCH, right));
            EMIT(load_register(compile_context, REG_RAX, left));
            EMIT(AMD64_ConvertQuadwordToOctoword(buffer));
            EMIT(AMD64_IntDiv_Reg(buffer, REG_SCRATCH));
            EMIT(AMD64_Move_RegReg(buffer,
                                   REG_SCRATCH,
                                   operation->instruction == _UNIT_I_MOD ? REG_RDX : REG_RAX));
            EMIT(restore_register(compile_context, REG_RDX, saved_rdx));
            EMIT(restore_register(compile_context, REG_RAX, saved_rax));
            EMIT(store_register(compile_context, destination, REG_SCRATCH));
            break;
        }

        case _UNIT_I_CONVERT: {
            EMIT(load_register(compile_context, REG_SCRATCH, left));
            switch ((UNIT_IntegerType) right->value) {
                case UNIT_TYPE_UINT8: {
                    EMIT(AMD64_MoveZeroExtend8_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_INT8: {
                    EMIT(AMD64_MoveSignExtend8_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_UINT16: {
                    EMIT(AMD64_MoveZeroExtend16_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_INT16: {
                    EMIT(AMD64_MoveSignExtend16_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_UINT32: {
                    EMIT(AMD64_Move32_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_INT32: {
                    EMIT(AMD64_MoveSignExtendDword_RegReg(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case UNIT_TYPE_UINT64:
                case UNIT_TYPE_INT64: {
                    break;
                }
            }
            EMIT(store_register(compile_context, destination, REG_SCRATCH));
            break;
        }

        case _UNIT_I_READ_BYTES: {
            EMIT(load_register(compile_context, REG_SCRATCH, left));
            switch (right->value) {
                case 1: {
                    EMIT(AMD64_MoveZeroExtend_RegDerefByte(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case 2: {
                    EMIT(AMD64_MoveZeroExtend_RegDerefWord(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case 4: {
                    EMIT(AMD64_Move_RegDerefDword(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                case 8: {
                    EMIT(AMD64_Move_RegDerefQword(buffer, REG_SCRATCH, REG_SCRATCH));
                    break;
                }
                default: {
                    _UNIT_Unreachable();
                }
            }
            EMIT(store_register(compile_context, destination, REG_SCRATCH));
            break;
        }

        case _UNIT_I_WRITE_BYTES: {
            AMD64_StackSlot saved;
            EMIT(save_register(compile_context, REG_R10, &saved));
            EMIT(load_register(compile_context, REG_SCRATCH, destination));
            EMIT(load_register(compile_context, REG_R10, left));
            AMD64_Indirect address = { .reg = REG_SCRATCH };
            switch (right->value) {
                case 1: {
                    EMIT(AMD64_Move8_IndirectReg(buffer, address, REG_R10));
                    break;
                }
                case 2: {
                    EMIT(AMD64_Move16_IndirectReg(buffer, address, REG_R10));
                    break;
                }
                case 4: {
                    EMIT(AMD64_Move32_IndirectReg(buffer, address, REG_R10));
                    break;
                }
                case 8: {
                    EMIT(AMD64_Move_IndirectReg(buffer, address, REG_R10));
                    break;
                }
                default: {
                    _UNIT_Unreachable();
                }
            }
            EMIT(restore_register(compile_context, REG_R10, saved));
            break;
        }

        case _UNIT_I_ADDRESS_OF:
            EMIT(AMD64_LoadEffectiveAddress_RegStack(buffer, REG_SCRATCH, item_stack_slot(left)));
            EMIT(store_register(compile_context, destination, REG_SCRATCH));
            break;
    }
    return _UNIT_OK;
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
