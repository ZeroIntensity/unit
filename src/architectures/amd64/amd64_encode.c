#include <unit/base.h>

#include <unit/internal/architectures.h>
#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>

#include "amd64_local.h"

// Actual opcodes
enum {
    OPCODE_MOV_RM64_R64  = 0x89,
    OPCODE_MOV_R64_RM64  = 0x8B,
    OPCODE_MOV_R64_IMM64 = 0xB8,
    OPCODE_ADD_RM64_R64 = 0x01,
    OPCODE_SUB_RM64_IMM8 = 0x83,
    OPCODE_RET = 0xc3,
    OPCODE_SYSCALL_0 = 0x0f,
    OPCODE_SYSCALL_1 = 0x05,
    OPCODE_CALL_REL32 = 0xE8,
    OPCODE_JMP_REL32 = 0xE9,
    OPCODE_CMP_RM64_IMM8  = 0x83,
    OPCODE_CMP_RM64_R64 = 0x39,
    OPCODE_JCC_REL32 = 0x0F,
    OPCODE_JE_REL32 = 0x84,
    OPCODE_JNE_REL32 = 0x85,
    OPCODE_JL_REL32 = 0x8C,
    OPCODE_JGE_REL32 = 0x8D,
    OPCODE_JLE_REL32 = 0x8E,
    OPCODE_JG_REL32 = 0x8F,
    OPCODE_LEA = 0x8D,
};

// Multi-purpose opcodes (specify the actual thing using ModRM)
enum {
    OPCODE_GROUP5 = 0xFF,
    OPCODE_GROUP1_IMM8  = 0x83,
    OPCODE_GROUP1_IMM32 = 0x81,
};

// Group 1
enum {
    GROUP1_ADD = 0,
    GROUP1_OR  = 1,
    GROUP1_AND = 4,
    GROUP1_SUB = 5,
    GROUP1_XOR = 6,
    GROUP1_CMP = 7,
};

// Group 5
enum {
    GROUP5_CALL = 2,
    GROUP5_JMP  = 4,
    GROUP5_PUSH = 6,
};

enum {
    REX = 0x40,
    REX_W = 0x08,
    REX_R = 0x04,
    REX_X = 0x02,
    REX_B = 0x01,
};

enum {
    SIB_RSP_BASE = 0x24,
};

static uint8_t
rex(uint8_t w, uint8_t r, uint8_t x, uint8_t b) {
    return
        REX |
        (w ? REX_W : 0) |
        (r ? REX_R : 0) |
        (x ? REX_X : 0) |
        (b ? REX_B : 0);
}

typedef enum {
    MOD_INDIRECT = 0x0,
    MOD_INDIRECT_DISP8 = 0x1,
    MOD_INDIRECT_DISP32 = 0x2,
    MOD_REGISTER = 0x3,
} ModRM_Mode;

static uint8_t
modrm(ModRM_Mode mode, uint8_t reg, uint8_t rm) {
    return
        (mode << 6) |
        (reg << 3) |
        rm;
}

static UNIT_Status
add_jump(_UNIT_CompileContext *compile_context, UNIT_Size label_index)
{
    assert(compile_context != NULL);
    _UNIT_PendingJump *jump = _UNIT_PendingJump_New(
        compile_context->context,
        _UNIT_CodeBuffer_CurrentIndex(&compile_context->buffer),
        label_index
    );
    if (jump == NULL) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->jump_table.pending_jumps,
                                        jump))) {
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

static inline uint8_t
needs_rex_r(AMD64_Register reg) {
    return reg >= 8;
}

static inline uint8_t
reg_bits(AMD64_Register reg) {
    return reg & 0x7;
}

UNIT_Status
AMD64_encode_instruction(_UNIT_CompileContext *compile_context,
                         AMD64_Instruction *instr)
{
    assert(compile_context != NULL);
    assert(instr != NULL);
    switch (instr->opcode) {

#define EMIT8(value)                                                                \
    if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(&compile_context->buffer, value))) {     \
        goto error;                                                                 \
    }
#define EMIT32(value)                                                               \
    if (UNIT_FAILED(_UNIT_CodeBuffer_Emit32(&compile_context->buffer, value))) {    \
        goto error;                                                                 \
    }
#define EMIT64(value)                                                               \
    if (UNIT_FAILED(_UNIT_CodeBuffer_Emit64(&compile_context->buffer, value))) {    \
        goto error;                                                                 \
    }
#define INDEX() _UNIT_CodeBuffer_CurrentIndex(&compile_context->buffer)
#define EMIT_JUMP(label_index)                                      \
    if (UNIT_FAILED(add_jump(compile_context, label_index))) {      \
        goto error;                                                 \
    }                                                               \
    EMIT32(0x00);

// First argument is the register in the ModRM reg field (REX.R),
// second argument is the register in ModRM rm field (REX.B).
// Pass 0 when that field isn't a register.
#define EMIT_REX(r_reg, b_reg)                                  \
    EMIT8(rex(1, needs_rex_r(r_reg), 0, needs_rex_r(b_reg)))

        case AMD64_RET: {
            EMIT8(OPCODE_RET);
            break;
        }

        case AMD64_SYSCALL: {
            EMIT8(OPCODE_SYSCALL_0);
            EMIT8(OPCODE_SYSCALL_1);
            break;
        }

        case AMD64_MOV: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];

            // mov reg, imm64 (register encoded in opcode byte, uses REX.B)
            if (dst.kind == OPERAND_REGISTER && src.kind == OPERAND_IMMEDIATE) {
                EMIT_REX(0, dst.reg);
                EMIT8(OPCODE_MOV_R64_IMM64 + reg_bits(dst.reg));
                EMIT64(src.immediate);
            }
            // mov reg, reg (src in reg field, dst in rm field)
            else if (dst.kind == OPERAND_REGISTER && src.kind == OPERAND_REGISTER) {
                EMIT_REX(src.reg, dst.reg);
                EMIT8(OPCODE_MOV_RM64_R64);
                EMIT8(modrm(MOD_REGISTER, reg_bits(src.reg), reg_bits(dst.reg)));
            }
            // mov [rsp + offset], reg (src.reg in reg field, RSP in rm)
            else if (dst.kind == OPERAND_STACK && src.kind == OPERAND_REGISTER) {
                EMIT_REX(src.reg, 0);
                EMIT8(OPCODE_MOV_RM64_R64);
                if (dst.immediate == 0) {
                    EMIT8(modrm(MOD_INDIRECT, reg_bits(src.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                } else if (dst.immediate <= 127) {
                    EMIT8(modrm(MOD_INDIRECT_DISP8, reg_bits(src.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                    EMIT8((uint8_t)dst.immediate);
                } else {
                    EMIT8(modrm(MOD_INDIRECT_DISP32, reg_bits(src.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                    EMIT32((uint32_t)dst.immediate);
                }
            }
            // mov reg, [rsp + offset] (dst.reg in reg field, RSP in rm)
            else if (dst.kind == OPERAND_REGISTER && src.kind == OPERAND_STACK) {
                EMIT_REX(dst.reg, 0);
                EMIT8(OPCODE_MOV_R64_RM64);
                if (src.immediate == 0) {
                    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                } else if (src.immediate <= 127) {
                    EMIT8(modrm(MOD_INDIRECT_DISP8, reg_bits(dst.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                    EMIT8((uint8_t)src.immediate);
                } else {
                    EMIT8(modrm(MOD_INDIRECT_DISP32, reg_bits(dst.reg), REG_RSP));
                    EMIT8(SIB_RSP_BASE);
                    EMIT32((uint32_t)src.immediate);
                }
            }
            break;
        }

        case AMD64_ADD: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];

            // add reg, reg (src in reg field, dst in rm field)
            if (dst.kind == OPERAND_REGISTER && src.kind == OPERAND_REGISTER) {
                EMIT_REX(src.reg, dst.reg);
                EMIT8(OPCODE_ADD_RM64_R64);
                EMIT8(modrm(MOD_REGISTER, reg_bits(src.reg), reg_bits(dst.reg)));
            }
            // add reg, imm32 (group opcode, dst in rm field)
            else {
                assert(src.kind == OPERAND_IMMEDIATE);
                assert(dst.kind == OPERAND_REGISTER);
                EMIT_REX(0, dst.reg);
                EMIT8(OPCODE_GROUP1_IMM32);
                EMIT8(modrm(MOD_REGISTER, GROUP1_ADD, reg_bits(dst.reg)));
                EMIT32((uint32_t)src.immediate);
            }
            break;
        }

        case AMD64_CALL_INDIRECT: {
            AMD64_Operand target = instr->operands[0];
            assert(target.kind == OPERAND_REGISTER);
            EMIT_REX(0, target.reg);
            EMIT8(OPCODE_GROUP5);
            EMIT8(modrm(MOD_REGISTER, GROUP5_CALL, reg_bits(target.reg)));
            break;
        }

        case AMD64_CALL_SYMBOL: {
            EMIT8(OPCODE_CALL_REL32);
            _UNIT_Relocation *relocation = _UNIT_Relocation_NewCall(
                compile_context->context,
                INDEX(),
                instr->operands[0].immediate
            );
            if (relocation == NULL) {
                goto error;
            }
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->symbol_table.relocations,
                                                relocation))) {
                goto error;
            }
            EMIT32(0x00);
            break;
        }

        case AMD64_JUMP: {
            UNIT_Size label_index = instr->operands[0].immediate;
            EMIT8(OPCODE_JMP_REL32);
            EMIT_JUMP(label_index);
            break;
        }

        case AMD64_JUMP_LABEL: {
            UNIT_Size label_index = instr->operands[0].immediate;
            if (UNIT_FAILED(_UNIT_SizeMap_Set(&compile_context->jump_table.label_offsets,
                                            label_index,
                                            INDEX()))) {
                goto error;
            }
            break;
        }

        case AMD64_COMPARE: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];
            // cmp reg, imm (group opcode, dst in rm field)
            if (dst.kind == OPERAND_REGISTER && src.kind == OPERAND_IMMEDIATE) {
                EMIT_REX(0, dst.reg);
                EMIT8(OPCODE_CMP_RM64_IMM8);
                EMIT8(modrm(MOD_REGISTER, GROUP1_CMP, reg_bits(dst.reg)));
                EMIT8((uint8_t)src.immediate);
            }
            // cmp reg, reg (src in reg field, dst in rm field)
            else {
                assert(src.kind == OPERAND_REGISTER);
                assert(dst.kind == OPERAND_REGISTER);
                EMIT_REX(src.reg, dst.reg);
                EMIT8(OPCODE_CMP_RM64_R64);
                EMIT8(modrm(MOD_REGISTER, reg_bits(src.reg), reg_bits(dst.reg)));
            }
            break;
        }

#define CONDITIONAL_JUMP_CASE(name, condition_opcode)           \
    case name: {                                                \
        UNIT_Size label_index = instr->operands[0].immediate;  \
        EMIT8(OPCODE_JCC_REL32);                                \
        EMIT8(condition_opcode);                                \
        EMIT_JUMP(label_index);                                 \
        break;                                                  \
    }

        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_EQUAL, OPCODE_JE_REL32)
        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_NOT_EQUAL, OPCODE_JNE_REL32)
        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_LESS, OPCODE_JL_REL32)
        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_GREATER, OPCODE_JG_REL32)
        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_LESS_EQUAL, OPCODE_JLE_REL32)
        CONDITIONAL_JUMP_CASE(AMD64_JUMP_IF_GREATER_EQUAL, OPCODE_JGE_REL32)

#undef CONDITIONAL_JUMP_CASE

        case AMD64_LOAD_STRING: {
            AMD64_Operand dst = instr->operands[0];
            UNIT_Size string_index = instr->operands[1].immediate;
            _UNIT_SizeMap *string_offsets = &compile_context->string_data.string_offsets;
            UNIT_Size byte_offset = _UNIT_SizeMap_GET(string_offsets, string_index);

            assert(dst.kind == OPERAND_REGISTER);
            // lea reg, [rip + disp32] (dst in reg field, rm=5 for RIP-relative)
            EMIT_REX(dst.reg, 0);
            EMIT8(OPCODE_LEA);
            EMIT8(modrm(MOD_INDIRECT, reg_bits(dst.reg), 5));

            _UNIT_Relocation *relocation = _UNIT_Relocation_NewData(
                compile_context->context, INDEX(), byte_offset);
            if (relocation == NULL) {
                goto error;
            }
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->symbol_table.relocations,
                                                relocation))) {
                goto error;
            }
            EMIT32(0x00);
            break;
        }

        case AMD64_LOAD_ADDRESS: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];

            // lea reg, [rsp + offset] (dst in reg field, RSP in rm)
            assert(dst.kind == OPERAND_REGISTER);
            assert(src.kind == OPERAND_STACK);
            EMIT_REX(dst.reg, 0);
            EMIT8(OPCODE_LEA);
            if (src.immediate == 0) {
                EMIT8(modrm(MOD_INDIRECT, reg_bits(dst.reg), REG_RSP));
                EMIT8(SIB_RSP_BASE);
            } else if (src.immediate <= 127) {
                EMIT8(modrm(MOD_INDIRECT_DISP8, reg_bits(dst.reg), REG_RSP));
                EMIT8(SIB_RSP_BASE);
                EMIT8((uint8_t)src.immediate);
            } else {
                EMIT8(modrm(MOD_INDIRECT_DISP32, reg_bits(dst.reg), REG_RSP));
                EMIT8(SIB_RSP_BASE);
                EMIT32((uint32_t)src.immediate);
            }
            break;
        }

        default:
            _UNIT_Unreachable();
    }

    _UNIT_Dealloc(compile_context->context, instr);
    return UNIT_OK;
error:
    _UNIT_Dealloc(compile_context->context, instr);
    return UNIT_FAIL;
}

void
AMD64_PatchPrologue(_UNIT_CompileContext *context,
                    UNIT_Size prologue_offset)
{
    if (context->frame_size > 0) {
        if (context->frame_size % 16 != 0) {
            context->frame_size += 16 - (context->frame_size % 16);
        }

        uint8_t prologue[] = {
            rex(1, 0, 0, 0),
            OPCODE_GROUP1_IMM32,
            modrm(MOD_REGISTER, GROUP1_SUB, REG_RSP),
            (context->frame_size >> 0) & 0xFF,
            (context->frame_size >> 8) & 0xFF,
            (context->frame_size >> 16) & 0xFF,
            (context->frame_size >> 24) & 0xFF,
        };
        _UNIT_CodeBuffer_PatchBytes(&context->buffer, prologue_offset,
                                    prologue, 7);
    } else {
        uint8_t nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
        _UNIT_CodeBuffer_PatchBytes(&context->buffer, prologue_offset,
                                    nops, 7);
    }
}

void
AMD64_PatchJumps(_UNIT_CompileContext *context)
{
    assert(context != NULL);
    UNIT_Size count = _UNIT_Vector_SIZE(&context->jump_table.pending_jumps);

    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_PendingJump *jump = _UNIT_Vector_GET(&context->jump_table.pending_jumps,
                                                   index);
        UNIT_Size label_offset = _UNIT_SizeMap_GET(&context->jump_table.label_offsets,
                                                    jump->label_index);

        UNIT_Size instruction_end = jump->patch_offset + 4;
        int32_t displacement = (int32_t)(label_offset - instruction_end);
        _UNIT_CodeBuffer_Patch32(&context->buffer, jump->patch_offset,
                                 displacement);
    }
}
