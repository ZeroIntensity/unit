#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/translation.h>

#include <unit/internal/collections/set.h>

static int8_t
compare_items(_UNIT_MachineItem *left,
              _UNIT_MachineItem *right)
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
            _UNIT_MachineItem *left_item = _UNIT_Vector_GET(left->call_args,
                                                            index);
            assert(left_item != NULL);
            _UNIT_MachineItem *right_item = _UNIT_Vector_GET(right->call_args,
                                                             index);
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
item_matches_or_contains(_UNIT_MachineItem *haystack,
                         _UNIT_MachineItem *needle)
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
            if (item_matches_or_contains(_UNIT_Vector_GET(haystack->call_args,
                                                          index),
                                         needle)) {
                return 1;
            }
        }
    }

    return 0;
}

static int8_t
compare_items_nullable(_UNIT_MachineItem *left,
                       _UNIT_MachineItem *right)
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
item_dead_in_block_recursive(_UNIT_BasicBlock *block,
                             UNIT_Size start,
                             UNIT_Size end,
                             _UNIT_MachineItem *item,
                             _UNIT_Set *checked)
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
        if (operation == NULL) {
            continue;
        }

        _UNIT_MachineItem *destination =
            _UNIT_MachineDestination_GetPointerNullable(operation->destination);
        if (item_matches_or_contains(destination, item)
            || item_matches_or_contains(operation->argument_1, item)
            || item_matches_or_contains(operation->argument_2, item)) {
            return 0;
        }
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&block->successors);
    for (UNIT_Size successor_index = 0; successor_index < size;
         ++successor_index) {
        _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&block->successors,
                                                       successor_index);
        assert(successor != NULL);
        if (_UNIT_Set_Contains(checked, successor)) {
            // A loop may use the value before the current scan's starting point.
            return 0;
        }

        UNIT_Size length = _UNIT_Vector_SIZE(&successor->instructions);
        int8_t result = item_dead_in_block_recursive(successor, 0, length, item, checked);
        if (result != 1) {
            return result;
        }
    }

    return 1;
}

static int8_t
item_dead_in_block(_UNIT_BasicBlock *block,
                   UNIT_Size start,
                   UNIT_Size end,
                   _UNIT_MachineItem *item)
{
    _UNIT_Set checked;
    if (UNIT_FAILED(_UNIT_Set_Init(&checked,
                                   block->context,
                                   _UNIT_Vector_SIZE(&block->successors)))) {
        return -1;
    }

    int8_t result = item_dead_in_block_recursive(block,
                                                 start,
                                                 end,
                                                 item,
                                                 &checked);
    _UNIT_Set_Clear(&checked);
    return result;
}

static UNIT_Status
optimize_block_loads(_UNIT_BasicBlock *block,
                     int8_t *did_change)
{
    assert(block != NULL);
    assert(did_change != NULL);
    *did_change = 0;
    _UNIT_Vector *instrs = &block->instructions;
    UNIT_Size size = _UNIT_Vector_SIZE(instrs);

    _UNIT_Vector new_instructions;
    if (UNIT_FAILED(_UNIT_Vector_Init(&new_instructions,
                                      block->context,
                                      size,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

#define APPEND(op) _UNIT_Vector_APPEND(&new_instructions, op)
#define CONTINUE_AND_DISCARD(op) \
        *did_change = 1;         \
        _UNIT_Dealloc(block->context, op); continue

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *op = _UNIT_Vector_STEAL(instrs, index);
        assert(op != NULL);

        if (op->instruction != _UNIT_I_LOAD) {
            APPEND(op);
            continue;
        }

        assert(!_UNIT_MachineDestination_IsNull(op->destination));
        assert(!_UNIT_MachineDestination_IsInput(op->destination));

        _UNIT_MachineItem *destination =
            _UNIT_MachineDestination_GetPointer(op->destination);
        _UNIT_MachineItem *source = op->argument_1;
        assert(op->argument_2 == NULL);

        // Memory stores are observable through pointers, including ADDRESS_OF.
        if (destination->type == _UNIT_TYPE_MEMORY) {
            APPEND(op);
            continue;
        }

        if (compare_items(destination, source)) {
            // Self-load
            CONTINUE_AND_DISCARD(op);
        }

        int8_t destination_never_used = item_dead_in_block(block,
                                                           index + 1,
                                                           size,
                                                           destination);
        if (destination_never_used == -1) {
            goto error;
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
        int8_t source_never_used = item_dead_in_block(block,
                                                      index + 1,
                                                      size,
                                                      source);
        if (source_never_used == -1) {
            goto error;
        }

        if (!source_never_used) {
            APPEND(op);
            continue;
        }

        _UNIT_MachineOperation *previous = NULL;
        for (UNIT_Size sub_index = _UNIT_Vector_SIZE(&new_instructions);
             sub_index > 0; --sub_index) {
            previous = _UNIT_Vector_GET(&new_instructions, sub_index - 1);
            if (previous != NULL) {
                break;
            }
        }

        if (previous == NULL) {
            APPEND(op);
            continue;
        }

        _UNIT_MachineItem *previous_dest =
            _UNIT_MachineDestination_GetPointerNullable(previous->destination);
        if (!compare_items_nullable(previous_dest, source)) {
            APPEND(op);
            continue;
        }

        if (_UNIT_MachineDestination_IsInput(previous->destination)) {
            APPEND(op);
            continue;
        }

        previous->destination =
            _UNIT_MachineDestination_FromDestination(destination);
        CONTINUE_AND_DISCARD(op);
    }

#undef APPEND
#undef CONTINUE_AND_DISCARD

    _UNIT_Vector_Clear(&block->instructions);
    block->instructions = new_instructions;
    return _UNIT_OK;
error:
    _UNIT_Vector_Clear(&new_instructions);
    return _UNIT_FAIL;
}

typedef struct {
    int64_t value;
    int8_t is_known;
} RegisterValue;

static int8_t
fold_register_value(RegisterValue *register_values,
                    _UNIT_MachineItem *operand)
{
    assert(register_values != NULL);
    if (operand == NULL) {
        return 0;
    }

    if (operand->type == _UNIT_TYPE_REGISTER
        && register_values[operand->value].is_known) {
        operand->type = _UNIT_TYPE_CONSTANT;
        operand->value = register_values[operand->value].value;
        return 1;
    }

    int8_t did_change = 0;
    if (operand->type == _UNIT_TYPE_CALL_ARGS) {
        UNIT_Size size = _UNIT_Vector_SIZE(operand->call_args);
        assert(size >= 0);
        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *arg = _UNIT_Vector_GET(operand->call_args,
                                                      index);
            if (fold_register_value(register_values, arg)) {
                did_change = 1;
            }
        }
    }

    return did_change;
}

static int8_t
fold_binary(_UNIT_MachineInstruction instruction, int64_t left, int64_t right, int64_t *result)
{
    switch (instruction) {
        case _UNIT_I_ADD: {
            *result = (int64_t)((uint64_t)left + (uint64_t)right);
            return 1;
        }
        case _UNIT_I_SUB: {
            *result = (int64_t)((uint64_t)left - (uint64_t)right);
            return 1;
        }
        case _UNIT_I_MUL: {
            *result = (int64_t)((uint64_t)left * (uint64_t)right);
            return 1;
        }
        case _UNIT_I_DIV:
        case _UNIT_I_MOD: {
            // Preserve runtime division traps instead of evaluating them in C.
            if (right == 0 || (left == INT64_MIN && right == -1)) {
                return 0;
            }

            *result = instruction == _UNIT_I_DIV ? left / right : left % right;
            return 1;
        }
        default: {
            return 0;
        }
    }
}

static UNIT_Status
optimize_block_folds(_UNIT_BasicBlock *block,
                     int8_t num_registers,
                     int8_t *did_change)
{
    assert(block != NULL);
    assert(did_change != NULL);
    *did_change = 0;
    _UNIT_Vector *instructions = &block->instructions;
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);

    _UNIT_Vector new_instructions;
    if (UNIT_FAILED(_UNIT_Vector_Init(&new_instructions,
                                      block->context,
                                      size,
                                      _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    RegisterValue *register_values = _UNIT_Calloc(block->context,
                                                  num_registers,
                                                  sizeof(RegisterValue));
    if (register_values == NULL) {
        _UNIT_Vector_Clear(&new_instructions);
        return _UNIT_FAIL;
    }

#define APPEND(op) _UNIT_Vector_APPEND(&new_instructions, op)

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *op = _UNIT_Vector_STEAL(instructions, index);
        assert(op != NULL);

#define FOLD_REGISTER_VALUE(name)                         \
        if (fold_register_value(register_values, name)) { \
            *did_change = 1;                              \
        }

        _UNIT_MachineItem *destination =
            _UNIT_MachineDestination_GetPointerNullable(op->destination);
        if (destination != NULL &&
            _UNIT_MachineDestination_IsInput(op->destination)) {
            FOLD_REGISTER_VALUE(destination);
        }

        FOLD_REGISTER_VALUE(op->argument_1);
        FOLD_REGISTER_VALUE(op->argument_2);

#undef FOLD_REGISTER_VALUE

        switch (op->instruction) {
            case _UNIT_I_LOAD: {
                assert(destination != NULL);
                if (destination->type == _UNIT_TYPE_REGISTER
                    && op->argument_1->type == _UNIT_TYPE_CONSTANT) {
                    register_values[destination->value].value =
                        op->argument_1->value;
                    register_values[destination->value].is_known = 1;
                    // Keep a materialized value for uses in successor blocks.
                    APPEND(op);
                    continue;
                }

                break;
            }

            case _UNIT_I_ADD:
            case _UNIT_I_SUB:
            case _UNIT_I_MUL:
            case _UNIT_I_DIV:
            case _UNIT_I_MOD: {
                int64_t value;
                if (destination->type == _UNIT_TYPE_REGISTER
                    && op->argument_1->type == _UNIT_TYPE_CONSTANT
                    && op->argument_2->type == _UNIT_TYPE_CONSTANT
                    && fold_binary(op->instruction,
                                   op->argument_1->value,
                                   op->argument_2->value,
                                   &value)) {
                    register_values[destination->value] = (RegisterValue) {value, 1};
                    op->instruction = _UNIT_I_LOAD;
                    op->argument_1->value = value;
                    op->argument_2 = NULL;
                    *did_change = 1;
                    APPEND(op);
                    continue;
                }

                break;
            }
            default: {
                break;
            }
        }

        if (destination != NULL
            && !_UNIT_MachineDestination_IsInput(op->destination)
            && destination->type == _UNIT_TYPE_REGISTER) {
            register_values[destination->value].is_known = 0;
        }

        APPEND(op);
    }

#undef APPEND

    _UNIT_Dealloc(block->context, register_values);
    _UNIT_Vector_Clear(&block->instructions);
    block->instructions = new_instructions;
    return _UNIT_OK;
}

static _UNIT_MachineItem *
jump_target(_UNIT_MachineOperation *operation)
{
    if (operation->instruction == _UNIT_I_JUMP) {
        return operation->argument_1;
    }

    if (operation->instruction >= _UNIT_I_JUMP_IF_EQUAL
        && operation->instruction <= _UNIT_I_JUMP_IF_LESS_EQUAL) {
        return _UNIT_MachineDestination_GetPointer(operation->destination);
    }

    return NULL;
}

static UNIT_Status
make_fallthroughs_explicit(_UNIT_Translation *translation)
{
    UNIT_Size count = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index + 1 < count; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
        if (size > 0) {
            _UNIT_MachineOperation *last = _UNIT_Vector_GET(&block->instructions, size - 1);
            if (last->instruction == _UNIT_I_JUMP || last->instruction == _UNIT_I_RETURN_VALUE
                || last->instruction == _UNIT_I_EXIT) {
                continue;
            }
        }

        _UNIT_BasicBlock *next = _UNIT_Vector_GET(&translation->blocks, index + 1);
        _UNIT_MachineItem *target = _UNIT_Translation_NewItem(translation,
                                                              _UNIT_TYPE_CONSTANT,
                                                              next->label_id,
                                                              NULL);
        if (target == NULL
            || UNIT_FAILED(_UNIT_Translation_Emit(block,
                                                  _UNIT_I_JUMP,
                                                  _UNIT_MachineDestination_NULL,
                                                  target,
                                                  NULL))
            || UNIT_FAILED(_UNIT_BasicBlock_AddSuccessor(block, next))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static _UNIT_MachineItem *
forwarding_target(_UNIT_BasicBlock *block)
{
    if (_UNIT_Vector_SIZE(&block->phis) != 0) {
        return NULL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
    UNIT_Size first = 0;
    if (size > 0) {
        _UNIT_MachineOperation *label = _UNIT_Vector_GET(&block->instructions, 0);
        first = label->instruction == _UNIT_I_JUMP_LABEL;
    }

    if (first == size) {
        return NULL;
    }

    _UNIT_MachineOperation *jump = _UNIT_Vector_GET(&block->instructions, first);
    _UNIT_MachineItem *target = jump_target(jump);
    if (target == NULL) {
        return NULL;
    }

    if (jump->instruction == _UNIT_I_JUMP) {
        return size == first + 1 ? target : NULL;
    }

    // A conditional jump is redundant only when its false path has the same target.
    if (size == first + 2) {
        _UNIT_MachineOperation *fallthrough = _UNIT_Vector_GET(&block->instructions, first + 1);
        if (fallthrough->instruction == _UNIT_I_JUMP
            && fallthrough->argument_1->value == target->value) {
            return target;
        }
    }

    return NULL;
}

static void
replace_block_reference(_UNIT_Vector *blocks,
                        _UNIT_BasicBlock *block,
                        _UNIT_BasicBlock *replacement)
{
    UNIT_Size length = 0;
    int8_t has_replacement = 0;
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(blocks); ++index) {
        _UNIT_BasicBlock *item = _UNIT_Vector_GET(blocks, index);
        if (item == block) {
            item = replacement;
        }

        if (item == NULL || (item == replacement && has_replacement)) {
            continue;
        }

        if (item == replacement) {
            has_replacement = 1;
        }

        blocks->items[length++] = item;
    }

    blocks->length = length;
}

static UNIT_Status
optimize_jump_blocks(_UNIT_Translation *translation)
{
    // Block removal, including moving a new entry block, must not change fallthroughs.
    if (UNIT_FAILED(make_fallthroughs_explicit(translation))) {
        return _UNIT_FAIL;
    }

    int8_t did_change;
    do {
        did_change = 0;
        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&translation->blocks);) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
            _UNIT_MachineItem *label = forwarding_target(block);
            _UNIT_BasicBlock *target = NULL;
            if (label != NULL) {
                for (UNIT_Size s = 0; s < _UNIT_Vector_SIZE(&block->successors); ++s) {
                    _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&block->successors, s);
                    if (successor->label_id == label->value) {
                        target = successor;
                        break;
                    }
                }
            }

            // Keep self-loops, including those formed by collapsing a jump cycle.
            if (target == NULL || target == block || _UNIT_Vector_SIZE(&target->phis) != 0) {
                ++index;
                continue;
            }

            for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&block->predecessors); ++p) {
                _UNIT_BasicBlock *predecessor = _UNIT_Vector_GET(&block->predecessors, p);
                if (UNIT_FAILED(_UNIT_BasicBlock_AddSuccessor(predecessor, target))) {
                    return _UNIT_FAIL;
                }

                for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&predecessor->instructions); ++i) {
                    _UNIT_MachineOperation *operation = _UNIT_Vector_GET(&predecessor->instructions,
                                                                         i);
                    _UNIT_MachineItem *old_target = jump_target(operation);
                    if (old_target == NULL || old_target->value != block->label_id) {
                        continue;
                    }

                    if (operation->instruction == _UNIT_I_JUMP) {
                        operation->argument_1 = label;
                    } else {
                        operation->destination = _UNIT_MachineDestination_FromInput(label);
                    }
                }

                replace_block_reference(&predecessor->successors, block, target);
            }

            replace_block_reference(&target->predecessors, block, NULL);
            if (index == 0) {
                // Enter the forwarded destination even when it is not next in layout.
                for (UNIT_Size i = 1; i < _UNIT_Vector_SIZE(&translation->blocks); ++i) {
                    if (_UNIT_Vector_GET(&translation->blocks, i) == target) {
                        translation->blocks.items[0] = target;
                        translation->blocks.items[i] = block;
                        break;
                    }
                }
            }

            replace_block_reference(&translation->blocks, block, NULL);
            _UNIT_BasicBlock_Free(translation->context, block);
            did_change = 1;
        }
    } while (did_change);

    return _UNIT_OK;
}

static void
optimize_jumps(_UNIT_Translation *translation)
{
    UNIT_Size block_count = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index + 1 < block_count; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
        if (size == 0) {
            continue;
        }

        _UNIT_MachineOperation *last = _UNIT_Vector_GET(&block->instructions, size - 1);
        if (last->instruction != _UNIT_I_JUMP) {
            continue;
        }

        assert(last->argument_1 != NULL);
        assert(last->argument_1->type == _UNIT_TYPE_CONSTANT);
        _UNIT_BasicBlock *next = _UNIT_Vector_GET(&translation->blocks, index + 1);
        if (last->argument_1->value == next->label_id) {
            // The edge remains in the CFG; only its explicit jump is redundant.
            _UNIT_Dealloc(block->context, _UNIT_Vector_Pop(&block->instructions));
        }
    }
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))

UNIT_Status
_UNIT_Translation_Optimize(_UNIT_Translation *translation,
                           int8_t num_registers)
{
    assert(translation != NULL);
    UNIT_Size block_count = _UNIT_Vector_SIZE(&translation->blocks);

    for (UNIT_Size i = 0; i < block_count; ++i) {
        int8_t any_did_change = 0;

        do {
            any_did_change = 0;

            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks,
                                                       i);
            assert(block != NULL);
            int8_t did_change;
            if (UNIT_FAILED(optimize_block_loads(block, &did_change))) {
                return _UNIT_FAIL;
            }

            any_did_change = MAX(did_change, any_did_change);

            if (UNIT_FAILED(optimize_block_folds(block,
                                                 num_registers,
                                                 &did_change))) {
                return _UNIT_FAIL;
            }

            any_did_change = MAX(did_change, any_did_change);
        } while (any_did_change);
    }

    if (UNIT_FAILED(optimize_jump_blocks(translation))) {
        return _UNIT_FAIL;
    }

    optimize_jumps(translation);
    return _UNIT_OK;
}
