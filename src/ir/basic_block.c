#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/translation.h>

UNIT_Status
_UNIT_LivenessInfo_Init(_UNIT_LivenessInfo *liveness,
                        UNIT_Context *context)
{
    assert(liveness != NULL);
    assert(context != NULL);
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->created_locations,
                                       context,
                                       8))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->used_locations,
                                       context,
                                       8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->alive_at_start,
                                       context,
                                       8))) {
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

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&liveness->last_uses, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        _UNIT_SizeSet_Clear(&liveness->used_locations);
        _UNIT_SizeSet_Clear(&liveness->alive_at_start);
        _UNIT_SizeSet_Clear(&liveness->alive_at_end);
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
    _UNIT_SizeMap_Clear(&liveness->last_uses);
}

static UNIT_Status
set_add_and_track(_UNIT_SizeSet *set,
                  UNIT_Size value,
                  int8_t *changed)
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
_UNIT_BasicBlock_New(UNIT_Context *context,
                     UNIT_Size id)
{
    assert(context != NULL);
    assert(id >= 0);
    _UNIT_BasicBlock *block = _UNIT_Alloc(context, sizeof(_UNIT_BasicBlock));
    if (block == NULL) {
        return NULL;
    }

    block->context = context;

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->instructions,
                                      context,
                                      32,
                                      _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, block);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->phis, context, 4, _UNIT_Dealloc))) {
        goto instructions_error;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->successors, context, 2, NULL))) {
        goto phis_error;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&block->predecessors, context, 2, NULL))) {
        goto successors_error;
    }

    if (UNIT_FAILED(_UNIT_LivenessInfo_Init(&block->liveness, context))) {
        goto predecessors_error;
    }

    block->id = id;
    block->label_id = _UNIT_BasicBlock_NO_LABEL; // Can be set later
    return block;
predecessors_error:
    _UNIT_Vector_Clear(&block->predecessors);
successors_error:
    _UNIT_Vector_Clear(&block->successors);
phis_error:
    _UNIT_Vector_Clear(&block->phis);
instructions_error:
    _UNIT_Vector_Clear(&block->instructions);
    _UNIT_Dealloc(context, block);
    return NULL;
}

void
_UNIT_BasicBlock_Free(UNIT_Context *context,
                      void *ptr)
{
    (void)context;
    assert(ptr != NULL);
    _UNIT_BasicBlock *block = (_UNIT_BasicBlock *)ptr;
    _UNIT_Vector_Clear(&block->instructions);
    _UNIT_Vector_Clear(&block->successors);
    _UNIT_Vector_Clear(&block->predecessors);
    _UNIT_Vector_Clear(&block->phis);
    _UNIT_LivenessInfo_Clear(&block->liveness);
    _UNIT_Dealloc(block->context, block);
}

UNIT_Status
_UNIT_BasicBlock_AddSuccessor(_UNIT_BasicBlock *block, _UNIT_BasicBlock *successor)
{
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&block->successors); ++index) {
        if (_UNIT_Vector_GET(&block->successors, index) == successor) {
            return _UNIT_OK;
        }
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&block->successors, successor))) {
        return _UNIT_FAIL;
    }

    return _UNIT_Vector_Append(&successor->predecessors, block);
}

UNIT_Status
_UNIT_BasicBlock_PopulateLivenessStep(_UNIT_BasicBlock *block, int8_t *changed)
{
    _UNIT_LivenessInfo *liveness = &block->liveness;
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&block->successors); ++index) {
        _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&block->successors, index);
        _UNIT_SizeSet_ITER(&successor->liveness.alive_at_start, location) {
            if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_end, location, changed))) {
                return _UNIT_FAIL;
            }
        }
        _UNIT_SizeSet_END_ITER();
        // PHI operands are uses on a particular edge, not uses in the successor.
        for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&successor->phis); ++p) {
            _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&successor->phis, p);
            _UNIT_Vector *inputs = phi->argument_1->phi_args;
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(inputs); ++i) {
                _UNIT_PhiInput *input = _UNIT_Vector_GET(inputs, i);
                if (input->predecessor == block && input->value->type == _UNIT_TYPE_LOCATION) {
                    if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_end,
                                                      input->value->value,
                                                      changed))
                        || UNIT_FAILED(_UNIT_SizeMap_Set(&liveness->last_uses,
                                                         input->value->value,
                                                         _UNIT_Vector_SIZE(
                                                             &block->instructions)))) {
                        return _UNIT_FAIL;
                    }
                }
            }
        }
    }

    _UNIT_SizeSet_ITER(&liveness->used_locations, location) {
        if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_start, location, changed))) {
            return _UNIT_FAIL;
        }
    }
    _UNIT_SizeSet_END_ITER();
    _UNIT_SizeSet_ITER(&liveness->alive_at_end, location) {
        if (!_UNIT_SizeSet_Contains(&liveness->created_locations, location)
            && UNIT_FAILED(set_add_and_track(&liveness->alive_at_start, location, changed))) {
            return _UNIT_FAIL;
        }
    }
    _UNIT_SizeSet_END_ITER();
    return _UNIT_OK;
}

static UNIT_Status
record_use(_UNIT_LivenessInfo *liveness, _UNIT_MachineItem *item, UNIT_Size index)
{
    if (item == NULL) {
        return _UNIT_OK;
    }

    if (item->type == _UNIT_TYPE_CALL_ARGS) {
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(item->call_args); ++i) {
            if (UNIT_FAILED(record_use(liveness, _UNIT_Vector_GET(item->call_args, i), index))) {
                return _UNIT_FAIL;
            }
        }
    } else if (item->type == _UNIT_TYPE_LOCATION) {
        if (!_UNIT_SizeSet_Contains(&liveness->created_locations, item->value)
            && UNIT_FAILED(_UNIT_SizeSet_Add(&liveness->used_locations, item->value))) {
            return _UNIT_FAIL;
        }

        return _UNIT_SizeMap_Set(&liveness->last_uses, item->value, index);
    }

    return _UNIT_OK;
}

static UNIT_Status
record_definition(_UNIT_LivenessInfo *liveness, _UNIT_MachineDestination dest, UNIT_Size index)
{
    if (_UNIT_MachineDestination_IsNull(dest) || _UNIT_MachineDestination_IsInput(dest)) {
        return _UNIT_OK;
    }

    _UNIT_MachineItem *item = _UNIT_MachineDestination_GetPointer(dest);
    if (item->type == _UNIT_TYPE_LOCATION) {
        if (UNIT_FAILED(_UNIT_SizeSet_Add(&liveness->created_locations, item->value))) {
            return _UNIT_FAIL;
        }

        return _UNIT_SizeMap_Set(&liveness->last_uses, item->value, index);
    }

    return _UNIT_OK;
}

UNIT_Status
_UNIT_Translation_AnalyzeLiveness(_UNIT_Translation *translation)
{
    UNIT_Size count = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < count; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        _UNIT_LivenessInfo *liveness = &block->liveness;
        // Recompute after any IR changes; stale uses otherwise inflate live ranges.
        _UNIT_SizeSet *sets[] = {&liveness->created_locations, &liveness->used_locations,
                                 &liveness->alive_at_start, &liveness->alive_at_end};
        for (UNIT_Size s = 0; s < 4; ++s) {
            for (UNIT_Size i = 0; i < sets[s]->capacity; ++i) {
                sets[s]->items[i].is_populated = 0;
            }

            sets[s]->len = 0;
        }

        for (UNIT_Size i = 0; i < liveness->last_uses.capacity; ++i) {
            liveness->last_uses.items[i].is_populated = 0;
        }

        liveness->last_uses.len = 0;
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&block->phis); ++i) {
            _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&block->phis, i);
            if (UNIT_FAILED(record_definition(liveness, phi->destination, 0))) {
                return _UNIT_FAIL;
            }
        }

        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&block->instructions); ++i) {
            _UNIT_MachineOperation *op = _UNIT_Vector_GET(&block->instructions, i);
            if (_UNIT_MachineDestination_IsInput(op->destination)
                && UNIT_FAILED(record_use(liveness,
                                          _UNIT_MachineDestination_GetPointer(op->destination),
                                          i))) {
                return _UNIT_FAIL;
            }

            if (UNIT_FAILED(record_use(liveness, op->argument_1, i))
                || UNIT_FAILED(record_use(liveness, op->argument_2, i))
                || UNIT_FAILED(record_definition(liveness, op->destination, i))) {
                return _UNIT_FAIL;
            }
        }
    }

    int8_t changed;
    do {
        changed = 0;
        for (UNIT_Size index = count; index > 0; --index) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index - 1);
            if (UNIT_FAILED(_UNIT_BasicBlock_PopulateLivenessStep(block, &changed))) {
                return _UNIT_FAIL;
            }
        }
    } while (changed);
    return _UNIT_OK;
}
