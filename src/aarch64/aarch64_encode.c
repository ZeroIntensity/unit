#include <unit/base.h>

#include <unit/internal/architectures.h>
#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>

#include "aarch64_local.h"

/*
 * ARM64 instructions are always 32 bits, little-endian.
 * This file encodes AArch64_Instructions into 32-bit machine words.
 */

/* ── AArch64 instruction encoding constants ─────────────────────────── */

enum {
    /* Move: ORR Xd, XZR, Xm (64-bit) */
    A64_ORR_64_SHIFTED      = 0xAA0003E0,
    /* Move: ORR Wd, WZR, Wm (32-bit, zero-extends to 64) */
    A64_ORR_32_SHIFTED      = 0x2A0003E0,

    /* MOVZ Xd, #imm16, LSL #0 */
    A64_MOVZ_64             = 0xD2800000,
    /* MOVK Xd, #imm16, LSL #hw */
    A64_MOVK_64             = 0xF2800000,

    /* LDR (64-bit, unsigned offset): Xt, [Xn, #imm12*8] */
    A64_LDR_64_UNSIGNED     = 0xF9400000,
    /* STR (64-bit, unsigned offset): Xt, [Xn, #imm12*8] */
    A64_STR_64_UNSIGNED     = 0xF9000000,

    /* LDRB (unsigned offset 0): Wt, [Xn] */
    A64_LDRB_UNSIGNED       = 0x39400000,
    /* LDRH (unsigned offset 0): Wt, [Xn] */
    A64_LDRH_UNSIGNED       = 0x79400000,
    /* LDR (32-bit, unsigned offset 0): Wt, [Xn] */
    A64_LDR_32_UNSIGNED     = 0xB9400000,

    /* STRB (unsigned offset 0): Wt, [Xn] */
    A64_STRB_UNSIGNED       = 0x39000000,
    /* STRH (unsigned offset 0): Wt, [Xn] */
    A64_STRH_UNSIGNED       = 0x79000000,
    /* STR (32-bit, unsigned offset 0): Wt, [Xn] */
    A64_STR_32_UNSIGNED     = 0xB9000000,

    /* LDRSB (64-bit, unsigned offset 0): Xt, [Xn] */
    A64_LDRSB_64_UNSIGNED   = 0x39800000,
    /* LDRSH (64-bit, unsigned offset 0): Xt, [Xn] */
    A64_LDRSH_64_UNSIGNED   = 0x79800000,
    /* LDRSW (unsigned offset 0): Xt, [Xn] */
    A64_LDRSW_UNSIGNED      = 0xB9800000,

    /* STP (pre-index, 64-bit): Xt1, Xt2, [Xn, #imm7*8]! */
    A64_STP_PRE_64          = 0xA9800000,
    /* LDP (post-index, 64-bit): Xt1, Xt2, [Xn], #imm7*8 */
    A64_LDP_POST_64         = 0xA8C00000,

    /* BL imm26 */
    A64_BL                  = 0x94000000,
    /* BLR Xn */
    A64_BLR                 = 0xD63F0000,
    /* B imm26 (unconditional) */
    A64_B                   = 0x14000000,
    /* B.cond imm19 */
    A64_B_COND              = 0x54000000,

    /* CMP (shifted register) = SUBS XZR, Xn, Xm */
    A64_SUBS_SHIFTED_REG    = 0xEB000000,
    /* CMP (immediate) = SUBS XZR, Xn, #imm12 */
    A64_SUBS_IMMEDIATE      = 0xF1000000,

    /* ADD (shifted register, 64-bit) */
    A64_ADD_SHIFTED_REG     = 0x8B000000,
    /* ADD (immediate, 64-bit) */
    A64_ADD_IMMEDIATE       = 0x91000000,
    /* SUB (shifted register, 64-bit) */
    A64_SUB_SHIFTED_REG     = 0xCB000000,
    /* SUB (immediate, 64-bit) */
    A64_SUB_IMMEDIATE       = 0xD1000000,

    /* MADD Xd, Xn, Xm, XZR  (MUL alias) */
    A64_MADD_XZR            = 0x9B007C00,
    /* SDIV Xd, Xn, Xm */
    A64_SDIV                = 0x9AC00C00,
    /* MSUB Xd, Xn, Xm, Xa */
    A64_MSUB                = 0x9B008000,

    /* ADRP Xd, <page> */
    A64_ADRP                = 0x90000000,

    /* RET X30 */
    A64_RET                 = 0xD65F03C0,
    /* NOP */
    A64_NOP                 = 0xD503201F,

    /* UBFM Wd, Wn, #0, #7  (UXTB) */
    A64_UBFM_UXTB          = 0x53001C00,
    /* UBFM Wd, Wn, #0, #15 (UXTH) */
    A64_UBFM_UXTH          = 0x53003C00,
    /* SBFM Xd, Xn, #0, #7  (SXTB) */
    A64_SBFM_SXTB          = 0x93401C00,
    /* SBFM Xd, Xn, #0, #15 (SXTH) */
    A64_SBFM_SXTH          = 0x93403C00,
    /* SBFM Xd, Xn, #0, #31 (SXTW) */
    A64_SBFM_SXTW          = 0x93407C00,

    /* SVC #imm16 */
    A64_SVC                 = 0xD4000001,

    /* Prologue/epilogue fixed sequences */
    A64_STP_FP_LR_PRE16    = 0xA9BF7BFD, /* stp x29, x30, [sp, #-16]! */
    A64_MOV_FP_SP           = 0x910003FD, /* mov x29, sp */
    A64_SUB_SP_IMM_BASE     = 0xD10003FF, /* sub sp, sp, #imm  (OR in imm<<10) */
    A64_ADD_SP_IMM_BASE     = 0x910003FF, /* add sp, sp, #imm  (OR in imm<<10) */
    A64_LDP_FP_LR_POST16   = 0xA8C17BFD, /* ldp x29, x30, [sp], #16 */
};

/* ── Bit-field helpers ──────────────────────────────────────────────── */

/* Extract the low 5 bits of a register number (the hardware Rd/Rn/Rm field). */
static inline uint32_t
reg_bits(AArch64_Register r)
{
    return (uint32_t)r & 0x1F;
}

/* Extract a 12-bit immediate, asserting it fits. */
static inline uint32_t
encode_imm12(int64_t value)
{
    assert(value >= 0 && value < 4096);
    return (uint32_t)value & 0xFFF;
}

/* Extract a 16-bit immediate. */
static inline uint32_t
encode_imm16(uint64_t value)
{
    return (uint32_t)(value & 0xFFFF);
}

/* Returns true if a signed value fits in an unsigned 12-bit immediate. */
static inline int
fits_imm12(int64_t value)
{
    return value >= 0 && value < 4096;
}

/* Encode a 7-bit signed offset for STP/LDP, given a byte offset divisible by 8. */
static inline uint32_t
encode_imm7_scaled8(int64_t byte_offset)
{
    assert(byte_offset % 8 == 0);
    return (uint32_t)((int32_t)(byte_offset / 8) & 0x7F);
}

/* ── Instruction word helpers ───────────────────────────────────────── */

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
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->jump_table.pending_jumps,
                                        jump))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

/* Emit a 32-bit ARM64 instruction word (little-endian). */
static UNIT_Status
emit_inst(_UNIT_CompileContext *ctx, uint32_t word)
{
    assert(ctx != NULL);
    return _UNIT_CodeBuffer_Emit32(&ctx->buffer, word);
}

/* Serialize a 32-bit instruction into a byte buffer (little-endian). */
static void
write_inst_le(uint8_t *out, uint32_t word)
{
    assert(out != NULL);
    out[0] = (word >>  0) & 0xFF;
    out[1] = (word >>  8) & 0xFF;
    out[2] = (word >> 16) & 0xFF;
    out[3] = (word >> 24) & 0xFF;
}

/* Read a 32-bit little-endian instruction word from a byte buffer. */
static uint32_t
read_inst_le(const uint8_t *data)
{
    assert(data != NULL);
    return (uint32_t)data[0]
         | ((uint32_t)data[1] << 8)
         | ((uint32_t)data[2] << 16)
         | ((uint32_t)data[3] << 24);
}

/* Emit a movz/movk sequence to load a full 64-bit immediate into Xd. */
static UNIT_Status
emit_mov_imm64(_UNIT_CompileContext *ctx, uint32_t rd, uint64_t imm)
{
    assert(ctx != NULL);
    /* movz Xd, #(imm & 0xFFFF), LSL #0 */
    uint32_t inst = A64_MOVZ_64 | (encode_imm16(imm) << 5) | rd;
    if (UNIT_FAILED(emit_inst(ctx, inst))) {
        return _UNIT_FAIL;
    }

    /* movk for each non-zero 16-bit chunk */
    for (int shift = 1; shift <= 3; ++shift) {
        uint32_t hw = encode_imm16(imm >> (shift * 16));
        if (hw != 0) {
            inst = A64_MOVK_64 | ((uint32_t)shift << 21) | (hw << 5) | rd;
            if (UNIT_FAILED(emit_inst(ctx, inst))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

/* ── Main encoder ───────────────────────────────────────────────────── */

UNIT_Status
AArch64_encode_instruction(_UNIT_CompileContext *compile_context,
                           AArch64_Instruction *instr)
{
    assert(compile_context != NULL);
    assert(instr != NULL);

#define EMIT_INST(word)                                             \
    if (UNIT_FAILED(emit_inst(compile_context, (word)))) {          \
        goto error;                                                 \
    }

#define INDEX() _UNIT_CodeBuffer_CurrentIndex(&compile_context->buffer)

#define EMIT_JUMP(label_index)                                      \
    if (UNIT_FAILED(add_jump(compile_context, label_index))) {      \
        goto error;                                                 \
    }                                                               \
    EMIT_INST(0x00000000) /* placeholder */

    switch (instr->opcode) {

        case AArch64_MOV_REG: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rm = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_ORR_64_SHIFTED | (rm << 16) | rd);
            break;
        }

        case AArch64_MOV_IMM: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_IMMEDIATE);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t imm16 = encode_imm16((uint64_t)instr->operands[1].immediate);
            EMIT_INST(A64_MOVZ_64 | (imm16 << 5) | rd);
            break;
        }

        case AArch64_MOV_WIDE: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_IMMEDIATE);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint64_t imm = (uint64_t)instr->operands[1].immediate;
            if (UNIT_FAILED(emit_mov_imm64(compile_context, rd, imm))) {
                goto error;
            }
            break;
        }

        case AArch64_LDR: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn;
            uint64_t offset;
            if (instr->operands[1].kind == A64_OPERAND_STACK) {
                rn = reg_bits(REG_SP);
                offset = (uint64_t)instr->operands[1].immediate;
            } else {
                assert(instr->operands[1].kind == A64_OPERAND_REGISTER
                       || instr->operands[1].kind == A64_OPERAND_INDIRECT);
                rn = reg_bits(instr->operands[1].reg);
                offset = 0;
            }
            assert(offset % 8 == 0);
            uint32_t imm12 = (uint32_t)(offset / 8);
            assert(imm12 < 4096);
            EMIT_INST(A64_LDR_64_UNSIGNED | (imm12 << 10) | (rn << 5) | rt);
            break;
        }

        case AArch64_STR: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn;
            uint64_t offset;
            if (instr->operands[1].kind == A64_OPERAND_STACK) {
                rn = reg_bits(REG_SP);
                offset = (uint64_t)instr->operands[1].immediate;
            } else {
                assert(instr->operands[1].kind == A64_OPERAND_REGISTER
                       || instr->operands[1].kind == A64_OPERAND_INDIRECT);
                rn = reg_bits(instr->operands[1].reg);
                offset = 0;
            }
            assert(offset % 8 == 0);
            uint32_t imm12 = (uint32_t)(offset / 8);
            assert(imm12 < 4096);
            EMIT_INST(A64_STR_64_UNSIGNED | (imm12 << 10) | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRB: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDRB_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRH: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDRH_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRW: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDR_32_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_STRB: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_STRB_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_STRH: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_STRH_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_STRW: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_STR_32_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRSB: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDRSB_64_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRSH: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDRSH_64_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_LDRSW: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rt = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_LDRSW_UNSIGNED | (rn << 5) | rt);
            break;
        }

        case AArch64_STP_PRE: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[2].kind == A64_OPERAND_IMMEDIATE);
            uint32_t rt1 = reg_bits(instr->operands[0].reg);
            uint32_t rt2 = reg_bits(instr->operands[1].reg);
            uint32_t imm7 = encode_imm7_scaled8(instr->operands[2].immediate);
            EMIT_INST(A64_STP_PRE_64 | (imm7 << 15) | (rt2 << 10)
                      | (reg_bits(REG_SP) << 5) | rt1);
            break;
        }

        case AArch64_LDP_POST: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[2].kind == A64_OPERAND_IMMEDIATE);
            uint32_t rt1 = reg_bits(instr->operands[0].reg);
            uint32_t rt2 = reg_bits(instr->operands[1].reg);
            uint32_t imm7 = encode_imm7_scaled8(instr->operands[2].immediate);
            EMIT_INST(A64_LDP_POST_64 | (imm7 << 15) | (rt2 << 10)
                      | (reg_bits(REG_SP) << 5) | rt1);
            break;
        }

        case AArch64_BL_SYMBOL: {
            assert(instr->operand_count == 1);
            assert(instr->operands[0].kind == A64_OPERAND_IMMEDIATE);
            uint32_t symbol_index = (uint32_t)instr->operands[0].immediate;
            _UNIT_Relocation *relocation = _UNIT_Relocation_NewCall(
                compile_context->context,
                INDEX(),
                symbol_index
            );
            if (relocation == NULL) {
                goto error;
            }
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->symbol_table.relocations,
                                                relocation))) {
                goto error;
            }
            EMIT_INST(A64_BL);
            break;
        }

        case AArch64_BLR: {
            assert(instr->operand_count == 1);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            uint32_t rn = reg_bits(instr->operands[0].reg);
            EMIT_INST(A64_BLR | (rn << 5));
            break;
        }

        case AArch64_B: {
            assert(instr->operand_count == 1);
            assert(instr->operands[0].kind == A64_OPERAND_IMMEDIATE);
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            EMIT_JUMP(label_index);
            break;
        }

        case AArch64_B_COND: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_IMMEDIATE);
            assert(instr->operands[1].kind == A64_OPERAND_CONDITION);
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            AArch64_Condition cond = instr->operands[1].condition;
            _UNIT_PendingJump *jump = _UNIT_PendingJump_New(
                compile_context->context,
                INDEX(),
                label_index
            );
            if (jump == NULL) {
                goto error;
            }
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->jump_table.pending_jumps,
                                                jump))) {
                goto error;
            }
            /* Emit B.cond placeholder with condition encoded */
            EMIT_INST(A64_B_COND | (uint32_t)cond);
            break;
        }

        case AArch64_B_LABEL: {
            assert(instr->operand_count == 1);
            assert(instr->operands[0].kind == A64_OPERAND_IMMEDIATE);
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            if (UNIT_FAILED(_UNIT_SizeMap_Set(&compile_context->jump_table.label_offsets,
                                            label_index,
                                            INDEX()))) {
                goto error;
            }
            break;
        }

        /* CMP - encoder selects register or immediate form */
        case AArch64_CMP: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            uint32_t rn = reg_bits(instr->operands[0].reg);

            if (instr->operands[1].kind == A64_OPERAND_IMMEDIATE) {
                uint32_t imm12 = encode_imm12(instr->operands[1].immediate);
                EMIT_INST(A64_SUBS_IMMEDIATE | (imm12 << 10) | (rn << 5)
                          | reg_bits(REG_XZR));
            } else {
                assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
                uint32_t rm = reg_bits(instr->operands[1].reg);
                EMIT_INST(A64_SUBS_SHIFTED_REG | (rm << 16) | (rn << 5)
                          | reg_bits(REG_XZR));
            }
            break;
        }

        /* ADD - encoder selects register or immediate form */
        case AArch64_ADD: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);

            if (instr->operands[2].kind == A64_OPERAND_IMMEDIATE) {
                uint32_t imm12 = encode_imm12(instr->operands[2].immediate);
                EMIT_INST(A64_ADD_IMMEDIATE | (imm12 << 10) | (rn << 5) | rd);
            } else {
                assert(instr->operands[2].kind == A64_OPERAND_REGISTER);
                uint32_t rm = reg_bits(instr->operands[2].reg);
                EMIT_INST(A64_ADD_SHIFTED_REG | (rm << 16) | (rn << 5) | rd);
            }
            break;
        }

        /* SUB - encoder selects register or immediate form */
        case AArch64_SUB: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);

            if (instr->operands[2].kind == A64_OPERAND_IMMEDIATE) {
                uint32_t imm12 = encode_imm12(instr->operands[2].immediate);
                EMIT_INST(A64_SUB_IMMEDIATE | (imm12 << 10) | (rn << 5) | rd);
            } else {
                assert(instr->operands[2].kind == A64_OPERAND_REGISTER);
                uint32_t rm = reg_bits(instr->operands[2].reg);
                EMIT_INST(A64_SUB_SHIFTED_REG | (rm << 16) | (rn << 5) | rd);
            }
            break;
        }

        case AArch64_MUL: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[2].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            uint32_t rm = reg_bits(instr->operands[2].reg);
            EMIT_INST(A64_MADD_XZR | (rm << 16) | (rn << 5) | rd);
            break;
        }

        case AArch64_SDIV: {
            assert(instr->operand_count == 3);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[2].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            uint32_t rm = reg_bits(instr->operands[2].reg);
            EMIT_INST(A64_SDIV | (rm << 16) | (rn << 5) | rd);
            break;
        }

        case AArch64_MSUB: {
            assert(instr->operand_count == 4);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[2].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[3].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            uint32_t rm = reg_bits(instr->operands[2].reg);
            uint32_t ra = reg_bits(instr->operands[3].reg);
            EMIT_INST(A64_MSUB | (rm << 16) | (ra << 10) | (rn << 5) | rd);
            break;
        }

        case AArch64_LOAD_STRING: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_IMMEDIATE);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            UNIT_Size string_index = (UNIT_Size)instr->operands[1].immediate;
            _UNIT_SizeMap *string_offsets = &compile_context->string_data.string_offsets;
            UNIT_Size byte_offset = _UNIT_SizeMap_GET(string_offsets, string_index);

            _UNIT_Relocation *relocation = _UNIT_Relocation_NewData(
                compile_context->context, INDEX(), byte_offset);
            if (relocation == NULL) {
                goto error;
            }
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->symbol_table.relocations,
                                                relocation))) {
                goto error;
            }
            /* ADRP Xd, <page> placeholder */
            EMIT_INST(A64_ADRP | rd);
            /* ADD Xd, Xd, #0 placeholder (patched by linker) */
            EMIT_INST(A64_ADD_IMMEDIATE | (rd << 5) | rd);
            break;
        }

        case AArch64_RET: {
            assert(instr->operand_count == 0);
            EMIT_INST(A64_RET);
            break;
        }

        case AArch64_UXTB: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_UBFM_UXTB | (rn << 5) | rd);
            break;
        }

        case AArch64_UXTH: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_UBFM_UXTH | (rn << 5) | rd);
            break;
        }

        case AArch64_UXTW: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rm = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_ORR_32_SHIFTED | (rm << 16) | rd);
            break;
        }

        case AArch64_SXTB: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_SBFM_SXTB | (rn << 5) | rd);
            break;
        }

        case AArch64_SXTH: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_SBFM_SXTH | (rn << 5) | rd);
            break;
        }

        case AArch64_SXTW: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_REGISTER);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t rn = reg_bits(instr->operands[1].reg);
            EMIT_INST(A64_SBFM_SXTW | (rn << 5) | rd);
            break;
        }

        case AArch64_ADD_SP_IMM: {
            assert(instr->operand_count == 2);
            assert(instr->operands[0].kind == A64_OPERAND_REGISTER);
            assert(instr->operands[1].kind == A64_OPERAND_IMMEDIATE
                   || instr->operands[1].kind == A64_OPERAND_STACK);
            uint32_t rd = reg_bits(instr->operands[0].reg);
            uint32_t imm12 = encode_imm12(instr->operands[1].immediate);
            /* ADD Xd, SP, #imm12 */
            EMIT_INST(A64_ADD_IMMEDIATE | (imm12 << 10)
                      | (reg_bits(REG_SP) << 5) | rd);
            break;
        }

        case AArch64_SVC: {
            assert(instr->operand_count == 1);
            assert(instr->operands[0].kind == A64_OPERAND_IMMEDIATE);
            uint32_t imm16 = encode_imm16((uint64_t)instr->operands[0].immediate);
            EMIT_INST(A64_SVC | (imm16 << 5));
            break;
        }

        case AArch64_ADRP:
        case AArch64_ADD_LO12:
            /* These are handled via LOAD_STRING; shouldn't be emitted directly */
            _UNIT_Unreachable();
            break;
    }

    _UNIT_Dealloc(compile_context->context, instr);
    return _UNIT_OK;
error:
    _UNIT_Dealloc(compile_context->context, instr);
    return _UNIT_FAIL;

#undef EMIT_INST
#undef INDEX
#undef EMIT_JUMP
}

/* ── Prologue / Epilogue / Jump patching ────────────────────────────── */

void
AArch64_PatchPrologue(_UNIT_CompileContext *compile_context,
                      UNIT_Size prologue_offset,
                      UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(prologue_offset >= 0);
    assert(frame_size >= 0);

    /* Ensure frame_size is 16-byte aligned for AArch64 SP alignment */
    if (frame_size != 0 && frame_size % 16 != 0) {
        frame_size = (frame_size + 15) & ~(UNIT_Size)15;
    }

    uint32_t inst2;
    if (frame_size == 0) {
        inst2 = A64_NOP;
    } else {
        assert(frame_size < 4096);
        inst2 = A64_SUB_SP_IMM_BASE | ((uint32_t)frame_size << 10);
    }

    uint8_t prologue[16];
    uint32_t insts[4] = { A64_STP_FP_LR_PRE16, A64_MOV_FP_SP, inst2, A64_NOP };
    for (int i = 0; i < 4; i++) {
        write_inst_le(&prologue[i * 4], insts[i]);
    }

    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer, prologue_offset,
                                prologue, 16);
}

void
AArch64_PatchEpilogue(_UNIT_CompileContext *compile_context,
                      UNIT_Size epilogue_offset,
                      UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(epilogue_offset >= 0);
    assert(frame_size >= 0);

    /* Ensure frame_size is 16-byte aligned */
    if (frame_size != 0 && frame_size % 16 != 0) {
        frame_size = (frame_size + 15) & ~(UNIT_Size)15;
    }

    uint32_t inst0, inst1;
    if (frame_size == 0) {
        inst0 = A64_LDP_FP_LR_POST16;
        inst1 = A64_NOP;
    } else {
        assert(frame_size < 4096);
        inst0 = A64_ADD_SP_IMM_BASE | ((uint32_t)frame_size << 10);
        inst1 = A64_LDP_FP_LR_POST16;
    }

    uint8_t epilogue[16];
    uint32_t insts[4] = { inst0, inst1, A64_NOP, A64_NOP };
    for (int i = 0; i < 4; i++) {
        write_inst_le(&epilogue[i * 4], insts[i]);
    }

    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer, epilogue_offset,
                                epilogue, 16);
}

void
AArch64_PatchJumps(_UNIT_CompileContext *compile_context)
{
    assert(compile_context != NULL);
    UNIT_Size count = _UNIT_Vector_SIZE(&compile_context->jump_table.pending_jumps);

    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_PendingJump *jump = _UNIT_Vector_GET(&compile_context->jump_table.pending_jumps,
                                                   index);
        assert(jump != NULL);
        UNIT_Size label_offset = _UNIT_SizeMap_GET(&compile_context->jump_table.label_offsets,
                                                   jump->label_index);

        int32_t displacement = (int32_t)(label_offset - jump->patch_offset);
        assert(displacement % 4 == 0);
        int32_t imm = displacement / 4;

        uint8_t *data = compile_context->buffer.data + jump->patch_offset;
        uint32_t existing = read_inst_le(data);

        uint32_t patched;
        if ((existing & 0xFF000000) == A64_B_COND) {
            /* B.cond: preserve condition bits, fill in imm19 */
            uint32_t cond = existing & 0xF;
            uint32_t imm19 = (uint32_t)imm & 0x7FFFF;
            patched = A64_B_COND | (imm19 << 5) | cond;
        } else {
            /* B (unconditional): fill in imm26 */
            uint32_t imm26 = (uint32_t)imm & 0x03FFFFFF;
            patched = A64_B | imm26;
        }

        write_inst_le(data, patched);
    }
}
