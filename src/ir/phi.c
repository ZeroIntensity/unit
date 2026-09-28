#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/translation.h>

static int8_t
same_home(const _UNIT_MachineItem *left, const _UNIT_MachineItem *right)
{
    return left->type == right->type && left->value == right->value;
}

static UNIT_Status
emit_copy(_UNIT_Translation *translation,
          _UNIT_BasicBlock *block,
          _UNIT_MachineItem *destination,
          _UNIT_MachineItem *source)
{
    // Each occurrence must be independent once physical registers can be folded.
    _UNIT_MachineItem *dst = _UNIT_Translation_NewItem(translation,
                                                       destination->type,
                                                       destination->value,
                                                       destination->hint);
    _UNIT_MachineItem *src = _UNIT_Translation_NewItem(translation,
                                                       source->type,
                                                       source->value,
                                                       source->hint);
    if (dst == NULL || src == NULL) {
        return _UNIT_FAIL;
    }

    return _UNIT_Translation_Emit(block,
                                  _UNIT_I_LOAD,
                                  _UNIT_MachineDestination_FromDestination(dst),
                                  src,
                                  NULL);
}

typedef struct {
    _UNIT_MachineItem *destination;
    _UNIT_MachineItem *source;
} Copy;

static UNIT_Status
emit_parallel_copies(_UNIT_Translation *translation,
                     _UNIT_CompileContext *compile_context,
                     _UNIT_BasicBlock *edge,
                     _UNIT_BasicBlock *predecessor,
                     _UNIT_BasicBlock *target,
                     UNIT_Size *temporary_slot)
{
    UNIT_Size count = _UNIT_Vector_SIZE(&target->phis);
    Copy *copies = _UNIT_Alloc(translation->context, count * sizeof(*copies));
    if (copies == NULL) {
        return _UNIT_FAIL;
    }

    UNIT_Size pending = 0;
    for (UNIT_Size p = 0; p < count; ++p) {
        _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&target->phis, p);
        _UNIT_MachineItem *destination = _UNIT_MachineDestination_GetPointer(phi->destination);
        _UNIT_Vector *inputs = phi->argument_1->phi_args;
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(inputs); ++i) {
            _UNIT_PhiInput *input = _UNIT_Vector_GET(inputs, i);
            if (input->predecessor == predecessor && !same_home(destination, input->value)) {
                copies[pending++] = (Copy) {destination, input->value};
            }
        }
    }

    while (pending > 0) {
        int8_t progress = 0;
        for (UNIT_Size i = 0; i < pending; ++i) {
            int8_t needed = 0;
            for (UNIT_Size j = 0; j < pending; ++j) {
                if (same_home(copies[i].destination, copies[j].source)) {
                    needed = 1;
                    break;
                }
            }

            if (needed) {
                continue;
            }

            if (UNIT_FAILED(emit_copy(translation,
                                      edge,
                                      copies[i].destination,
                                      copies[i].source))) {
                goto error;
            }

            copies[i] = copies[--pending];
            progress = 1;
            break;
        }

        if (progress) {
            continue;
        }

        // All remaining copies form cycles. Save one source before overwriting
        // it; the temporary is never a destination in the pending copy set.
        if (*temporary_slot == -1) {
            *temporary_slot = _UNIT_StackFrame_AllocateSlotID(&compile_context->stack_frame);
        }

        _UNIT_MachineItem *temporary = _UNIT_Translation_NewItem(translation,
                                                                 _UNIT_TYPE_MEMORY,
                                                                 *temporary_slot,
                                                                 "phi temporary");
        _UNIT_MachineItem *source = copies[0].source;
        if (temporary == NULL || UNIT_FAILED(emit_copy(translation, edge, temporary, source))) {
            goto error;
        }

        for (UNIT_Size i = 0; i < pending; ++i) {
            if (same_home(copies[i].source, source)) {
                copies[i].source = temporary;
            }
        }
    }

    _UNIT_Dealloc(translation->context, copies);
    return _UNIT_OK;
error:
    _UNIT_Dealloc(translation->context, copies);
    return _UNIT_FAIL;
}

static UNIT_Status
emit_jump(_UNIT_Translation *translation, _UNIT_BasicBlock *block, UNIT_Size label)
{
    _UNIT_MachineItem *target = _UNIT_Translation_NewItem(translation,
                                                          _UNIT_TYPE_CONSTANT,
                                                          label,
                                                          NULL);
    if (target == NULL) {
        return _UNIT_FAIL;
    }

    return _UNIT_Translation_Emit(block, _UNIT_I_JUMP, _UNIT_MachineDestination_NULL, target, NULL);
}

UNIT_Status
_UNIT_Translation_LowerPhis(_UNIT_Translation *translation, _UNIT_CompileContext *compile_context)
{
    UNIT_Size count = _UNIT_Vector_SIZE(&translation->blocks);
    UNIT_Size next_label = 0;
    // Make fallthroughs explicit before adding edge blocks to the end of layout.
    for (UNIT_Size b = 0; b < count; ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        if (block->label_id >= next_label) {
            next_label = block->label_id + 1;
        }

        UNIT_Size successors = _UNIT_Vector_SIZE(&block->successors);
        if (successors == 0) {
            continue;
        }

        _UNIT_MachineOperation *last = _UNIT_Vector_GET(&block->instructions,
                                                        _UNIT_Vector_SIZE(&block->instructions) -
                                                        1);
        if (last->instruction != _UNIT_I_JUMP) {
            _UNIT_BasicBlock *fallthrough = _UNIT_Vector_GET(&block->successors, successors - 1);
            if (UNIT_FAILED(emit_jump(translation, block, fallthrough->label_id))) {
                return _UNIT_FAIL;
            }
        }
    }

    UNIT_Size temporary_slot = -1;
    for (UNIT_Size b = 0; b < count; ++b) {
        _UNIT_BasicBlock *target = _UNIT_Vector_GET(&translation->blocks, b);
        if (_UNIT_Vector_SIZE(&target->phis) == 0) {
            continue;
        }

        for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&target->predecessors); ++p) {
            _UNIT_BasicBlock *predecessor = _UNIT_Vector_GET(&target->predecessors, p);
            _UNIT_BasicBlock *edge = _UNIT_BasicBlock_New(translation->context, next_label++);
            if (edge == NULL || UNIT_FAILED(_UNIT_Vector_Append(&translation->blocks, edge))) {
                return _UNIT_FAIL;
            }

            edge->label_id = edge->id;
            _UNIT_MachineItem *label = _UNIT_Translation_NewItem(translation,
                                                                 _UNIT_TYPE_CONSTANT,
                                                                 edge->label_id,
                                                                 "phi edge");
            if (label == NULL
                || UNIT_FAILED(_UNIT_Translation_Emit(edge,
                                                      _UNIT_I_JUMP_LABEL,
                                                      _UNIT_MachineDestination_FromDestination(
                                                          label),
                                                      NULL,
                                                      NULL))
                || UNIT_FAILED(emit_parallel_copies(translation,
                                                    compile_context,
                                                    edge,
                                                    predecessor,
                                                    target,
                                                    &temporary_slot))
                || UNIT_FAILED(emit_jump(translation, edge, target->label_id))
                || UNIT_FAILED(_UNIT_Vector_Append(&edge->predecessors, predecessor))
                || UNIT_FAILED(_UNIT_Vector_Append(&edge->successors, target))) {
                return _UNIT_FAIL;
            }

            // Rewrite both a taken branch and an explicit fallthrough, including
            // the case where both branches lead to the same successor.
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&predecessor->instructions); ++i) {
                _UNIT_MachineOperation *op = _UNIT_Vector_GET(&predecessor->instructions, i);
                _UNIT_MachineItem *jump = NULL;
                if (op->instruction == _UNIT_I_JUMP) {
                    jump = op->argument_1;
                } else if (op->instruction >= _UNIT_I_JUMP_IF_EQUAL
                           && op->instruction <= _UNIT_I_JUMP_IF_LESS_EQUAL) {
                    jump = _UNIT_MachineDestination_GetPointer(op->destination);
                }

                if (jump != NULL && jump->value == target->label_id) {
                    jump->value = edge->label_id;
                }
            }

            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&predecessor->successors); ++i) {
                if (_UNIT_Vector_GET(&predecessor->successors, i) == target) {
                    predecessor->successors.items[i] = edge;
                }
            }

            target->predecessors.items[p] = edge;
        }

        for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&target->phis); ++p) {
            _UNIT_Dealloc(translation->context, _UNIT_Vector_GET(&target->phis, p));
        }

        target->phis.length = 0;
    }

    return _UNIT_OK;
}
