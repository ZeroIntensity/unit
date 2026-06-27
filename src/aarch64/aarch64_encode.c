#include <unit/base.h>

#include <unit/internal/architectures.h>
#include <unit/internal/code_buffer.h>
#include <unit/internal/compile_context.h>

#include "aarch64_local.h"

/*
 * ARM64 instructions are always 32 bits, little-endian.
 * This file encodes AARCH64_Instructions into 32-bit machine words.
 */

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

/* Emit a 32-bit ARM64 instruction word (little-endian) */
static UNIT_Status
emit_inst(_UNIT_CompileContext *ctx, uint32_t word)
{
    return _UNIT_CodeBuffer_Emit32(&ctx->buffer, word);
}

/* Emit a movz/movk sequence to load a full 64-bit immediate into Xd */
static UNIT_Status
emit_mov_imm64(_UNIT_CompileContext *ctx, uint32_t rd, uint64_t imm)
{
    /* movz Xd, #(imm & 0xFFFF), LSL #0 */
    uint32_t hw0 = (uint32_t)(imm & 0xFFFF);
    uint32_t inst = 0xD2800000 | ((uint32_t)hw0 << 5) | rd;
    if (UNIT_FAILED(emit_inst(ctx, inst))) return _UNIT_FAIL;

    /* movk for each non-zero 16-bit chunk */
    for (int shift = 1; shift <= 3; ++shift) {
        uint32_t hw = (uint32_t)((imm >> (shift * 16)) & 0xFFFF);
        if (hw != 0) {
            /* movk Xd, #hw, LSL #(shift*16) */
            inst = 0xF2800000 | ((uint32_t)shift << 21) | ((uint32_t)hw << 5) | rd;
            if (UNIT_FAILED(emit_inst(ctx, inst))) return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

UNIT_Status
AARCH64_encode_instruction(_UNIT_CompileContext *compile_context,
                           AARCH64_Instruction *instr)
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

        case AARCH64_MOV_REG: {
            /* mov Xd, Xn  is ORR Xd, XZR, Xn */
            uint32_t rd = instr->operands[0].reg;
            uint32_t rn = instr->operands[1].reg;
            /* ORR (shifted register): 1|01|01010|00|0|Rm|000000|Rn|Rd */
            /* sf=1, opc=01, shift=00, imm6=000000, Rn=11111(XZR) */
            EMIT_INST(0xAA0003E0 | ((rn & 0x1F) << 16) | (rd & 0x1F));
            break;
        }

        case AARCH64_MOV_IMM: {
            /* movz Xd, #imm16 */
            uint32_t rd = instr->operands[0].reg;
            uint64_t imm = (uint64_t)instr->operands[1].immediate;
            EMIT_INST(0xD2800000 | ((uint32_t)(imm & 0xFFFF) << 5) | (rd & 0x1F));
            break;
        }

        case AARCH64_MOV_WIDE: {
            /* Full 64-bit immediate load via movz+movk sequence */
            uint32_t rd = instr->operands[0].reg;
            uint64_t imm = (uint64_t)instr->operands[1].immediate;
            if (UNIT_FAILED(emit_mov_imm64(compile_context, rd & 0x1F, imm))) {
                goto error;
            }
            break;
        }

        /* Loads from [sp + offset] using unsigned offset form */
        case AARCH64_LDR: {
            /* ldr Xt, [Xn, #imm] - unsigned offset, 64-bit */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn;
            uint64_t offset;
            if (instr->operands[1].kind == A64_OPERAND_STACK) {
                rn = 31; /* SP */
                offset = (uint64_t)instr->operands[1].immediate;
            } else {
                rn = instr->operands[1].reg & 0x1F;
                offset = 0;
            }
            assert(offset % 8 == 0);
            uint32_t imm12 = (uint32_t)(offset / 8);
            assert(imm12 < 4096);
            /* LDR (imm, unsigned): 11|111|00101|imm12|Rn|Rt */
            EMIT_INST(0xF9400000 | (imm12 << 10) | (rn << 5) | rt);
            break;
        }

        case AARCH64_STR: {
            /* str Xt, [Xn, #imm] - unsigned offset, 64-bit */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn;
            uint64_t offset;
            if (instr->operands[1].kind == A64_OPERAND_STACK) {
                rn = 31; /* SP */
                offset = (uint64_t)instr->operands[1].immediate;
            } else {
                rn = instr->operands[1].reg & 0x1F;
                offset = 0;
            }
            assert(offset % 8 == 0);
            uint32_t imm12 = (uint32_t)(offset / 8);
            assert(imm12 < 4096);
            /* STR (imm, unsigned): 11|111|00100|imm12|Rn|Rt */
            EMIT_INST(0xF9000000 | (imm12 << 10) | (rn << 5) | rt);
            break;
        }

        /* Byte load/store for indirect access */
        case AARCH64_LDRB: {
            /* ldrb Wt, [Xn] */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDRB unsigned offset 0: 00|111|00101|000000000000|Rn|Rt */
            EMIT_INST(0x39400000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_LDRH: {
            /* ldrh Wt, [Xn] */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDRH unsigned offset 0: 01|111|00101|000000000000|Rn|Rt */
            EMIT_INST(0x79400000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_LDRW: {
            /* ldr Wt, [Xn] (32-bit) */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDR (32-bit, unsigned offset 0): 10|111|00101|000000000000|Rn|Rt */
            EMIT_INST(0xB9400000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_STRB: {
            /* strb Wt, [Xn] */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* STRB unsigned offset 0: 00|111|00100|000000000000|Rn|Rt */
            EMIT_INST(0x39000000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_STRH: {
            /* strh Wt, [Xn] */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* STRH unsigned offset 0: 01|111|00100|000000000000|Rn|Rt */
            EMIT_INST(0x79000000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_STRW: {
            /* str Wt, [Xn] (32-bit) */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* STR (32-bit, unsigned offset 0): 10|111|00100|000000000000|Rn|Rt */
            EMIT_INST(0xB9000000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_LDRSB: {
            /* ldrsb Xt, [Xn] (sign-extend byte to 64-bit) */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDRSB (64-bit, unsigned offset 0): 00|111|00110|000000000000|Rn|Rt */
            EMIT_INST(0x39800000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_LDRSH: {
            /* ldrsh Xt, [Xn] (sign-extend halfword to 64-bit) */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDRSH (64-bit, unsigned offset 0): 01|111|00110|000000000000|Rn|Rt */
            EMIT_INST(0x79800000 | (rn << 5) | rt);
            break;
        }

        case AARCH64_LDRSW: {
            /* ldrsw Xt, [Xn] (sign-extend word to 64-bit) */
            uint32_t rt = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* LDRSW (64-bit, unsigned offset 0): 10|111|00110|000000000000|Rn|Rt */
            EMIT_INST(0xB9800000 | (rn << 5) | rt);
            break;
        }

        /* STP pre-index: stp Xt1, Xt2, [sp, #-imm]! */
        case AARCH64_STP_PRE: {
            uint32_t rt1 = instr->operands[0].reg & 0x1F;
            uint32_t rt2 = instr->operands[1].reg & 0x1F;
            int64_t offset = instr->operands[2].immediate;
            assert(offset % 8 == 0);
            int32_t imm7 = (int32_t)(offset / 8) & 0x7F;
            /* STP pre-index 64-bit: 10|101|0011|0|imm7|Rt2|Rn(=SP)|Rt1 */
            EMIT_INST(0xA9800000 | ((uint32_t)imm7 << 15) | (rt2 << 10)
                      | (31 << 5) | rt1);
            break;
        }

        /* LDP post-index: ldp Xt1, Xt2, [sp], #imm */
        case AARCH64_LDP_POST: {
            uint32_t rt1 = instr->operands[0].reg & 0x1F;
            uint32_t rt2 = instr->operands[1].reg & 0x1F;
            int64_t offset = instr->operands[2].immediate;
            assert(offset % 8 == 0);
            int32_t imm7 = (int32_t)(offset / 8) & 0x7F;
            /* LDP post-index 64-bit: 10|101|0001|1|imm7|Rt2|Rn(=SP)|Rt1 */
            EMIT_INST(0xA8C00000 | ((uint32_t)imm7 << 15) | (rt2 << 10)
                      | (31 << 5) | rt1);
            break;
        }

        /* BL <symbol> - call with relocation */
        case AARCH64_BL_SYMBOL: {
            /* BL: 1|00101|imm26 - we emit placeholder, relocation patches it */
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
            /* BL with imm26=0 placeholder */
            EMIT_INST(0x94000000);
            break;
        }

        /* BLR Xn - indirect call */
        case AARCH64_BLR: {
            uint32_t rn = instr->operands[0].reg & 0x1F;
            /* BLR: 1101011|0001|11111|000000|Rn|00000 */
            EMIT_INST(0xD63F0000 | (rn << 5));
            break;
        }

        /* B <label> - unconditional branch to label */
        case AARCH64_B: {
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            EMIT_JUMP(label_index);
            break;
        }

        /* B.<cond> <label> - conditional branch */
        case AARCH64_B_COND: {
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            AARCH64_Condition cond = instr->operands[1].condition;
            /* We store the condition in the placeholder so PatchJumps can use it.
             * B.cond: 0101010|0|imm19|0|cond
             * We'll emit a placeholder with just the condition bits set. */
            _UNIT_PendingJump *jump = _UNIT_PendingJump_New(
                compile_context->context,
                INDEX(),
                label_index
            );
            if (jump == NULL) goto error;
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->jump_table.pending_jumps,
                                                jump))) {
                goto error;
            }
            /* Emit B.cond placeholder with condition encoded */
            EMIT_INST(0x54000000 | (uint32_t)cond);
            break;
        }

        /* Label marker - just records the offset */
        case AARCH64_B_LABEL: {
            UNIT_Size label_index = (UNIT_Size)instr->operands[0].immediate;
            if (UNIT_FAILED(_UNIT_SizeMap_Set(&compile_context->jump_table.label_offsets,
                                            label_index,
                                            INDEX()))) {
                goto error;
            }
            break;
        }

        /* CMP Xn, Xm */
        case AARCH64_CMP_REG: {
            uint32_t rn = instr->operands[0].reg & 0x1F;
            uint32_t rm = instr->operands[1].reg & 0x1F;
            /* SUBS XZR, Xn, Xm: 11101011|00|0|Rm|000000|Rn|11111 */
            EMIT_INST(0xEB000000 | (rm << 16) | (rn << 5) | 0x1F);
            break;
        }

        /* CMP Xn, #imm12 */
        case AARCH64_CMP_IMM: {
            uint32_t rn = instr->operands[0].reg & 0x1F;
            uint32_t imm = (uint32_t)instr->operands[1].immediate & 0xFFF;
            /* SUBS XZR, Xn, #imm12: 11|1100010|0|imm12|Rn|11111 */
            EMIT_INST(0xF1000000 | (imm << 10) | (rn << 5) | 0x1F);
            break;
        }

        /* ADD Xd, Xn, Xm */
        case AARCH64_ADD_REG: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t rm = instr->operands[2].reg & 0x1F;
            /* ADD (shifted reg): 1|00|01011|00|0|Rm|000000|Rn|Rd */
            EMIT_INST(0x8B000000 | (rm << 16) | (rn << 5) | rd);
            break;
        }

        /* ADD Xd, Xn, #imm12 */
        case AARCH64_ADD_IMM: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t imm = (uint32_t)instr->operands[2].immediate & 0xFFF;
            /* ADD (immediate): 1|00|100010|0|imm12|Rn|Rd */
            EMIT_INST(0x91000000 | (imm << 10) | (rn << 5) | rd);
            break;
        }

        /* SUB Xd, Xn, Xm */
        case AARCH64_SUB_REG: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t rm = instr->operands[2].reg & 0x1F;
            /* SUB (shifted reg): 1|10|01011|00|0|Rm|000000|Rn|Rd */
            EMIT_INST(0xCB000000 | (rm << 16) | (rn << 5) | rd);
            break;
        }

        /* SUB Xd, Xn, #imm12 */
        case AARCH64_SUB_IMM: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t imm = (uint32_t)instr->operands[2].immediate & 0xFFF;
            /* SUB (immediate): 1|10|100010|0|imm12|Rn|Rd */
            EMIT_INST(0xD1000000 | (imm << 10) | (rn << 5) | rd);
            break;
        }

        /* MUL Xd, Xn, Xm */
        case AARCH64_MUL: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t rm = instr->operands[2].reg & 0x1F;
            /* MADD Xd, Xn, Xm, XZR: 1|00|11011|000|Rm|0|11111|Rn|Rd */
            EMIT_INST(0x9B007C00 | (rm << 16) | (rn << 5) | rd);
            break;
        }

        /* SDIV Xd, Xn, Xm */
        case AARCH64_SDIV: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t rm = instr->operands[2].reg & 0x1F;
            /* SDIV: 1|00|11010110|Rm|00001|1|Rn|Rd */
            EMIT_INST(0x9AC00C00 | (rm << 16) | (rn << 5) | rd);
            break;
        }

        /* MSUB Xd, Xn, Xm, Xa: Xa - Xn*Xm */
        case AARCH64_MSUB: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            uint32_t rm = instr->operands[2].reg & 0x1F;
            uint32_t ra = instr->operands[3].reg & 0x1F;
            /* MSUB: 1|00|11011|000|Rm|1|Ra|Rn|Rd */
            EMIT_INST(0x9B008000 | (rm << 16) | (ra << 10) | (rn << 5) | rd);
            break;
        }

        /* LOAD_STRING pseudo: emits a relocation for string data */
        case AARCH64_LOAD_STRING: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            UNIT_Size string_index = (UNIT_Size)instr->operands[1].immediate;
            _UNIT_SizeMap *string_offsets = &compile_context->string_data.string_offsets;
            UNIT_Size byte_offset = _UNIT_SizeMap_GET(string_offsets, string_index);

            /* We emit two instructions:
             * ADRP Xd, <page>  - placeholder, patched by relocation
             * ADD  Xd, Xd, #lo12  - placeholder, patched by relocation */

            /* ADRP placeholder - relocation type RELOCATION_DATA */
            _UNIT_Relocation *relocation = _UNIT_Relocation_NewData(
                compile_context->context, INDEX(), byte_offset);
            if (relocation == NULL) goto error;
            if (UNIT_FAILED(_UNIT_Vector_Append(&compile_context->symbol_table.relocations,
                                                relocation))) {
                goto error;
            }
            /* ADRP Xd: 1|immlo(2)|10000|immhi(19)|Rd */
            EMIT_INST(0x90000000 | rd);

            /* ADD Xd, Xd, #0 placeholder - will be patched by linker/JIT */
            /* ADD immediate: 1|00|100010|0|imm12|Rn|Rd */
            EMIT_INST(0x91000000 | (rd << 5) | rd);
            break;
        }

        /* RET */
        case AARCH64_RET: {
            /* RET X30: 1101011|0010|11111|0000|0|0|11110|00000 */
            EMIT_INST(0xD65F03C0);
            break;
        }

        /* Extension operations */
        case AARCH64_UXTB: {
            /* AND Xd, Xn, #0xFF  - using UBFM encoding: UBFM Wd, Wn, #0, #7 */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* UBFM Wd, Wn, #0, #7: 0|10|100110|0|000000|000111|Rn|Rd */
            EMIT_INST(0x53001C00 | (rn << 5) | rd);
            break;
        }

        case AARCH64_UXTH: {
            /* UBFM Wd, Wn, #0, #15 */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            EMIT_INST(0x53003C00 | (rn << 5) | rd);
            break;
        }

        case AARCH64_UXTW: {
            /* MOV Wd, Wn => ORR Wd, WZR, Wn (32-bit, zero-extends to 64) */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* ORR (32-bit shifted register): 0|01|01010|00|0|Rm|000000|Rn(=WZR)|Rd */
            EMIT_INST(0x2A0003E0 | (rn << 16) | rd);
            break;
        }

        case AARCH64_SXTB: {
            /* SBFM Xd, Xn, #0, #7 */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            /* SBFM Xd, Xn, #0, #7: 1|00|100110|1|000000|000111|Rn|Rd */
            EMIT_INST(0x93401C00 | (rn << 5) | rd);
            break;
        }

        case AARCH64_SXTH: {
            /* SBFM Xd, Xn, #0, #15 */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            EMIT_INST(0x93403C00 | (rn << 5) | rd);
            break;
        }

        case AARCH64_SXTW: {
            /* SBFM Xd, Xn, #0, #31 */
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t rn = instr->operands[1].reg & 0x1F;
            EMIT_INST(0x93407C00 | (rn << 5) | rd);
            break;
        }

        /* ADD Xd, SP, #imm (for address-of stack slot) */
        case AARCH64_ADD_SP_IMM: {
            uint32_t rd = instr->operands[0].reg & 0x1F;
            uint32_t imm = (uint32_t)instr->operands[1].immediate & 0xFFF;
            /* ADD Xd, SP, #imm: 1|00|100010|0|imm12|11111|Rd */
            EMIT_INST(0x910003E0 | (imm << 10) | rd);
            break;
        }

        /* SVC #imm16 (syscall) */
        case AARCH64_SVC: {
            uint32_t imm16 = (uint32_t)instr->operands[0].immediate & 0xFFFF;
            /* SVC #imm16: 11010100|000|imm16|00001 */
            EMIT_INST(0xD4000001 | (imm16 << 5));
            break;
        }

        case AARCH64_ADRP:
        case AARCH64_ADD_LO12:
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

void
AARCH64_PatchPrologue(_UNIT_CompileContext *compile_context,
                      UNIT_Size prologue_offset,
                      UNIT_Size frame_size)
{
    assert(compile_context != NULL);
    assert(prologue_offset >= 0);
    assert(frame_size >= 0);

    /* The prologue reservation is 16 bytes (4 instructions):
     *   stp x29, x30, [sp, #-frame_size]!
     *   mov x29, sp
     *   (2 NOPs if frame_size == 0 and we skip stp)
     *
     * We always reserve 16 bytes. If frame_size == 0, we still save/restore
     * x29/x30 with a minimum 16-byte frame for the link register. */

    /* Ensure frame_size is 16-byte aligned for AArch64 SP alignment */
    if (frame_size != 0 && frame_size % 16 != 0) {
        frame_size = (frame_size + 15) & ~(UNIT_Size)15;
    }

    /* stp x29, x30, [sp, #-16]!  -- save FP/LR, pre-decrement SP by 16 */
    uint32_t stp = 0xA9BF7BFD; /* stp x29, x30, [sp, #-16]! */

    /* mov x29, sp: ADD X29, SP, #0 */
    uint32_t mov_fp = 0x910003FD; /* mov x29, sp */

    uint32_t nop = 0xD503201F;
    uint32_t inst2, inst3;

    if (frame_size == 0) {
        inst2 = nop;
        inst3 = nop;
    } else {
        assert(frame_size < 4096);
        /* sub sp, sp, #frame_size */
        inst2 = 0xD10003FF | ((uint32_t)frame_size << 10);
        inst3 = nop;
    }

    uint8_t prologue[16];
    uint32_t insts[4] = { stp, mov_fp, inst2, inst3 };
    for (int i = 0; i < 4; i++) {
        prologue[i*4 + 0] = (insts[i] >>  0) & 0xFF;
        prologue[i*4 + 1] = (insts[i] >>  8) & 0xFF;
        prologue[i*4 + 2] = (insts[i] >> 16) & 0xFF;
        prologue[i*4 + 3] = (insts[i] >> 24) & 0xFF;
    }

    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer, prologue_offset,
                                prologue, 16);
}

void
AARCH64_PatchEpilogue(_UNIT_CompileContext *compile_context,
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

    uint32_t nop = 0xD503201F;
    uint32_t inst0, inst1, inst2, inst3;

    /* ldp x29, x30, [sp], #16  -- restore FP/LR and pop 16 bytes */
    uint32_t ldp = 0xA8C17BFD; /* ldp x29, x30, [sp], #16 */

    if (frame_size == 0) {
        inst0 = ldp;
        inst1 = nop;
        inst2 = nop;
        inst3 = nop;
    } else {
        assert(frame_size < 4096);
        /* add sp, sp, #frame_size */
        inst0 = 0x910003FF | ((uint32_t)frame_size << 10);
        inst1 = ldp;
        inst2 = nop;
        inst3 = nop;
    }

    uint8_t epilogue[16];
    uint32_t insts[4] = { inst0, inst1, inst2, inst3 };
    for (int i = 0; i < 4; i++) {
        epilogue[i*4 + 0] = (insts[i] >>  0) & 0xFF;
        epilogue[i*4 + 1] = (insts[i] >>  8) & 0xFF;
        epilogue[i*4 + 2] = (insts[i] >> 16) & 0xFF;
        epilogue[i*4 + 3] = (insts[i] >> 24) & 0xFF;
    }

    _UNIT_CodeBuffer_PatchBytes(&compile_context->buffer, epilogue_offset,
                                epilogue, 16);
}

void
AARCH64_PatchJumps(_UNIT_CompileContext *compile_context)
{
    assert(compile_context != NULL);
    UNIT_Size count = _UNIT_Vector_SIZE(&compile_context->jump_table.pending_jumps);

    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_PendingJump *jump = _UNIT_Vector_GET(&compile_context->jump_table.pending_jumps,
                                                   index);
        UNIT_Size label_offset = _UNIT_SizeMap_GET(&compile_context->jump_table.label_offsets,
                                                   jump->label_index);

        int32_t displacement = (int32_t)(label_offset - jump->patch_offset);
        assert(displacement % 4 == 0);
        int32_t imm = displacement / 4;

        /* Read the existing instruction to determine if it's B or B.cond */
        uint8_t *data = compile_context->buffer.data + jump->patch_offset;
        uint32_t existing = (uint32_t)data[0]
                          | ((uint32_t)data[1] << 8)
                          | ((uint32_t)data[2] << 16)
                          | ((uint32_t)data[3] << 24);

        uint32_t patched;
        if ((existing & 0xFF000000) == 0x54000000) {
            /* B.cond: 0101010|0|imm19|0|cond */
            uint32_t cond = existing & 0xF;
            uint32_t imm19 = (uint32_t)imm & 0x7FFFF;
            patched = 0x54000000 | (imm19 << 5) | cond;
        } else {
            /* B (unconditional): 0|00101|imm26 */
            uint32_t imm26 = (uint32_t)imm & 0x03FFFFFF;
            patched = 0x14000000 | imm26;
        }

        data[0] = (patched >>  0) & 0xFF;
        data[1] = (patched >>  8) & 0xFF;
        data[2] = (patched >> 16) & 0xFF;
        data[3] = (patched >> 24) & 0xFF;
    }
}
