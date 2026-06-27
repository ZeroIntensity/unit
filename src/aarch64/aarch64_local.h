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
} AARCH64_Register;

typedef enum {
    /* Moves */
    AARCH64_MOV_REG,       /* mov Xd, Xn */
    AARCH64_MOV_IMM,       /* movz/movk Xd, #imm */
    AARCH64_MOV_WIDE,      /* full 64-bit immediate using movz+movk sequence */

    /* Loads/Stores */
    AARCH64_LDR,           /* ldr Xt, [Xn, #offset] */
    AARCH64_STR,           /* str Xt, [Xn, #offset] */
    AARCH64_LDRB,          /* ldrb Wt, [Xn] */
    AARCH64_LDRH,          /* ldrh Wt, [Xn] */
    AARCH64_LDRW,          /* ldr Wt, [Xn] */
    AARCH64_STRB,          /* strb Wt, [Xn] */
    AARCH64_STRH,          /* strh Wt, [Xn] */
    AARCH64_STRW,          /* str Wt, [Xn] */
    AARCH64_LDRSB,         /* ldrsb Xt, [Xn] */
    AARCH64_LDRSH,         /* ldrsh Xt, [Xn] */
    AARCH64_LDRSW,         /* ldrsw Xt, [Xn] */

    /* Stack: STP/LDP for prologue/epilogue */
    AARCH64_STP_PRE,       /* stp Xt1, Xt2, [sp, #-imm]! */
    AARCH64_LDP_POST,      /* ldp Xt1, Xt2, [sp], #imm */

    /* Calls */
    AARCH64_BL_SYMBOL,     /* bl <symbol> (relocation) */
    AARCH64_BLR,           /* blr Xn */

    /* Branches */
    AARCH64_B,             /* b <label> */
    AARCH64_B_COND,        /* b.<cond> <label> */
    AARCH64_B_LABEL,       /* label marker (no code emitted) */

    /* Comparisons */
    AARCH64_CMP_REG,       /* cmp Xn, Xm */
    AARCH64_CMP_IMM,       /* cmp Xn, #imm */

    /* Arithmetic */
    AARCH64_ADD_REG,       /* add Xd, Xn, Xm */
    AARCH64_ADD_IMM,       /* add Xd, Xn, #imm */
    AARCH64_SUB_REG,       /* sub Xd, Xn, Xm */
    AARCH64_SUB_IMM,       /* sub Xd, Xn, #imm */
    AARCH64_MUL,           /* mul Xd, Xn, Xm */
    AARCH64_SDIV,          /* sdiv Xd, Xn, Xm */
    AARCH64_MSUB,          /* msub Xd, Xn, Xm, Xa (Xa - Xn*Xm) */

    /* Misc */
    AARCH64_ADRP,          /* adrp Xd, <page> (relocation placeholder) */
    AARCH64_ADD_LO12,      /* add Xd, Xd, #:lo12:<sym> (relocation placeholder) */
    AARCH64_LOAD_STRING,   /* pseudo: adrp+add for string data (relocation) */
    AARCH64_RET,           /* ret (return via X30) */

    /* Extension */
    AARCH64_UXTB,         /* and Xd, Xn, #0xFF */
    AARCH64_UXTH,         /* and Xd, Xn, #0xFFFF */
    AARCH64_UXTW,         /* mov Wd, Wn (zero-extend 32->64) */
    AARCH64_SXTB,         /* sxtb Xd, Wn */
    AARCH64_SXTH,         /* sxth Xd, Wn */
    AARCH64_SXTW,         /* sxtw Xd, Wn */

    /* Address */
    AARCH64_ADD_SP_IMM,    /* add Xd, sp, #imm */

    /* SVC (for exit syscall on Linux) */
    AARCH64_SVC,           /* svc #0 */
} AARCH64_Opcode;

typedef enum {
    AARCH64_COND_EQ = 0x0,
    AARCH64_COND_NE = 0x1,
    AARCH64_COND_GE = 0xA,
    AARCH64_COND_LT = 0xB,
    AARCH64_COND_GT = 0xC,
    AARCH64_COND_LE = 0xD,
} AARCH64_Condition;

typedef enum {
    A64_OPERAND_NONE,
    A64_OPERAND_REGISTER,
    A64_OPERAND_IMMEDIATE,
    A64_OPERAND_STACK,      /* [sp + offset] */
    A64_OPERAND_INDIRECT,   /* [Xn] */
    A64_OPERAND_CONDITION,
} AARCH64_OperandKind;

typedef struct {
    AARCH64_OperandKind kind;
    union {
        AARCH64_Register reg;
        int64_t immediate;
        AARCH64_Condition condition;
    };
} AARCH64_Operand;

typedef struct {
    AARCH64_Opcode opcode;
    AARCH64_Operand operands[4];
    UNIT_Size operand_count;
} AARCH64_Instruction;

UNIT_Status
AARCH64_encode_instruction(_UNIT_CompileContext *context,
                           AARCH64_Instruction *instr);

void
AARCH64_PatchPrologue(_UNIT_CompileContext *context,
                      UNIT_Size prologue_offset,
                      UNIT_Size frame_size);

void
AARCH64_PatchEpilogue(_UNIT_CompileContext *compile_context,
                      UNIT_Size epilogue_offset,
                      UNIT_Size frame_size);

void
AARCH64_PatchJumps(_UNIT_CompileContext *context);

#endif
