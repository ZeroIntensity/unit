#ifndef UNIT_AARCH64_LOCAL_H
#define UNIT_AARCH64_LOCAL_H

#include <unit/base.h>

#include <unit/internal/compile_context.h>

typedef enum {
    REG_X0  = 0,
    REG_X1  = 1,
    REG_X2  = 2,
    REG_X3  = 3,
    REG_X4  = 4,
    REG_X5  = 5,
    REG_X6  = 6,
    REG_X7  = 7,
    REG_X8  = 8,
    REG_X9  = 9,
    REG_X10 = 10,
    REG_X11 = 11,
    REG_X12 = 12,
    REG_X13 = 13,
    REG_X14 = 14,
    REG_X15 = 15,
    REG_X16 = 16, /* IP0 - scratch / intra-procedure-call */
    REG_X17 = 17, /* IP1 - scratch / intra-procedure-call */
    REG_X18 = 18, /* Platform register (reserved on Apple) */
    REG_X19 = 19,
    REG_X20 = 20,
    REG_X21 = 21,
    REG_X22 = 22,
    REG_X23 = 23,
    REG_X24 = 24,
    REG_X25 = 25,
    REG_X26 = 26,
    REG_X27 = 27,
    REG_X28 = 28,
    REG_X29 = 29, /* FP - frame pointer */
    REG_X30 = 30, /* LR - link register */
    REG_XZR = 31, /* Zero register / SP depending on context */
    REG_SP  = 31, /* Stack pointer (same encoding as XZR) */
} AArch64_Register;

typedef enum {
    /* Moves */
    AArch64_MOV_REG,       /* mov Xd, Xn */
    AArch64_MOV_IMM,       /* movz/movk Xd, #imm */
    AArch64_MOV_WIDE,      /* full 64-bit immediate using movz+movk sequence */

    /* Loads/Stores */
    AArch64_LDR,           /* ldr Xt, [Xn, #offset] */
    AArch64_STR,           /* str Xt, [Xn, #offset] */
    AArch64_LDRB,          /* ldrb Wt, [Xn] */
    AArch64_LDRH,          /* ldrh Wt, [Xn] */
    AArch64_LDRW,          /* ldr Wt, [Xn] */
    AArch64_STRB,          /* strb Wt, [Xn] */
    AArch64_STRH,          /* strh Wt, [Xn] */
    AArch64_STRW,          /* str Wt, [Xn] */
    AArch64_LDRSB,         /* ldrsb Xt, [Xn] */
    AArch64_LDRSH,         /* ldrsh Xt, [Xn] */
    AArch64_LDRSW,         /* ldrsw Xt, [Xn] */

    /* Stack: STP/LDP for prologue/epilogue */
    AArch64_STP_PRE,       /* stp Xt1, Xt2, [sp, #-imm]! */
    AArch64_LDP_POST,      /* ldp Xt1, Xt2, [sp], #imm */

    /* Calls */
    AArch64_BL_SYMBOL,     /* bl <symbol> (relocation) */
    AArch64_BLR,           /* blr Xn */

    /* Branches */
    AArch64_B,             /* b <label> */
    AArch64_B_COND,        /* b.<cond> <label> */
    AArch64_B_LABEL,       /* label marker (no code emitted) */

    /* Comparisons - encoder selects reg/imm form based on operands */
    AArch64_CMP,           /* cmp Xn, Xm  or  cmp Xn, #imm12 */

    /* Arithmetic - encoder selects reg/imm form based on operands */
    AArch64_ADD,           /* add Xd, Xn, Xm  or  add Xd, Xn, #imm12 */
    AArch64_SUB,           /* sub Xd, Xn, Xm  or  sub Xd, Xn, #imm12 */
    AArch64_MUL,           /* mul Xd, Xn, Xm */
    AArch64_SDIV,          /* sdiv Xd, Xn, Xm */
    AArch64_MSUB,          /* msub Xd, Xn, Xm, Xa (Xa - Xn*Xm) */

    /* Misc */
    AArch64_ADRP,          /* adrp Xd, <page> (relocation placeholder) */
    AArch64_ADD_LO12,      /* add Xd, Xd, #:lo12:<sym> (relocation placeholder) */
    AArch64_LOAD_STRING,   /* pseudo: adrp+add for string data (relocation) */
    AArch64_RET,           /* ret (return via X30) */

    /* Extension */
    AArch64_UXTB,         /* ubfm Wd, Wn, #0, #7 */
    AArch64_UXTH,         /* ubfm Wd, Wn, #0, #15 */
    AArch64_UXTW,         /* mov Wd, Wn (zero-extend 32->64) */
    AArch64_SXTB,         /* sbfm Xd, Xn, #0, #7 */
    AArch64_SXTH,         /* sbfm Xd, Xn, #0, #15 */
    AArch64_SXTW,         /* sbfm Xd, Xn, #0, #31 */

    /* Address */
    AArch64_ADD_SP_IMM,    /* add Xd, sp, #imm */

    /* SVC (for exit syscall on Linux) */
    AArch64_SVC,           /* svc #imm16 */
} AArch64_Opcode;

typedef enum {
    AArch64_COND_EQ = 0x0,
    AArch64_COND_NE = 0x1,
    AArch64_COND_GE = 0xA,
    AArch64_COND_LT = 0xB,
    AArch64_COND_GT = 0xC,
    AArch64_COND_LE = 0xD,
} AArch64_Condition;

typedef enum {
    A64_OPERAND_NONE,
    A64_OPERAND_REGISTER,
    A64_OPERAND_IMMEDIATE,
    A64_OPERAND_STACK,      /* [sp + offset] */
    A64_OPERAND_INDIRECT,   /* [Xn] */
    A64_OPERAND_CONDITION,
} AArch64_OperandKind;

typedef struct {
    AArch64_OperandKind kind;
    union {
        AArch64_Register reg;
        int64_t immediate;
        AArch64_Condition condition;
    };
} AArch64_Operand;

typedef struct {
    AArch64_Opcode opcode;
    AArch64_Operand operands[4];
    UNIT_Size operand_count;
} AArch64_Instruction;

UNIT_Status
AArch64_encode_instruction(_UNIT_CompileContext *context,
                           AArch64_Instruction *instr);

void
AArch64_PatchPrologue(_UNIT_CompileContext *context,
                      UNIT_Size prologue_offset,
                      UNIT_Size frame_size);

void
AArch64_PatchEpilogue(_UNIT_CompileContext *compile_context,
                      UNIT_Size epilogue_offset,
                      UNIT_Size frame_size);

void
AArch64_PatchJumps(_UNIT_CompileContext *context);

#endif
