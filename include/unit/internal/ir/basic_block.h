#ifndef UNIT_BASIC_BLOCK_H
#define UNIT_BASIC_BLOCK_H

#include <unit/internal/base.h>

#include <unit/internal/collections/size_map.h>
#include <unit/internal/collections/size_set.h>
#include <unit/internal/collections/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

static const UNIT_Size _UNIT_BasicBlock_NO_LABEL = -1;

typedef struct {
    _UNIT_SizeSet created_locations;
    _UNIT_SizeSet used_locations;
    _UNIT_SizeSet alive_at_start;
    _UNIT_SizeSet alive_at_end;
    _UNIT_SizeMap last_uses;
} _UNIT_LivenessInfo;

UNIT_Status
_UNIT_LivenessInfo_Init(_UNIT_LivenessInfo *liveness, UNIT_Context *context);

void
_UNIT_LivenessInfo_Clear(_UNIT_LivenessInfo *liveness);

typedef struct _UNIT_BasicBlock {
    UNIT_Context *context;
    UNIT_Size id;
    UNIT_Size label_id; // or _UNIT_BasicBlock_NO_LABEL
    _UNIT_Vector instructions; // Holds _UNIT_MachineOperation*
    _UNIT_Vector phis; // Holds _UNIT_MachineOperation* (_UNIT_I_PHI)
    _UNIT_Vector successors; // Holds (unowned) _UNIT_BasicBlock*
    _UNIT_Vector predecessors; // Holds (unowned) _UNIT_BasicBlock*
    _UNIT_LivenessInfo liveness;
} _UNIT_BasicBlock;

_UNIT_BasicBlock *
_UNIT_BasicBlock_New(UNIT_Context *context, UNIT_Size id);

void
_UNIT_BasicBlock_Free(UNIT_Context *context, void *ptr);

UNIT_Status
_UNIT_BasicBlock_AddSuccessor(_UNIT_BasicBlock *block,
                              _UNIT_BasicBlock *successor);

/* Call this in a loop until *changed is zero. */
UNIT_Status
_UNIT_BasicBlock_PopulateLivenessStep(_UNIT_BasicBlock *block,
                                      int8_t *changed);

#ifdef __cplusplus
}
#endif

#endif
