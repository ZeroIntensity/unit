#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/register_allocation.h>

void
_UNIT_RegisterAllocator_Init(_UNIT_RegisterAllocator *allocator, UNIT_Context *context)
{
    *allocator = (_UNIT_RegisterAllocator) {.context = context};
}

static void
clear_interference(_UNIT_RegisterAllocator *allocator)
{
    for (UNIT_Size i = 0; i < allocator->initialized_interference; ++i) {
        _UNIT_SizeSet_Clear(&allocator->interference[i]);
    }

    if (allocator->interference != NULL) {
        _UNIT_Dealloc(allocator->context, allocator->interference);
    }

    allocator->interference = NULL;
    allocator->initialized_interference = 0;
}

static void
clear_liveness(_UNIT_RegisterAllocator *allocator)
{
    for (UNIT_Size b = 0; b < allocator->num_liveness_blocks; ++b) {
        if (allocator->liveness_blocks[b].boundaries != NULL) {
            _UNIT_Dealloc(allocator->context, allocator->liveness_blocks[b].boundaries);
        }
    }

    if (allocator->liveness_blocks != NULL) {
        _UNIT_Dealloc(allocator->context, allocator->liveness_blocks);
    }

    allocator->liveness_blocks = NULL;
    allocator->num_liveness_blocks = 0;
}

void
_UNIT_RegisterAllocator_Clear(_UNIT_RegisterAllocator *allocator)
{
    clear_interference(allocator);
    clear_liveness(allocator);
    if (allocator->registers != NULL) {
        _UNIT_Dealloc(allocator->context, allocator->registers);
    }

    if (allocator->spills != NULL) {
        _UNIT_Dealloc(allocator->context, allocator->spills);
    }

    _UNIT_RegisterAllocator_Init(allocator, allocator->context);
}

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
record_interference(_UNIT_RegisterAllocator *allocator,
                    _UNIT_SizeSet *live,
                    _UNIT_MachineDestination dest)
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
build_interference(_UNIT_RegisterAllocator *allocator)
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
allocated_item(_UNIT_RegisterAllocator *allocator, _UNIT_MachineItem *original)
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
rewrite_operations(_UNIT_RegisterAllocator *allocator, _UNIT_Vector *operations)
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
    _UNIT_RegisterAllocator *allocator = &compile_context->register_allocator;
    _UNIT_RegisterAllocator_Clear(allocator);
    allocator->translation = translation;
    allocator->interference = _UNIT_Alloc(context, capacity * sizeof(*allocator->interference));
    allocator->registers = _UNIT_Alloc(context, capacity * sizeof(*allocator->registers));
    allocator->spills = _UNIT_Alloc(context, capacity * sizeof(*allocator->spills));
    UNIT_Status status = _UNIT_FAIL;
    if (allocator->interference == NULL || allocator->registers == NULL ||
        allocator->spills == NULL) {
        goto done;
    }

    for (UNIT_Size i = 0; i < count; ++i) {
        if (UNIT_FAILED(_UNIT_SizeSet_Init(&allocator->interference[i], context, 4))) {
            goto done;
        }

        ++allocator->initialized_interference;
        allocator->registers[i] = -1;
        allocator->spills[i] = -1;
    }

    if (UNIT_FAILED(build_interference(allocator))) {
        goto done;
    }

    // Assign a location once for the entire procedure. In particular, a loop's
    // incoming value must have the same home before and after its backedge.
    for (UNIT_Size location = 0; location < count; ++location) {
        uint64_t used = 0;
        _UNIT_SizeSet_ITER(&allocator->interference[location], other) {
            if (allocator->registers[other] >= 0) {
                used |= UINT64_C(1) << allocator->registers[other];
            }
        }
        _UNIT_SizeSet_END_ITER();
        for (UNIT_Size reg = 0; reg < num_registers; ++reg) {
            if (!(used & (UINT64_C(1) << reg))) {
                allocator->registers[location] = reg;
                break;
            }
        }

        if (allocator->registers[location] == -1) {
            allocator->spills[location] =
                _UNIT_StackFrame_AllocateSlotID(&compile_context->stack_frame);
        }
    }

    for (UNIT_Size b = 0; b < _UNIT_Vector_SIZE(&translation->blocks); ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        // Independent operand objects are necessary after leaving SSA: physical
        // register folding must not mutate another instruction's operands.
        if (UNIT_FAILED(rewrite_operations(allocator, &block->phis))
            || UNIT_FAILED(rewrite_operations(allocator, &block->instructions))) {
            goto done;
        }
    }

    status = _UNIT_OK;
done:
    clear_interference(allocator);
    return status;
}

static const _UNIT_RegisterBlockLiveness *
block_liveness(const _UNIT_RegisterAllocator *allocator, const _UNIT_BasicBlock *block)
{
    if (block->id >= 0 && block->id < allocator->num_liveness_blocks
        && allocator->liveness_blocks[block->id].block == block) {
        return &allocator->liveness_blocks[block->id];
    }

    // PHI edge blocks can have IDs that differ from their layout indices.
    for (UNIT_Size b = 0; b < allocator->num_liveness_blocks; ++b) {
        if (allocator->liveness_blocks[b].block == block) {
            return &allocator->liveness_blocks[b];
        }
    }

    return NULL;
}

static uint64_t
register_uses(const _UNIT_MachineItem *item)
{
    if (item == NULL) {
        return 0;
    }

    if (item->type == _UNIT_TYPE_REGISTER) {
        assert(item->value >= 0 && item->value < 64);
        return UINT64_C(1) << item->value;
    }

    uint64_t uses = 0;
    if (item->type == _UNIT_TYPE_CALL_ARGS) {
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(item->call_args); ++i) {
            uses |= register_uses(_UNIT_Vector_GET(item->call_args, i));
        }
    }

    return uses;
}

UNIT_Status
_UNIT_RegisterAllocator_AnalyzeLiveness(_UNIT_RegisterAllocator *allocator,
                                        const _UNIT_Translation *translation)
{
    clear_liveness(allocator);
    UNIT_Size count = _UNIT_Vector_SIZE(&translation->blocks);
    if (count == 0) {
        return _UNIT_OK;
    }

    allocator->liveness_blocks = _UNIT_Calloc(allocator->context,
                                              count,
                                              sizeof(*allocator->liveness_blocks));
    if (allocator->liveness_blocks == NULL) {
        return _UNIT_FAIL;
    }

    allocator->num_liveness_blocks = count;
    for (UNIT_Size b = 0; b < count; ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        assert(_UNIT_Vector_SIZE(&block->phis) == 0);
        _UNIT_RegisterBlockLiveness *info = &allocator->liveness_blocks[b];
        info->block = block;
        info->instruction_count = _UNIT_Vector_SIZE(&block->instructions);
        info->boundaries = _UNIT_Calloc(allocator->context,
                                        info->instruction_count + 1,
                                        sizeof(*info->boundaries));
        if (info->boundaries == NULL) {
            return _UNIT_FAIL;
        }
    }

    int8_t changed;
    do {
        changed = 0;
        for (UNIT_Size b = count; b > 0; --b) {
            _UNIT_RegisterBlockLiveness *info = &allocator->liveness_blocks[b - 1];
            const _UNIT_BasicBlock *block = info->block;
            uint64_t old_entry = info->boundaries[0];
            uint64_t live = 0;
            for (UNIT_Size s = 0; s < _UNIT_Vector_SIZE(&block->successors); ++s) {
                const _UNIT_RegisterBlockLiveness *successor =
                    block_liveness(allocator, _UNIT_Vector_GET(&block->successors, s));
                assert(successor != NULL);
                live |= successor->boundaries[0];
            }

            info->boundaries[info->instruction_count] = live;
            for (UNIT_Size i = info->instruction_count; i > 0; --i) {
                _UNIT_MachineOperation *op = _UNIT_Vector_GET(&block->instructions, i - 1);
                const _UNIT_MachineItem *destination =
                    _UNIT_MachineDestination_GetPointerNullable(op->destination);
                if (_UNIT_MachineDestination_IsInput(op->destination)) {
                    live |= register_uses(destination);
                } else if (destination != NULL && destination->type == _UNIT_TYPE_REGISTER) {
                    live &= ~register_uses(destination);
                }

                live |= register_uses(op->argument_1) | register_uses(op->argument_2);
                info->boundaries[i - 1] = live;
            }

            if (old_entry != info->boundaries[0]) {
                changed = 1;
            }
        }
    } while (changed);

    return _UNIT_OK;
}

int8_t
_UNIT_RegisterAllocator_IsLive(const _UNIT_RegisterAllocator *allocator,
                               const _UNIT_BasicBlock *block,
                               UNIT_Size instruction_index,
                               UNIT_Size register_index)
{
    const _UNIT_RegisterBlockLiveness *info = block_liveness(allocator, block);
    assert(info != NULL);
    assert(instruction_index >= 0 && instruction_index <= info->instruction_count);
    assert(register_index >= 0 && register_index < 64);
    return (info->boundaries[instruction_index] & (UINT64_C(1) << register_index)) != 0;
}
