#include <unit/internal/basic_block.h>
#include <unit/internal/translation.h>

#include <unit/internal/set.h>

static int8_t
compare_items(_UNIT_MachineItem *left, _UNIT_MachineItem *right)
{
    assert(left != NULL);
    assert(right != NULL);

    if (left->type != right->type) {
        return 0;
    }

    if (left->type == _UNIT_TYPE_CALL_ARGS) {
        assert(right->type == _UNIT_TYPE_CALL_ARGS);
        UNIT_Size size = _UNIT_Vector_SIZE(left->call_args);
        if (size != _UNIT_Vector_SIZE(right->call_args)) {
            return 0;
        }

        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *left_item = _UNIT_Vector_GET(left->call_args, index);
            assert(left_item != NULL);
            _UNIT_MachineItem *right_item = _UNIT_Vector_GET(right->call_args, index);
            assert(right_item != NULL);
            if (!compare_items(left_item, right_item)) {
                return 0;
            }
        }

        return 1;
    }

    return left->value == right->value;
}

static int8_t
item_matches_or_contains(_UNIT_MachineItem *haystack, _UNIT_MachineItem *needle)
{
    if (haystack == NULL || needle == NULL) {
        return 0;
    }

    if (haystack->type == needle->type && haystack->value == needle->value) {
        return 1;
    }

    if (haystack->type == _UNIT_TYPE_CALL_ARGS) {
        UNIT_Size size = _UNIT_Vector_SIZE(haystack->call_args);
        for (UNIT_Size index = 0; index < size; ++index) {
            if (item_matches_or_contains(_UNIT_Vector_GET(haystack->call_args, index), needle)) {
                return 1;
            }
        }
    }

    return 0;
}

static int8_t
compare_items_nullable(_UNIT_MachineItem *left, _UNIT_MachineItem *right)
{
    if (left == NULL) {
        return 0;
    }

    if (right == NULL) {
        return 0;
    }

    return compare_items(left, right);
}

static int8_t
item_dead_in_block_recursive(_UNIT_BasicBlock *block, UNIT_Size start,
                             UNIT_Size end, _UNIT_MachineItem *item, _UNIT_Set *checked)
{
    assert(block != NULL);
    assert(start >= 0);
    assert(end >= 0);
    assert(item != NULL);
    _UNIT_Vector *instructions = &block->instructions;

    if (UNIT_FAILED(_UNIT_Set_Add(checked, block))) {
        return -1;
    }

    for (UNIT_Size index = start; index < end; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(instructions, index);
        assert(operation != NULL);
        if (operation == NULL) {
            continue;
        }

        _UNIT_MachineItem *destination = _UNIT_MachineDestination_GetPointerNullable(operation->destination);
        if (item_matches_or_contains(destination, item)
            || item_matches_or_contains(operation->argument_1, item)
            || item_matches_or_contains(operation->argument_2, item)) {
            return 0;
        }
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&block->successors);
    for (UNIT_Size successor_index = 0; successor_index < size; ++successor_index) {
        _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&block->successors, successor_index);
        assert(successor != NULL);
        if (_UNIT_Set_Contains(checked, successor)) {
            continue;
        }

        UNIT_Size length = _UNIT_Vector_SIZE(&successor->instructions);
        if (!item_dead_in_block_recursive(successor, 0, length, item, checked)) {
            return 0;
        }
    }

    return 1;
}

static int8_t
item_dead_in_block(_UNIT_BasicBlock *block, UNIT_Size start,
                   UNIT_Size end, _UNIT_MachineItem *item)
{
    _UNIT_Set checked;
    if (UNIT_FAILED(_UNIT_Set_Init(&checked, block->context,
                                   _UNIT_Vector_SIZE(&block->successors)))) {
        return -1;
    }

    int8_t result = item_dead_in_block_recursive(block, start, end, item, &checked);
    _UNIT_Set_Clear(&checked);
    return result;
}

static UNIT_Status
optimize_block_loads(_UNIT_BasicBlock *block, int8_t *did_change)
{
    assert(block != NULL);
    assert(did_change != NULL);
    *did_change = 0;
    _UNIT_Vector *instrs = &block->instructions;
    UNIT_Size size = _UNIT_Vector_SIZE(instrs);

    _UNIT_Vector new_instructions;
    if (UNIT_FAILED(_UNIT_Vector_Init(&new_instructions, block->context, size,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

#define APPEND(op) _UNIT_Vector_APPEND(&new_instructions, op)
#define CONTINUE_AND_DISCARD(op) *did_change = 1; _UNIT_Dealloc(block->context, op); continue

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *op = _UNIT_Vector_STEAL(instrs, index);
        assert(op != NULL);

        if (op->instruction != _UNIT_I_LOAD) {
            APPEND(op);
            continue;
        }
        assert(!_UNIT_MachineDestination_IsNull(op->destination));
        assert(!_UNIT_MachineDestination_IsInput(op->destination));

        _UNIT_MachineItem *destination = _UNIT_MachineDestination_GetPointer(op->destination);
        _UNIT_MachineItem *source = op->argument_1;
        assert(op->argument_2 == NULL);

        if (compare_items(destination, source)) {
            // Self-load
            CONTINUE_AND_DISCARD(op);
        }

        int8_t destination_never_used = item_dead_in_block(block, index + 1, size, destination);
        if (destination_never_used == -1) {
            return _UNIT_FAIL;
        }

        if (destination_never_used) {
            CONTINUE_AND_DISCARD(op);
        }

        // If the previous instruction produced src, and src is dead after this load,
        // change the previous instruction's destination and delete the load.
        //
        // For example:
        // register_0 = ADD(register_1, 1)
        // register_2 = LOAD(register_0)
        //
        // can be turned into
        //
        // register_2 = ADD(register_1, 1)
        int8_t source_never_used = item_dead_in_block(block, index + 1, size, source);
        if (source_never_used == -1) {
            return _UNIT_FAIL;
        }
        if (!source_never_used) {
            APPEND(op);
            continue;
        }

        _UNIT_MachineOperation *previous = NULL;
        for (UNIT_Size sub_index = _UNIT_Vector_SIZE(&new_instructions); sub_index > 0; --sub_index) {
            previous = _UNIT_Vector_GET(&new_instructions, sub_index - 1);
            if (previous != NULL) {
                break;
            }
        }

        if (previous == NULL) {
            APPEND(op);
            continue;
        }

        _UNIT_MachineItem *previous_dest = _UNIT_MachineDestination_GetPointerNullable(previous->destination);
        if (!compare_items_nullable(previous_dest, source)) {
            APPEND(op);
            continue;
        }

        assert(!_UNIT_MachineDestination_IsInput(previous->destination));
        previous->destination = _UNIT_MachineDestination_FromDestination(destination);
        CONTINUE_AND_DISCARD(op);
    }

#undef APPEND
#undef CONTINUE_AND_DISCARD

    _UNIT_Vector_Clear(&block->instructions);
    block->instructions = new_instructions;
    return _UNIT_OK;
}

UNIT_Status
_UNIT_Translation_Optimize(_UNIT_Translation *translation)
{
    assert(translation != NULL);
    UNIT_Size block_count = _UNIT_Vector_SIZE(&translation->blocks);

    for (UNIT_Size i = 0; i < block_count; ++i) {
        while (true) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, i);
            assert(block != NULL);
            int8_t did_change;
            if (UNIT_FAILED(optimize_block_loads(block, &did_change))) {
                return _UNIT_FAIL;
            }

            if (!did_change) {
                break;
            }
        }
    }

    return _UNIT_OK;
}
