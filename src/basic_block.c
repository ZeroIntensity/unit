#include <unit/internal/basic_block.h>
#include <unit/internal/translation.h>

UNIT_Status
_UNIT_LivenessInfo_Init(_UNIT_LivenessInfo *liveness, UNIT_Context *context)
{
    assert(liveness != NULL);
    assert(context != NULL);
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->created_locations, context, 8))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->used_locations, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->alive_at_start, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        _UNIT_SizeSet_Clear(&liveness->used_locations);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->alive_at_end, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        _UNIT_SizeSet_Clear(&liveness->used_locations);
        _UNIT_SizeSet_Clear(&liveness->alive_at_start);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

void
_UNIT_LivenessInfo_Clear(_UNIT_LivenessInfo *liveness)
{
    assert(liveness != NULL);
    _UNIT_SizeSet_Clear(&liveness->created_locations);
    _UNIT_SizeSet_Clear(&liveness->used_locations);
    _UNIT_SizeSet_Clear(&liveness->alive_at_start);
    _UNIT_SizeSet_Clear(&liveness->alive_at_end);
}

static UNIT_Status
set_add_and_track(_UNIT_SizeSet *set, UNIT_Size value, int8_t *changed)
{
    if (!_UNIT_SizeSet_Contains(set, value)) {
        if (UNIT_FAILED(_UNIT_SizeSet_Add(set, value))) {
            return _UNIT_FAIL;
        }
        *changed = 1;
    }
    return _UNIT_OK;
}

_UNIT_BasicBlock *
_UNIT_BasicBlock_New(UNIT_Context *context, UNIT_Size id)
{
    assert(context != NULL);
    assert(id >= 0);
    _UNIT_BasicBlock *block = _UNIT_Alloc(context, sizeof(_UNIT_BasicBlock));
    if (block == NULL) {
        return NULL;
    }
    block->context = context;

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->instructions, context,
                                      32, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, block);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->successors, context,
                                      4, NULL))) {
        _UNIT_Dealloc(context, block);
        _UNIT_Vector_Clear(&block->instructions);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_LivenessInfo_Init(&block->liveness, context))) {
        _UNIT_Dealloc(context, block);
        _UNIT_Vector_Clear(&block->instructions);
        _UNIT_Vector_Clear(&block->successors);
        return NULL;
    }

    block->id = id;
    block->label_id = _UNIT_BasicBlock_NO_LABEL; // Can be set later
    return block;
}

void
_UNIT_BasicBlock_Free(UNIT_Context *context, void *ptr)
{
    assert(ptr != NULL);
    _UNIT_BasicBlock *block = (_UNIT_BasicBlock *)ptr;
    _UNIT_Vector_Clear(&block->instructions);
    _UNIT_Vector_Clear(&block->successors);
    _UNIT_LivenessInfo_Clear(&block->liveness);
    _UNIT_Dealloc(block->context, block);
}

static UNIT_Status
populate_liveness_info(_UNIT_Vector *successors,
                       _UNIT_LivenessInfo *liveness,
                       int8_t *changed)
{
    assert(successors != NULL);
    assert(liveness != NULL);
    assert(changed != NULL);

    // alive_at_end = union of alive_at_start of all successors
    UNIT_Size size = _UNIT_Vector_SIZE(successors);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *successor = _UNIT_Vector_GET(successors, index);
        _UNIT_SizeSet_ITER(&successor->liveness.alive_at_start, location);
            if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_end,
                                              location, changed))) {
                return _UNIT_FAIL;
            }
        _UNIT_SizeSet_END_ITER();
    }

    // alive_at_start = used_locations + (alive_at_end - created_locations)

    // Everything this block uses must be alive at its start
    _UNIT_SizeSet_ITER(&liveness->used_locations, location);
        if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_start,
                                          location, changed))) {
            return _UNIT_FAIL;
        }
    _UNIT_SizeSet_END_ITER();

    // Everything alive at the end that this block didn't create
    // must also be alive at the start
    _UNIT_SizeSet_ITER(&liveness->alive_at_end, location);
        if (!_UNIT_SizeSet_Contains(&liveness->created_locations, location)) {
            if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_start,
                                              location, changed))) {
                return _UNIT_FAIL;
            }
        }
    _UNIT_SizeSet_END_ITER();

    return _UNIT_OK;
}

UNIT_Status
_UNIT_BasicBlock_PopulateLivenessStep(_UNIT_BasicBlock *block, int8_t *changed)
{
    assert(block != NULL);
    assert(changed != NULL);
    return populate_liveness_info(&block->successors, &block->liveness, changed);
}
