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
    REG_R8 = 8,
    REG_R9 = 9,
    REG_R10 = 10,
    REG_R11 = 11,
    REG_R12 = 12,
    REG_R13 = 13,
    REG_R14 = 14,
    REG_R15 = 15,
} AMD64_Register;

// Indirect means "there's a pointer in this register"
typedef struct {
    AMD64_Register reg; // The register containing the pointer
} AMD64_Indirect;

typedef struct {
    uint64_t immediate;
} AMD64_Immediate;

typedef struct {
    uint64_t offset;
} AMD64_StackSlot;


// reg = mov(reg)
UNIT_Status
AMD64_Move_RegReg(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Register src);

// reg = mov([rsp + offset]) (dst.reg in reg field, RSP in rm)
UNIT_Status
AMD64_Move_RegStack(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_StackSlot src);

// reg = mov(imm64) (register encoded in opcode byte, uses REX.B)
UNIT_Status
AMD64_Move_RegIndirect(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Indirect src);

// [rsp + offset] = mov(reg)
UNIT_Status
AMD64_Move_StackReg(_UNIT_CodeBuffer *buffer, AMD64_StackSlot dst, AMD64_Register src);

// reg = mov(imm64)
UNIT_Status
AMD64_Move_RegImmediate(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Immediate src);

UNIT_Status
AMD64_Move_IndirectReg(_UNIT_CodeBuffer *buffer, AMD64_Indirect dst, AMD64_Register src);

UNIT_Status
AMD64_Move8_IndirectReg(_UNIT_CodeBuffer *buffer, AMD64_Indirect dst, AMD64_Register src);

UNIT_Status
AMD64_Move16_IndirectReg(_UNIT_CodeBuffer *buffer, AMD64_Indirect dst, AMD64_Register src);

UNIT_Status
AMD64_Move32_IndirectReg(_UNIT_CodeBuffer *buffer, AMD64_Indirect dst, AMD64_Register src);

// mov reg32, *dword
UNIT_Status
AMD64_Move_RegDerefDword(_UNIT_CodeBuffer *buffer,
                         AMD64_Register dst,
                         AMD64_Register ptr);

// mov reg64, *qword
UNIT_Status
AMD64_Move_RegDerefQword(_UNIT_CodeBuffer *buffer,
                         AMD64_Register dst,
                         AMD64_Register ptr);

// reg64 = movzx(reg8)
UNIT_Status
AMD64_MoveZeroExtend8_RegReg(_UNIT_CodeBuffer *buffer,
                             AMD64_Register dst,
                             AMD64_Register src);

// reg64 = movsx(reg8)
UNIT_Status
AMD64_MoveSignExtend8_RegReg(_UNIT_CodeBuffer *buffer,
                             AMD64_Register dst,
                             AMD64_Register src);

// movzx reg64, reg16
UNIT_Status
AMD64_MoveZeroExtend16_RegReg(_UNIT_CodeBuffer *buffer,
                              AMD64_Register dst,
                              AMD64_Register src);

// movsx reg64, reg16
UNIT_Status
AMD64_MoveSignExtend16_RegReg(_UNIT_CodeBuffer *buffer,
                              AMD64_Register dst,
                              AMD64_Register src);

// mov reg32, reg32 (implicit zero-extend to 64 bit)
UNIT_Status
AMD64_Move32_RegReg(_UNIT_CodeBuffer *buffer,
                    AMD64_Register dst,
                    AMD64_Register src);

// movsxd reg64, reg32
UNIT_Status
AMD64_MoveSignExtendDword_RegReg(_UNIT_CodeBuffer *buffer,
                                 AMD64_Register dst,
                                 AMD64_Register src);

// movzx reg64, *byte
UNIT_Status
AMD64_MoveZeroExtend_RegDerefByte(_UNIT_CodeBuffer *buffer,
                                  AMD64_Register dst,
                                  AMD64_Register ptr);

// movzx reg64, *word
UNIT_Status
AMD64_MoveZeroExtend_RegDerefWord(_UNIT_CodeBuffer *buffer,
                                  AMD64_Register dst,
                                  AMD64_Register ptr);

// reg = cmp(reg, imm8)
UNIT_Status
AMD64_Compare_RegImmediate(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Immediate src);

// dst = cmp(dst, src)
UNIT_Status
AMD64_Compare_RegReg(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Register src);

// reg = cmp(reg, [rsp + offset])
UNIT_Status
AMD64_Compare_RegStack(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_StackSlot src);

// abi_specific_reg = call reg
UNIT_Status
AMD64_CallIndirect(_UNIT_CodeBuffer *buffer, AMD64_Register target);

// abi_specific_reg = call <relocation>
UNIT_Status
AMD64_CallSymbol(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_Syscall(_UNIT_CodeBuffer *buffer);

// dst = add(dst, src)
UNIT_Status
AMD64_Add_RegReg(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Register src);

// dst = add(dst, imm32)
UNIT_Status
AMD64_Add_RegImmediate(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Immediate src);

// dst = sub(dst, src)
UNIT_Status
AMD64_Sub_RegReg(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Register src);

// dst = sub(dst, imm32)
UNIT_Status
AMD64_Sub_RegImmediate(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Immediate src);

// dst = sub(dst, src)
UNIT_Status
AMD64_IntMul_RegReg(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Register src);

// dst = mul(dst, imm32)
UNIT_Status
AMD64_IntMul_RegImmediate(_UNIT_CodeBuffer *buffer, AMD64_Register dst, AMD64_Immediate src);

// RAX (quotient), RDX (remainder) = RDX / divisor
UNIT_Status
AMD64_IntDiv_Reg(_UNIT_CodeBuffer *buffer, AMD64_Register divisor);

// dst = &src
UNIT_Status
AMD64_LoadEffectiveAddress_RegStack(_UNIT_CodeBuffer *buffer,
                                    AMD64_Register dst,
                                    AMD64_StackSlot src);

// dst = &<relocation>
UNIT_Status
AMD64_LoadEffectiveAddress_RegRel(_UNIT_CodeBuffer *buffer,
                                  AMD64_Register dst,
                                  UNIT_Size *relocation_index);

// RDX = (int128)RDX
UNIT_Status
AMD64_ConvertQuadwordToOctoword(_UNIT_CodeBuffer *buffer);

// jmp <relocation>
UNIT_Status
AMD64_Jump_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

// if cmp { je <relocation> }
UNIT_Status
AMD64_JumpEqual_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_JumpNotEqual_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_JumpGreater_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_JumpGreaterEqual_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_JumpLess_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

// jle <relocation>
UNIT_Status
AMD64_JumpLessEqual_Rel(_UNIT_CodeBuffer *buffer, UNIT_Size *relocation_index);

UNIT_Status
AMD64_Return(_UNIT_CodeBuffer *buffer);

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
