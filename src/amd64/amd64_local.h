#ifndef UNIT_AMD64_LOCAL_H
#define UNIT_AMD64_LOCAL_H

#include <unit/base.h>

#include <unit/internal/compile_context.h>

typedef enum {
    REG_RAX = 0,
    REG_RCX = 1,
    REG_RDX = 2,
    REG_RBX = 3,
    REG_RSP = 4,
    REG_RBP = 5,
    REG_RSI = 6,
    REG_RDI = 7,
    REG_R8  = 8,
    REG_R9  = 9,
    REG_R10 = 10,
    REG_R11 = 11,
    REG_R12 = 12,
    REG_R13 = 13,
    REG_R14 = 14,
    REG_R15 = 15,
} AMD64_Register;

typedef enum {
    // Moves
    AMD64_MOV,
    AMD64_MOVZX,
    AMD64_MOVZX8,
    AMD64_MOVSX8,
    AMD64_MOVZX16,
    AMD64_MOVSX16,
    AMD64_MOVSXD,
    AMD64_MOV32,
    AMD64_MOV_SIZED,

    // Calls
    AMD64_SYSCALL,
    AMD64_CALL_INDIRECT,
    AMD64_CALL_SYMBOL,

    // Jumps
    AMD64_JUMP,
    AMD64_JUMP_LABEL,
    AMD64_JUMP_IF_EQUAL,
    AMD64_JUMP_IF_NOT_EQUAL,
    AMD64_JUMP_IF_LESS,
    AMD64_JUMP_IF_GREATER,
    AMD64_JUMP_IF_LESS_EQUAL,
    AMD64_JUMP_IF_GREATER_EQUAL,

    // Comparisons
    AMD64_COMPARE,

    // Arithmetic
    AMD64_ADD,
    AMD64_SUB,
    AMD64_MUL,
    AMD64_DIV,

    // Misc
    AMD64_LOAD_STRING,
    AMD64_LOAD_ADDRESS,
    AMD64_CQO,
    AMD64_RET,
} AMD64_Opcode;

typedef enum {
    OPERAND_REGISTER,
    OPERAND_IMMEDIATE,
    OPERAND_STACK,
    OPERAND_INDIRECT // Fancy word for pointer
} AMD64_OperandKind;

typedef struct {
    AMD64_OperandKind kind;

    union {
        AMD64_Register reg;
        uint64_t immediate;
        uint64_t stack_offset;
    };
} AMD64_Operand;

typedef struct {
    AMD64_Opcode opcode;
    AMD64_Operand operands[3];
    UNIT_Size operand_count;
} AMD64_Instruction;

UNIT_Status
AMD64_encode_instruction(_UNIT_CompileContext *context,
                         AMD64_Instruction *instr);

void
AMD64_PatchPrologue(_UNIT_CompileContext *context,
                    UNIT_Size prologue_offset,
                    UNIT_Size frame_size);

void
AMD64_PatchEpilogue(_UNIT_CompileContext *compile_context,
                    UNIT_Size epilogue_offset,
                    UNIT_Size frame_size);

void
AMD64_PatchJumps(_UNIT_CompileContext *context);

#endif
