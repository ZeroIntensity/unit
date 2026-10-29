#ifndef UNIT_REGISTER_ALLOCATION_H
#define UNIT_REGISTER_ALLOCATION_H

#include <unit/internal/base.h>
#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/translation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const _UNIT_BasicBlock *block;
    UNIT_Size instruction_count;
    uint64_t *boundaries;
} _UNIT_RegisterBlockLiveness;

typedef struct {
    UNIT_Context *context;
    _UNIT_Translation *translation;
    _UNIT_SizeSet *interference;
    UNIT_Size initialized_interference;
    UNIT_Size *registers;
    UNIT_Size *spills;
    _UNIT_RegisterBlockLiveness *liveness_blocks;
    UNIT_Size num_liveness_blocks;
} _UNIT_RegisterAllocator;

void
_UNIT_RegisterAllocator_Init(_UNIT_RegisterAllocator *allocator, UNIT_Context *context);

void
_UNIT_RegisterAllocator_Clear(_UNIT_RegisterAllocator *allocator);

// Analyze the final physical-register IR after PHI lowering and optimization.
UNIT_Status
_UNIT_RegisterAllocator_AnalyzeLiveness(_UNIT_RegisterAllocator *allocator,
                                        const _UNIT_Translation *translation);

// Index i describes the boundary before instruction i; instruction_count is
// the boundary at block exit. Register indices are allocator register numbers.
int8_t
_UNIT_RegisterAllocator_IsLive(const _UNIT_RegisterAllocator *allocator,
                               const _UNIT_BasicBlock *block,
                               UNIT_Size instruction_index,
                               UNIT_Size register_index);

struct _UNIT_CompileContext;

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    struct _UNIT_CompileContext *compile_context,
                                    int8_t num_registers);

#ifdef __cplusplus
}
#endif

#endif
