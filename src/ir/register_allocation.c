#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/register_allocation.h>

// Assign a location once for the entire procedure. In particular, a loop's
// incoming value must have the same home before and after its backedge.
typedef struct {
    _UNIT_Translation *translation;
    _UNIT_SizeSet *interference;
    UNIT_Size *registers;
    UNIT_Size *spills;
} Allocator;

static UNIT_Status
add_uses(_UNIT_SizeSet *live, _UNIT_MachineItem *item)
{
    if (item == NULL) {
        return _UNIT_OK;
    }

    if (item->type == _UNIT_TYPE_LOCATION) {
        return _UNIT_SizeSet_Add(live, item->value);
    }

    if (item->type == _UNIT_TYPE_CALL_ARGS) {
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(item->call_args); ++i) {
            if (UNIT_FAILED(add_uses(live, _UNIT_Vector_GET(item->call_args, i)))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
record_interference(Allocator *allocator, _UNIT_SizeSet *live, _UNIT_MachineDestination dest)
{
    if (_UNIT_MachineDestination_IsNull(dest) || _UNIT_MachineDestination_IsInput(dest)) {
        return _UNIT_OK;
    }

    _UNIT_MachineItem *item = _UNIT_MachineDestination_GetPointer(dest);
    if (item->type != _UNIT_TYPE_LOCATION) {
        return _UNIT_OK;
    }

    UNIT_Size location = item->value;
    _UNIT_SizeSet_ITER(live, other) {
        if (location != other
            && (UNIT_FAILED(_UNIT_SizeSet_Add(&allocator->interference[location], other))
                || UNIT_FAILED(_UNIT_SizeSet_Add(&allocator->interference[other], location)))) {
            return _UNIT_FAIL;
        }
    }
    _UNIT_SizeSet_END_ITER();
    _UNIT_SizeSet_Remove(live, location);
    return _UNIT_OK;
}

static UNIT_Status
build_interference(Allocator *allocator)
{
    _UNIT_Translation *translation = allocator->translation;
    for (UNIT_Size b = 0; b < _UNIT_Vector_SIZE(&translation->blocks); ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        _UNIT_SizeSet live;
        if (UNIT_FAILED(_UNIT_SizeSet_Init(&live, translation->context, 8))) {
            return _UNIT_FAIL;
        }

        _UNIT_SizeSet_ITER(&block->liveness.alive_at_end, location) {
            if (UNIT_FAILED(_UNIT_SizeSet_Add(&live, location))) {
                goto error;
            }
        }
        _UNIT_SizeSet_END_ITER();
        for (UNIT_Size i = _UNIT_Vector_SIZE(&block->instructions); i > 0; --i) {
            _UNIT_MachineOperation *op = _UNIT_Vector_GET(&block->instructions, i - 1);
            if (UNIT_FAILED(record_interference(allocator, &live, op->destination))
                || UNIT_FAILED(add_uses(&live, op->argument_1))
                || UNIT_FAILED(add_uses(&live, op->argument_2))) {
                goto error;
            }

            if (_UNIT_MachineDestination_IsInput(op->destination)
                && UNIT_FAILED(add_uses(&live,
                                        _UNIT_MachineDestination_GetPointer(op->destination)))) {
                goto error;
            }
        }

        // The PHIs define all their results at block entry; their operands were
        // already accounted for on predecessor edges by liveness analysis.
        for (UNIT_Size i = _UNIT_Vector_SIZE(&block->phis); i > 0; --i) {
            _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&block->phis, i - 1);
            if (UNIT_FAILED(record_interference(allocator, &live, phi->destination))) {
                goto error;
            }
        }

        _UNIT_SizeSet_Clear(&live);
        continue;
error:
        _UNIT_SizeSet_Clear(&live);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

static _UNIT_MachineItem *
allocated_item(Allocator *allocator, _UNIT_MachineItem *original)
{
    _UNIT_Translation *translation = allocator->translation;
    _UNIT_MachineItem *item = _UNIT_Translation_NewItem(translation,
                                                        _UNIT_TYPE_CONSTANT,
                                                        0,
                                                        original->hint);
    if (item == NULL) {
        return NULL;
    }

    if (original->type == _UNIT_TYPE_CALL_ARGS || original->type == _UNIT_TYPE_PHI_ARGS) {
        _UNIT_Vector *source = original->call_args;
        _UNIT_Vector *args = _UNIT_Vector_New(translation->context,
                                              _UNIT_Vector_SIZE(source),
                                              original->type ==
                                              _UNIT_TYPE_PHI_ARGS ? _UNIT_Dealloc : NULL);
        if (args == NULL) {
            return NULL;
        }

        item->type = original->type;
        item->call_args = args;
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(source); ++i) {
            if (original->type == _UNIT_TYPE_CALL_ARGS) {
                _UNIT_MachineItem *arg = allocated_item(allocator, _UNIT_Vector_GET(source, i));
                if (arg == NULL) {
                    return NULL;
                }

                _UNIT_Vector_APPEND(args, arg);
            } else {
                _UNIT_PhiInput *input = _UNIT_Vector_GET(source, i);
                _UNIT_MachineItem *value = allocated_item(allocator, input->value);
                if (value == NULL) {
                    return NULL;
                }

                _UNIT_PhiInput *copy = _UNIT_Alloc(translation->context, sizeof(*copy));
                if (copy == NULL) {
                    return NULL;
                }

                *copy = (_UNIT_PhiInput) {input->predecessor, value};
                _UNIT_Vector_APPEND(args, copy);
            }
        }
    } else if (original->type == _UNIT_TYPE_LOCATION) {
        UNIT_Size location = original->value;
        if (allocator->registers[location] >= 0) {
            item->type = _UNIT_TYPE_REGISTER;
            item->value = allocator->registers[location];
        } else {
            item->type = _UNIT_TYPE_MEMORY;
            item->value = allocator->spills[location];
        }
    } else {
        assert(original->type != _UNIT_TYPE_COMPARISON);
        item->type = original->type;
        item->value = original->value;
    }

    return item;
}

static UNIT_Status
rewrite_operations(Allocator *allocator, _UNIT_Vector *operations)
{
    for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(operations); ++i) {
        _UNIT_MachineOperation *op = _UNIT_Vector_GET(operations, i);
        _UNIT_MachineItem **arguments[] = {&op->argument_1, &op->argument_2};
        for (UNIT_Size a = 0; a < 2; ++a) {
            if (*arguments[a] != NULL) {
                *arguments[a] = allocated_item(allocator, *arguments[a]);
                if (*arguments[a] == NULL) {
                    return _UNIT_FAIL;
                }
            }
        }

        if (!_UNIT_MachineDestination_IsNull(op->destination)) {
            _UNIT_MachineItem *item = allocated_item(allocator,
                                                     _UNIT_MachineDestination_GetPointer(
                                                         op->destination));
            if (item == NULL) {
                return _UNIT_FAIL;
            }

            op->destination = _UNIT_MachineDestination_IsInput(op->destination)
                              ? _UNIT_MachineDestination_FromInput(item)
                              : _UNIT_MachineDestination_FromDestination(item);
        }
    }

    return _UNIT_OK;
}

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    _UNIT_CompileContext *compile_context,
                                    int8_t num_registers)
{
    assert(num_registers > 0 && num_registers <= 64);
    UNIT_Context *context = translation->context;
    UNIT_Size count = translation->num_locations;
    UNIT_Size capacity = count ? count : 1;
    Allocator allocator = {.translation = translation};
    allocator.interference = _UNIT_Alloc(context, capacity * sizeof(*allocator.interference));
    allocator.registers = _UNIT_Alloc(context, capacity * sizeof(*allocator.registers));
    allocator.spills = _UNIT_Alloc(context, capacity * sizeof(*allocator.spills));
    UNIT_Size initialized = 0;
    UNIT_Status status = _UNIT_FAIL;
    if (allocator.interference == NULL || allocator.registers == NULL || allocator.spills == NULL) {
        goto done;
    }

    for (UNIT_Size i = 0; i < count; ++i) {
        if (UNIT_FAILED(_UNIT_SizeSet_Init(&allocator.interference[i], context, 4))) {
            goto done;
        }

        ++initialized;
        allocator.registers[i] = -1;
        allocator.spills[i] = -1;
    }

    if (UNIT_FAILED(build_interference(&allocator))) {
        goto done;
    }

    for (UNIT_Size location = 0; location < count; ++location) {
        uint64_t used = 0;
        _UNIT_SizeSet_ITER(&allocator.interference[location], other) {
            if (allocator.registers[other] >= 0) {
                used |= UINT64_C(1) << allocator.registers[other];
            }
        }
        _UNIT_SizeSet_END_ITER();
        for (UNIT_Size reg = 0; reg < num_registers; ++reg) {
            if (!(used & (UINT64_C(1) << reg))) {
                allocator.registers[location] = reg;
                break;
            }
        }

        if (allocator.registers[location] == -1) {
            allocator.spills[location] =
                _UNIT_StackFrame_AllocateSlotID(&compile_context->stack_frame);
        }
    }

    for (UNIT_Size b = 0; b < _UNIT_Vector_SIZE(&translation->blocks); ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        // Independent operand objects are necessary after leaving SSA: physical
        // register folding must not mutate another instruction's operands.
        if (UNIT_FAILED(rewrite_operations(&allocator, &block->phis))
            || UNIT_FAILED(rewrite_operations(&allocator, &block->instructions))) {
            goto done;
        }
    }

    status = _UNIT_OK;
done:
    for (UNIT_Size i = 0; i < initialized; ++i) {
        _UNIT_SizeSet_Clear(&allocator.interference[i]);
    }

    if (allocator.interference != NULL) {
        _UNIT_Dealloc(context, allocator.interference);
    }

    if (allocator.registers != NULL) {
        _UNIT_Dealloc(context, allocator.registers);
    }

    if (allocator.spills != NULL) {
        _UNIT_Dealloc(context, allocator.spills);
    }

    return status;
}
