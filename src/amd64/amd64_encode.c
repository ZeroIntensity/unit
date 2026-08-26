#include <unit/base.h>

#include <unit/internal/architectures.h>
#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>
#include <stdio.h>
#include "amd64_local.h"

// Actual opcodes
enum {
    // Moves
    OPCODE_MOV_RM64_R64 = 0x89,
    OPCODE_MOV_R64_RM64 = 0x8B,
    OPCODE_MOV_R64_IMM64 = 0xB8,
    OPCODE_MOV_RM8_R8 = 0x88,

    // Moves with zero extension
    OPCODE_OPERAND_SIZE_PREFIX = 0x66,
    OPCODE_MOVZX_R_RM8_0 = 0x0F,
    OPCODE_MOVZX_R_RM8_1 = 0xB6,
    OPCODE_MOVZX_R_RM16_0 = 0x0F,
    OPCODE_MOVZX_R_RM16_1 = 0xB7,
    OPCODE_MOVSX_R_RM8_0 = 0x0F,
    OPCODE_MOVSX_R_RM8_1 = 0xBE,
    OPCODE_MOVSX_R_RM16_0 = 0x0F,
    OPCODE_MOVSX_R_RM16_1 = 0xBF,
    OPCODE_MOVSXD_R64_RM32 = 0x63,

    // Syscalls
    OPCODE_SYSCALL_0 = 0x0f,
    OPCODE_SYSCALL_1 = 0x05,

    // Comparisons
    OPCODE_CMP_RM64_IMM8  = 0x83,
    OPCODE_CMP_RM64_R64 = 0x39,
    OPCODE_CMP_R64_RM64 = 0x3B,

    // Jumps
    OPCODE_JMP_REL32 = 0xE9,
    OPCODE_JCC_REL32 = 0x0F,
    OPCODE_JE_REL32 = 0x84,
    OPCODE_JNE_REL32 = 0x85,
    OPCODE_JL_REL32 = 0x8C,
    OPCODE_JGE_REL32 = 0x8D,
    OPCODE_JLE_REL32 = 0x8E,
    OPCODE_JG_REL32 = 0x8F,

    // Arithmetic
    OPCODE_ADD_RM64_R64 = 0x01,
    OPCODE_SUB_RM64_IMM8 = 0x83,
    OPCODE_SUB_RM64_R64 = 0x29,
    OPCODE_IMUL_R64_RM64_0 = 0x0F,
    OPCODE_IMUL_R64_RM64_1 = 0xAF,
    OPCODE_IMUL_R64_RM64_IMM32 = 0x69,
    OPCODE_IMUL_R64_RM64_IMM8 = 0x6B,
    OPCODE_IDIV_RM64 = 0xF7,

    // Misc
    OPCODE_RET = 0xc3,
    OPCODE_CALL_REL32 = 0xE8,
    OPCODE_LEA = 0x8D,
    OPCODE_CQO = 0x99,
    OPCODE_NOP = 0x90,
};

// Multi-purpose opcodes (specify the actual thing using ModRM)
enum {
    OPCODE_GROUP5 = 0xFF,
    OPCODE_GROUP1_IMM8 = 0x83,
    OPCODE_GROUP1_IMM32 = 0x81,
};

// Group 1
enum {
    GROUP1_ADD = 0,
    GROUP1_OR = 1,
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
rex(uint8_t w,
    uint8_t r,
    uint8_t x,
    uint8_t b) {
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
modrm(ModRM_Mode mode,
      uint8_t reg,
      uint8_t rm) {
    return
        (mode << 6) |
        (reg << 3) |
        rm;
}

#define EMIT8(value)                                              \
        if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(buffer, value))) { \
            return _UNIT_FAIL;                                    \
        }

#define EMIT32(value)                                              \
        if (UNIT_FAILED(_UNIT_CodeBuffer_Emit32(buffer, value))) { \
            return _UNIT_FAIL;                                     \
        }

#define EMIT64(value)                                              \
        if (UNIT_FAILED(_UNIT_CodeBuffer_Emit64(buffer, value))) { \
            return _UNIT_FAIL;                                     \
        }

static inline uint8_t
needs_rex_r(AMD64_Register reg) {
    return reg >= 8;
}

static inline uint8_t
reg_bits(AMD64_Register reg) {
    return reg & 0x7;
}

// First argument is the register in the ModRM reg field (REX.R),
// second argument is the register in ModRM rm field (REX.B).
// Pass 0 when that field isn't a register.
#define EMIT_REX(r_reg, b_reg) EMIT8(rex(1, needs_rex_r(r_reg), 0, needs_rex_r(b_reg)))

static inline UNIT_Status
emit_relocation(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    *relocation_index = _UNIT_CodeBuffer_CurrentIndex(buffer);
    EMIT32(0);
    return _UNIT_OK;
}

#define EMIT_RELOCATION()                                             \
        if (UNIT_FAILED(emit_relocation(buffer, relocation_index))) { \
            return _UNIT_FAIL;                                        \
        }

static UNIT_Status
emit_stack_slot(_UNIT_CodeBuffer *buffer, uint32_t stack_offset, AMD64_Register dst)
{
    if (stack_offset == 0) {
        EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), REG_RSP));
        EMIT8(SIB_RSP_BASE);
    } else if (stack_offset <= 127) {
        EMIT8(modrm(MOD_INDIRECT_DISP8,
                    reg_bits(dst),
                    REG_RSP));
        EMIT8(SIB_RSP_BASE);
        EMIT8(stack_offset);
    } else {
        EMIT8(modrm(MOD_INDIRECT_DISP32,
                    reg_bits(dst),
                    REG_RSP));
        EMIT8(SIB_RSP_BASE);
        EMIT32(stack_offset);
    }

    return _UNIT_OK;
}

#define EMIT_STACK_SLOT(stack_offset, dst)                             \
        if (UNIT_FAILED(emit_stack_slot(buffer, stack_offset, dst))) { \
            return _UNIT_FAIL;                                         \
        }

UNIT_Status
AMD64_Emit_Mov(_UNIT_CodeBuffer *buffer, AMD64_Operand dst, AMD64_Operand src)
{
    assert(dst.kind != OPERAND_IMMEDIATE);

    // reg = mov(imm64) (register encoded in opcode byte, uses REX.B)
    if (dst.kind == OPERAND_REGISTER &&
        src.kind == OPERAND_IMMEDIATE) {
        EMIT_REX(0, dst.reg);
        EMIT8(OPCODE_MOV_R64_IMM64 + reg_bits(dst.reg));
        EMIT64(src.immediate);
    }
    // reg = mov(reg)
    else if (dst.kind == OPERAND_REGISTER &&
             src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, dst.reg);
        EMIT8(OPCODE_MOV_RM64_R64);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(src.reg),
                    reg_bits(dst.reg)));
    }
    // [rsp + offset] = mov(reg)
    else if (dst.kind == OPERAND_STACK &&
             src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, 0);
        EMIT8(OPCODE_MOV_RM64_R64);
        EMIT_STACK_SLOT(dst.stack_offset, src.reg);
    }
    // reg = mov([rsp + offset]) (dst.reg in reg field, RSP in rm)
    else if (dst.kind == OPERAND_REGISTER &&
             src.kind == OPERAND_STACK) {
        EMIT_REX(dst.reg, 0);
        EMIT8(OPCODE_MOV_R64_RM64);
        EMIT_STACK_SLOT(src.stack_offset, dst.reg)
    }
    // reg = mov([reg])
    else if (dst.kind == OPERAND_REGISTER &&
             src.kind == OPERAND_INDIRECT) {
        EMIT_REX(dst.reg, src.reg);
        EMIT8(OPCODE_MOV_R64_RM64);
        EMIT8(modrm(MOD_INDIRECT,
                    reg_bits(dst.reg),
                    reg_bits(src.reg)));
    }
    // [reg] = mov(reg)
    else if (dst.kind == OPERAND_INDIRECT &&
             src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, dst.reg);
        EMIT8(OPCODE_MOV_RM64_R64);
        EMIT8(modrm(MOD_INDIRECT,
                    reg_bits(src.reg),
                    reg_bits(dst.reg)));
    }
    // [reg] = mov(imm)
    else if (dst.kind == OPERAND_INDIRECT &&
             src.kind == OPERAND_IMMEDIATE) {
        EMIT_REX(0, REG_R11);
        EMIT8(OPCODE_MOV_R64_IMM64 + reg_bits(REG_R11));
        EMIT64(src.immediate);
        EMIT_REX(REG_R11, dst.reg);
        EMIT8(OPCODE_MOV_RM64_R64);
        EMIT8(modrm(MOD_INDIRECT,
                    reg_bits(REG_R11),
                    reg_bits(dst.reg)));
    } else {
        // It's easier for refactoring to use an unreachable else rather
        // than asserting.
        _UNIT_Unreachable();
    }

    return _UNIT_OK;
}

/* reg64 = movzx(reg8)
 * Zero-extend an 8-bit value (stored in src) to a 64-bit value and store it in dst. */
UNIT_Status
AMD64_Emit_Movzx8(_UNIT_CodeBuffer *buffer,
                  AMD64_Register src,
                  AMD64_Register dst)
{
    EMIT_REX(dst, src);
    EMIT8(OPCODE_MOVZX_R_RM8_0);
    EMIT8(OPCODE_MOVZX_R_RM8_1);
    EMIT8(modrm(MOD_REGISTER, reg_bits(dst), reg_bits(src)));
    return _UNIT_OK;
}

/* movsx reg64, reg8 — sign-extend byte to 64-bit */
UNIT_Status
AMD64_Emit_Movsx8(_UNIT_CodeBuffer *buffer,
                  AMD64_Register dst,
                  AMD64_Register src)
{
    EMIT_REX(dst, src);
    EMIT8(OPCODE_MOVSX_R_RM8_0);
    EMIT8(OPCODE_MOVSX_R_RM8_1);
    EMIT8(modrm(MOD_REGISTER, reg_bits(dst), reg_bits(src)));
    return _UNIT_OK;
}

/* movzx reg64, reg16 — zero-extend word to 64-bit */
UNIT_Status
AMD64_Emit_Movzx16(_UNIT_CodeBuffer *buffer,
                   AMD64_Register dst,
                   AMD64_Register src)
{
    EMIT_REX(dst, src);
    EMIT8(OPCODE_MOVZX_R_RM16_0);
    EMIT8(OPCODE_MOVZX_R_RM16_1);
    EMIT8(modrm(MOD_REGISTER, reg_bits(dst), reg_bits(src)));
    return _UNIT_OK;
}

/* movsx reg64, reg16 — sign-extend word to 64-bit */
UNIT_Status
AMD64_Emit_Movsx16(_UNIT_CodeBuffer *buffer,
                   AMD64_Register dst,
                   AMD64_Register src)
{
    EMIT_REX(dst, src);
    EMIT8(OPCODE_MOVSX_R_RM16_0);
    EMIT8(OPCODE_MOVSX_R_RM16_1);
    EMIT8(modrm(MOD_REGISTER, reg_bits(dst), reg_bits(src)));
    return _UNIT_OK;
}

/* mov reg32, reg32 — 32-bit move, implicit zero-extend to 64-bit */
UNIT_Status
AMD64_Emit_Mov32(_UNIT_CodeBuffer *buffer,
                 AMD64_Register dst,
                 AMD64_Register src)
{
    EMIT8(rex(0, needs_rex_r(src), 0, needs_rex_r(dst)));
    EMIT8(OPCODE_MOV_RM64_R64);
    EMIT8(modrm(MOD_REGISTER, reg_bits(src), reg_bits(dst)));
    return _UNIT_OK;
}

/* movsxd reg64, reg32 — sign-extend dword to 64-bit */
UNIT_Status
AMD64_Emit_Movsxd(_UNIT_CodeBuffer *buffer,
                  AMD64_Register dst,
                  AMD64_Register src)
{
    EMIT8(rex(1, needs_rex_r(dst), 0, needs_rex_r(src)));
    EMIT8(OPCODE_MOVSXD_R64_RM32);
    EMIT8(modrm(MOD_REGISTER, reg_bits(dst), reg_bits(src)));
    return _UNIT_OK;
}

/* ---- Memory to register (READ_BYTES) ---- */

/* movzx reg64, byte [ptr] — zero-extend byte from memory */
UNIT_Status
AMD64_Emit_Movzx8_Deref(_UNIT_CodeBuffer *buffer,
                        AMD64_Register dst,
                        AMD64_Register ptr)
{
    EMIT_REX(dst, ptr);
    EMIT8(OPCODE_MOVZX_R_RM8_0);
    EMIT8(OPCODE_MOVZX_R_RM8_1);
    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), reg_bits(ptr)));
    return _UNIT_OK;
}

/* movzx reg64, word [ptr] — zero-extend word from memory */
UNIT_Status
AMD64_Emit_Movzx16_Deref(_UNIT_CodeBuffer *buffer,
                         AMD64_Register dst,
                         AMD64_Register ptr)
{
    EMIT_REX(dst, ptr);
    EMIT8(OPCODE_MOVZX_R_RM16_0);
    EMIT8(OPCODE_MOVZX_R_RM16_1);
    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), reg_bits(ptr)));
    return _UNIT_OK;
}

/* mov reg32, dword [ptr] — 32-bit load, implicit zero-extend */
UNIT_Status
AMD64_Emit_Mov32_Deref(_UNIT_CodeBuffer *buffer,
                       AMD64_Register dst,
                       AMD64_Register ptr)
{
    EMIT8(rex(0, needs_rex_r(dst), 0, needs_rex_r(ptr)));
    EMIT8(OPCODE_MOV_R64_RM64);
    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), reg_bits(ptr)));
    return _UNIT_OK;
}

/* mov reg64, qword [ptr] — full 64-bit load */
UNIT_Status
AMD64_Emit_Mov64_Deref(_UNIT_CodeBuffer *buffer,
                       AMD64_Register dst,
                       AMD64_Register ptr)
{
    EMIT_REX(dst, ptr);
    EMIT8(OPCODE_MOV_R64_RM64);
    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), reg_bits(ptr)));
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Cmp(_UNIT_CodeBuffer *buffer, AMD64_Operand src, AMD64_Register dst)
{
    // reg = cmp(imm8)
    if (src.kind == OPERAND_IMMEDIATE) {
        EMIT_REX(0, dst);
        EMIT8(OPCODE_CMP_RM64_IMM8);
        EMIT8(modrm(MOD_REGISTER, GROUP1_CMP, reg_bits(dst)));
        EMIT8(src.immediate);
    }
    // reg = cmp(reg)
    else if (src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, dst);
        EMIT8(OPCODE_CMP_RM64_R64);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(src.reg),
                    reg_bits(dst)));
    }
    // reg = cmp([rsp + offset])
    else {
        assert(src.kind == OPERAND_STACK);
        EMIT_REX(dst, 0);
        EMIT8(OPCODE_CMP_R64_RM64);
        EMIT_STACK_SLOT(src.immediate, dst);
    }

    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_CallIndirect(_UNIT_CodeBuffer *buffer, AMD64_Register target)
{
    EMIT_REX(0, target);
    EMIT8(OPCODE_GROUP5);
    EMIT8(modrm(MOD_REGISTER, GROUP5_CALL, reg_bits(target)));
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_CallSymbol(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_CALL_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Syscall(_UNIT_CodeBuffer *buffer)
{
    EMIT8(OPCODE_SYSCALL_0);
    EMIT8(OPCODE_SYSCALL_1);
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Add(_UNIT_CodeBuffer *buffer, AMD64_Operand src, AMD64_Register dst)
{
    // reg1 = add(reg1, reg2)
    if (src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, dst);
        EMIT8(OPCODE_ADD_RM64_R64);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(src.reg),
                    reg_bits(dst)));
    }
    // reg = add(reg, imm32)
    else {
        // Group opcode, dst in rm field
        assert(src.kind == OPERAND_IMMEDIATE);
        EMIT_REX(0, dst);
        EMIT8(OPCODE_GROUP1_IMM32);
        EMIT8(modrm(MOD_REGISTER, GROUP1_ADD, reg_bits(dst)));
        EMIT32((uint32_t)src.immediate);
    }

    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Sub(_UNIT_CodeBuffer *buffer, AMD64_Operand src, AMD64_Register dst)
{
    // reg1 = sub(reg1, reg2)
    if (src.kind == OPERAND_REGISTER) {
        EMIT_REX(src.reg, dst);
        EMIT8(OPCODE_SUB_RM64_R64);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(src.reg),
                    reg_bits(dst)));
    }
    // reg = sub(reg, imm32)
    else {
        assert(src.kind == OPERAND_IMMEDIATE);
        EMIT_REX(0, dst);
        EMIT8(OPCODE_GROUP1_IMM32);
        EMIT8(modrm(MOD_REGISTER, GROUP1_SUB, reg_bits(dst)));
        EMIT32((uint32_t)src.immediate);
    }

    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Mul(_UNIT_CodeBuffer *buffer, AMD64_Operand src, AMD64_Register dst)
{
    // reg = imul(reg, imm32)
    if (src.kind == OPERAND_IMMEDIATE) {
        int64_t value = src.immediate;
        EMIT_REX(dst, dst);
        EMIT8(value >= -128 && value <= 127
            ? OPCODE_IMUL_R64_RM64_IMM8
            : OPCODE_IMUL_R64_RM64_IMM32);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(dst),
                    reg_bits(dst)));
        if (value >= -128 && value <= 127) {
            EMIT8((uint8_t)(value & 0xFF));
        } else {
            EMIT32((uint32_t)value);
        }
    } else {
        // reg1 = imul(reg1, reg2)
        assert(src.kind == OPERAND_REGISTER);
        EMIT_REX(dst, src.reg);
        EMIT8(OPCODE_IMUL_R64_RM64_0);
        EMIT8(OPCODE_IMUL_R64_RM64_1);
        EMIT8(modrm(MOD_REGISTER,
                    reg_bits(dst),
                    reg_bits(src.reg)));
    }

    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Div(_UNIT_CodeBuffer *buffer, AMD64_Register divisor)
{
    EMIT_REX(0, divisor);
    EMIT8(OPCODE_IDIV_RM64);
    EMIT8(modrm(MOD_REGISTER, 7, reg_bits(divisor)));
    return _UNIT_OK;
}

/* "Load effective address".
 * Load the address of a stack slot into a register. */
UNIT_Status
AMD64_Emit_Lea(_UNIT_CodeBuffer *buffer, AMD64_Register dst, uint32_t stack_offset)
{
    EMIT_REX(dst, 0);
    EMIT8(OPCODE_LEA);
    EMIT_STACK_SLOT(stack_offset, dst);
    return _UNIT_OK;
}

/* Load the address of a "displacement".
 * Basically, an offset from where the current instruction is in memory.
 */
UNIT_Status
AMD64_Emit_LeaRip(_UNIT_CodeBuffer *buffer,
                  AMD64_Register dst,
                  UNIT_Size *relocation_index)
{
    EMIT_REX(dst, 0);
    EMIT8(OPCODE_LEA);
    EMIT8(modrm(MOD_INDIRECT, reg_bits(dst), 5));
    EMIT_RELOCATION();
    return _UNIT_OK;
}

/* "Convert quadword to octoword".
 * In normal person terms, this just means that it converts a 64-bit integer
 * (stored in RAX) into an 128-bit integer (stored in RDX). */
UNIT_Status
AMD64_Emit_Cqo(_UNIT_CodeBuffer *buffer)
{
    EMIT8(rex(1, 0, 0, 0));
    EMIT8(OPCODE_CQO);
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Jump(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JMP_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpEqual(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JE_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpNotEqual(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JNE_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpGreater(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JG_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpGreaterEqual(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JGE_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpLess(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JL_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_JumpLessEqual(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index)
{
    EMIT8(OPCODE_JCC_REL32);
    EMIT8(OPCODE_JLE_REL32);
    EMIT_RELOCATION();
    return _UNIT_OK;
}

UNIT_Status
AMD64_Emit_Ret(_UNIT_CodeBuffer *buffer)
{
    EMIT8(OPCODE_RET);
    return _UNIT_OK;
}

UNIT_Status
AMD64_encode_instruction(_UNIT_CompileContext *compile_context,
                         AMD64_Instruction *instr)
{
    assert(compile_context != NULL);
    assert(instr != NULL);
    switch (instr->opcode) {


        /* Moves */

        case AMD64_MOV: {


            break;
        }

        /* Moves with zero extension */

        case AMD64_MOVZX: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];
            UNIT_Size size = instr->operands[2].immediate;
            assert(dst.kind == OPERAND_REGISTER);
            assert(src.kind == OPERAND_INDIRECT);

            switch (size) {
                case 1: {
                    EMIT_REX(dst.reg, src.reg);
                    EMIT8(OPCODE_MOVZX_R_RM8_0);
                    EMIT8(OPCODE_MOVZX_R_RM8_1);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(dst.reg),
                                reg_bits(src.reg)));
                    break;
                }
                case 2: {
                    EMIT_REX(dst.reg, src.reg);
                    EMIT8(OPCODE_MOVZX_R_RM16_0);
                    EMIT8(OPCODE_MOVZX_R_RM16_1);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(dst.reg),
                                reg_bits(src.reg)));
                    break;
                }
                case 4: {
                    EMIT8(rex(0,
                              needs_rex_r(dst.reg),
                              0,
                              needs_rex_r(src.reg)));
                    EMIT8(OPCODE_MOV_R64_RM64);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(dst.reg),
                                reg_bits(src.reg)));
                    break;
                }
                case 8: {
                    EMIT_REX(dst.reg, src.reg);
                    EMIT8(OPCODE_MOV_R64_RM64);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(dst.reg),
                                reg_bits(src.reg)));
                    break;
                }
                default: {
                    _UNIT_Unreachable();
                }
            }
            break;
        }

        case AMD64_MOV_SIZED: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];
            UNIT_Size size = instr->operands[2].immediate;
            assert(dst.kind == OPERAND_INDIRECT);
            assert(src.kind == OPERAND_REGISTER);

            switch (size) {
                case 1: {
                    EMIT_REX(src.reg, dst.reg);
                    EMIT8(OPCODE_MOV_RM8_R8);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(src.reg),
                                reg_bits(dst.reg)));
                    break;
                }
                case 2: {
                    EMIT8(OPCODE_OPERAND_SIZE_PREFIX);
                    EMIT_REX(src.reg, dst.reg);
                    EMIT8(OPCODE_MOV_RM64_R64);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(src.reg),
                                reg_bits(dst.reg)));
                    break;
                }
                case 4: {
                    EMIT8(rex(0,
                              needs_rex_r(src.reg),
                              0,
                              needs_rex_r(dst.reg)));
                    EMIT8(OPCODE_MOV_RM64_R64);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(src.reg),
                                reg_bits(dst.reg)));
                    break;
                }
                case 8: {
                    EMIT_REX(src.reg, dst.reg);
                    EMIT8(OPCODE_MOV_RM64_R64);
                    EMIT8(modrm(MOD_INDIRECT,
                                reg_bits(src.reg),
                                reg_bits(dst.reg)));
                    break;
                }
                default: {
                    _UNIT_Unreachable();
                }
            }
            break;
        }


        #define TWO_BYTE_REG_REG_CASE(name, byte0, byte1)       \
                case name: {                                    \
                        AMD64_Operand dst = instr->operands[0]; \
                        AMD64_Operand src = instr->operands[1]; \
                        assert(dst.kind == OPERAND_REGISTER);   \
                        assert(src.kind == OPERAND_REGISTER);   \
                        EMIT_REX(dst.reg, src.reg);             \
                        EMIT8(byte0);                           \
                        EMIT8(byte1);                           \
                        EMIT8(modrm(MOD_REGISTER,               \
                                    reg_bits(dst.reg),          \
                                    reg_bits(src.reg)));        \
                        break;                                  \
                }

        #define ONE_BYTE_REG_REG_CASE(name, opcode, use_rex_w)  \
                case name: {                                    \
                        AMD64_Operand dst = instr->operands[0]; \
                        AMD64_Operand src = instr->operands[1]; \
                        assert(dst.kind == OPERAND_REGISTER);   \
                        assert(src.kind == OPERAND_REGISTER);   \
                        EMIT8(rex(use_rex_w,                    \
                                  needs_rex_r(dst.reg),         \
                                  0,                            \
                                  needs_rex_r(src.reg)));       \
                        EMIT8(opcode);                          \
                        EMIT8(modrm(MOD_REGISTER,               \
                                    reg_bits(dst.reg),          \
                                    reg_bits(src.reg)));        \
                        break;                                  \
                }

            TWO_BYTE_REG_REG_CASE(AMD64_MOVZX8,
                                  OPCODE_MOVZX_R_RM8_0,
                                  OPCODE_MOVZX_R_RM8_1)
            TWO_BYTE_REG_REG_CASE(AMD64_MOVSX8,
                                  OPCODE_MOVSX_R_RM8_0,
                                  OPCODE_MOVSX_R_RM8_1)
            TWO_BYTE_REG_REG_CASE(AMD64_MOVZX16,
                                  OPCODE_MOVZX_R_RM16_0,
                                  OPCODE_MOVZX_R_RM16_1)
            TWO_BYTE_REG_REG_CASE(AMD64_MOVSX16,
                                  OPCODE_MOVSX_R_RM16_0,
                                  OPCODE_MOVSX_R_RM16_1)
            ONE_BYTE_REG_REG_CASE(AMD64_MOVSXD, OPCODE_MOVSXD_R64_RM32, 1)

        // MOV32 has a swapped dest and src so we can't use the macro
        case AMD64_MOV32: {
            AMD64_Operand dst = instr->operands[0];
            AMD64_Operand src = instr->operands[1];
            assert(dst.kind == OPERAND_REGISTER);
            assert(src.kind == OPERAND_REGISTER);
            EMIT8(rex(0, needs_rex_r(src.reg), 0, needs_rex_r(dst.reg)));
            EMIT8(OPCODE_MOV_RM64_R64);
            EMIT8(modrm(MOD_REGISTER, reg_bits(src.reg), reg_bits(dst.reg)));
            break;
        }

        #undef TWO_BYTE_REG_REG_CASE
        #undef ONE_BYTE_REG_REG_CASE

        case AMD64_LOAD_STRING: {
            AMD64_Operand dst = instr->operands[0];
            UNIT_Size string_index = instr->operands[1].immediate;
            _UNIT_SizeMap *string_offsets =
                &compile_context->string_data.string_offsets;
            UNIT_Size byte_offset = _UNIT_SizeMap_GET(string_offsets,
                                                      string_index);

            assert(dst.kind == OPERAND_REGISTER);
            // lea reg, [rip + disp32] (dst in reg field, rm=5 for RIP-relative)
            EMIT_REX(dst.reg, 0);
            EMIT8(OPCODE_LEA);
            EMIT8(modrm(MOD_INDIRECT, reg_bits(dst.reg), 5));
            EMIT32(byte_offset);

            _UNIT_Relocation *relocation = _UNIT_Relocation_NewData(compile_context->context,
                                                                    INDEX(),
                                                                    byte_offset);
            if (relocation == NULL) {
                goto error;
            }

            if (UNIT_FAILED(_UNIT_Vector_Append(
                                &compile_context->symbol_table.relocations,
                                relocation))) {
                goto error;
            }

            EMIT32(byte_offset);
            break;
        }
    }

    _UNIT_Dealloc(compile_context->context, instr);
    return _UNIT_OK;
error:
    _UNIT_Dealloc(compile_context->context, instr);
    return _UNIT_FAIL;
}

void
AMD64_PatchPrologue(_UNIT_CompileContext *compile_context,
                    UNIT_Size prologue_offset,
                    UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(prologue_offset >= 0);
    assert(frame_size >= 0);
    if (frame_size == 0) {
        uint8_t nops[] = {
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP
        };
        _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer,
                                    prologue_offset,
                                    nops,
                                    7);
        return;
    }

    uint8_t prologue[] = {
        rex(1, 0, 0, 0),
        OPCODE_GROUP1_IMM32,
        modrm(MOD_REGISTER, GROUP1_SUB, REG_RSP),
        (frame_size >> 0) & 0xFF,
        (frame_size >> 8) & 0xFF,
        (frame_size >> 16) & 0xFF,
        (frame_size >> 24) & 0xFF,
    };
    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer,
                                prologue_offset,
                                prologue,
                                7);
}

void
AMD64_PatchEpilogue(_UNIT_CompileContext *compile_context,
                    UNIT_Size epilogue_offset,
                    UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(epilogue_offset >= 0);
    assert(frame_size >= 0);
    if (frame_size == 0) {
        uint8_t nops[] = {
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP,
            OPCODE_NOP
        };
        _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer,
                                    epilogue_offset,
                                    nops,
                                    7);
        return;
    }

    uint8_t epilogue[] = {
        rex(1, 0, 0, 0),
        OPCODE_GROUP1_IMM32,
        modrm(MOD_REGISTER, GROUP1_ADD, REG_RSP),
        (frame_size >> 0) & 0xFF,
        (frame_size >> 8) & 0xFF,
        (frame_size >> 16) & 0xFF,
        (frame_size >> 24) & 0xFF,
    };
    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer,
                                epilogue_offset,
                                epilogue,
                                7);
}

void
AMD64_PatchJumps(_UNIT_CompileContext *compile_context)
{
    assert(compile_context != NULL);
    UNIT_Size count =
        _UNIT_Vector_SIZE(&compile_context->jump_table.pending_jumps);

    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_PendingJump *jump =
            _UNIT_Vector_GET(&compile_context->jump_table.pending_jumps,
                             index);
        UNIT_Size label_offset =
            _UNIT_SizeMap_GET(&compile_context->jump_table.label_offsets,
                              jump->label_index);

        UNIT_Size instruction_end = jump->patch_offset + 4;
        int32_t displacement = (int32_t)(label_offset - instruction_end);
        _UNIT_CodeBuffer_Patch32(&compile_context->buffer,
                                 jump->patch_offset,
                                 displacement);
    }
}
