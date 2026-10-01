#include <stdio.h>

#include <unit/internal/allocation.h>
#include <unit/internal/utils.h>

#include <unit/internal/errors.h>

#include <unit/internal/ir/procedure.h>
#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/translation.h>

#define NAME(name)             \
        case _UNIT_I_ ## name: \
            return #name

const char *
machine_instruction_name(_UNIT_MachineInstruction machine_instruction)
{
    switch (machine_instruction) {
        NAME(LOAD);
        NAME(PHI);
        NAME(JUMP_LABEL);
        NAME(CALL_SYMBOL);
        NAME(JUMP);
        NAME(EXIT);
        NAME(RETURN_VALUE);
        NAME(COMPARE_EQUAL);
        NAME(JUMP_IF_EQUAL);
        NAME(JUMP_IF_NOT_EQUAL);
        NAME(JUMP_IF_LESS);
        NAME(JUMP_IF_LESS_EQUAL);
        NAME(JUMP_IF_GREATER);
        NAME(JUMP_IF_GREATER_EQUAL);
        NAME(LOAD_STRING);
        NAME(ADDRESS_OF);
        NAME(ADD);
        NAME(SUB);
        NAME(MUL);
        NAME(DIV);
        NAME(MOD);
        NAME(READ_BYTES);
        NAME(WRITE_BYTES);
        NAME(LOAD_ARGUMENT);
        NAME(CONVERT);
    }
    _UNIT_Unreachable();
}

#undef NAME

const char *
integer_type_name(UNIT_IntegerType type)
{
#define INT_TYPE(name)           \
        case UNIT_TYPE_ ## name: \
            return #name
    switch (type) {
        INT_TYPE(INT8);
        INT_TYPE(INT16);
        INT_TYPE(INT32);
        INT_TYPE(INT64);
        INT_TYPE(UINT8);
        INT_TYPE(UINT16);
        INT_TYPE(UINT32);
        INT_TYPE(UINT64);
    }
#undef INT_TYPE
    _UNIT_Unreachable();
}

UNIT_Status
print_machine_item(FILE *stream,
                   _UNIT_MachineItem *item,
                   UNIT_Context *context)
{
#define PRINT(...)                                              \
        if (fprintf(stream, __VA_ARGS__) < 0) {                 \
            _UNIT_SetOSError(context, "printing machine item"); \
            return _UNIT_FAIL;                                  \
        }

    assert(item != NULL);
    if (item->type == _UNIT_TYPE_CONSTANT) {
        PRINT("%lld", (long long)item->value);
    } else if (item->type == _UNIT_TYPE_LOCATION) {
        PRINT("location_%lld", (long long)item->value);
    } else if (item->type == _UNIT_TYPE_CALL_ARGS) {
        PRINT("[");
        UNIT_Size size = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(item->call_args,
                                                           index);
            assert(arg_item != NULL);
            if (UNIT_FAILED(print_machine_item(stream, arg_item, context))) {
                return _UNIT_FAIL;
            }

            if (index + 1 != size) {
                PRINT(", ");
            }
        }

        PRINT("]");
    } else if (item->type == _UNIT_TYPE_PHI_ARGS) {
        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(item->phi_args); ++index) {
            _UNIT_PhiInput *input = _UNIT_Vector_GET(item->phi_args, index);
            PRINT("%s[block %lld: ", index ? ", " : "", (long long)input->predecessor->id);
            if (UNIT_FAILED(print_machine_item(stream, input->value, context))) {
                return _UNIT_FAIL;
            }

            PRINT("]");
        }
    } else if (item->type == _UNIT_TYPE_COMPARISON) {
        if (UNIT_FAILED(print_machine_item(stream,
                                           item->comparison.left,
                                           context))) {
            return _UNIT_FAIL;
        }

        switch (item->comparison.type) {
            case UNIT_OP_COMPARE_EQUAL: {
                PRINT(" == ");
                break;
            }
            case UNIT_OP_COMPARE_NOT_EQUAL: {
                PRINT(" != ");
                break;
            }
            case UNIT_OP_COMPARE_LESS: {
                PRINT(" < ");
                break;
            }
            case UNIT_OP_COMPARE_LESS_EQUAL: {
                PRINT(" <= ");
                break;
            }
            case UNIT_OP_COMPARE_GREATER: {
                PRINT(" > ");
                break;
            }
            case UNIT_OP_COMPARE_GREATER_EQUAL: {
                PRINT(" >= ");
                break;
            }
            default: {
                _UNIT_Unreachable();
            }
        }
        if (UNIT_FAILED(print_machine_item(stream,
                                           item->comparison.right,
                                           context))) {
            return _UNIT_FAIL;
        }
    } else if (item->type == _UNIT_TYPE_MEMORY) {
        PRINT("stack_slot_%lld", (long long)item->value);
    } else {
        assert(item->type == _UNIT_TYPE_REGISTER);
        PRINT("register_%lld", (long long)item->value);
    }

    if (item->hint != NULL) {
        PRINT(" (");
        PRINT("\"");
        _UNIT_PrintString(context, item->hint, stream);
        PRINT("\"");
        PRINT(")");
    }

#undef PRINT

    return _UNIT_OK;
}

static UNIT_Status
print_instruction_stream(FILE *stream,
                         _UNIT_Vector *instructions)
{
#define PRINT(...)                                           \
        if (fprintf(stream, __VA_ARGS__) < 0) {              \
            _UNIT_SetOSError(instructions->context,          \
                             "printing instruction stream"); \
            return _UNIT_FAIL;                               \
        }

    assert(instructions != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(instructions,
                                                             index);
        assert(operation != NULL);
        if (operation->instruction == _UNIT_I_JUMP_LABEL) {
            continue;
        }

        assert(operation != NULL);
        PRINT("        ");
        _UNIT_MachineItem *destination =
            _UNIT_MachineDestination_GetPointerNullable(
                operation->destination);
        int8_t is_input =
            _UNIT_MachineDestination_IsInput(operation->destination);
        if (destination != NULL && !is_input) {
            if (UNIT_FAILED(print_machine_item(stream,
                                               destination,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }

            PRINT(" = ");
        }

        PRINT("%s(", machine_instruction_name(operation->instruction));
        if (destination != NULL && is_input) {
            // There's no reason to use the destination as an input if one of
            // the other two arguments are free.
            assert(operation->argument_1 != NULL);
            assert(operation->argument_2 != NULL);
            if (UNIT_FAILED(print_machine_item(stream,
                                               destination,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }

            PRINT(", ");
        }

        if (operation->argument_1 != NULL) {
            if (UNIT_FAILED(print_machine_item(stream,
                                               operation->argument_1,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }
        }

        if (operation->argument_2 != NULL) {
            assert(operation->argument_1 != NULL ||
                   !_UNIT_MachineDestination_IsNull(operation->destination));
            PRINT(", ");
            if (UNIT_FAILED(print_machine_item(stream,
                                               operation->argument_2,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }
        }

        PRINT(")\n");
    }

#undef PRINT

    return _UNIT_OK;
}

UNIT_Status
_UNIT_Translation_PrintInstructions(const _UNIT_Translation *translation,
                                    const char *name,
                                    FILE *stream)
{
#define PRINT(...)                                                          \
        if (fprintf(stream, __VA_ARGS__) < 0) {                             \
            _UNIT_SetOSError(translation->context, "printing translation"); \
            return _UNIT_FAIL;                                              \
        }

    assert(translation != NULL);
    assert(name != NULL);

    PRINT("translation for \"%s\":\n", name);
    UNIT_Size size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks,
                                                   index);
        assert(block != NULL);
        PRINT("    block %lld", (long long)block->id);
        if (_UNIT_Vector_SIZE(&block->instructions) > 0) {
            _UNIT_MachineOperation *first = _UNIT_Vector_GET(&block->instructions, 0);
            if (first->instruction == _UNIT_I_JUMP_LABEL) {
                _UNIT_MachineItem *label = _UNIT_MachineDestination_GetPointer(first->destination);
                PRINT(", label %s (%lld):", label->hint, (long long)label->value);
            }
        }

        PRINT("\n");
        if (UNIT_FAILED(print_instruction_stream(stream, &block->phis))) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(print_instruction_stream(stream,
                                                 &block->instructions))) {
            return _UNIT_FAIL;
        }
    }

#undef PRINT
    return _UNIT_OK;
}

_UNIT_MachineItem *
_UNIT_Translation_NewItem(_UNIT_Translation *translation,
                          _UNIT_MachineItem_Type type,
                          int64_t value,
                          const char *hint)
{
    _UNIT_MachineItem *item = _UNIT_Calloc(translation->context, 1, sizeof(*item));
    if (item == NULL) {
        return NULL;
    }

    item->type = type;
    item->value = value;
    if (hint != NULL) {
        item->hint = _UNIT_StrDup(translation->context, hint);
        if (item->hint == NULL) {
            _UNIT_Dealloc(translation->context, item);
            return NULL;
        }
    }

    item->next = translation->item_list_head;
    translation->item_list_head = item;
    return item;
}

static UNIT_Status
append_operation(_UNIT_Vector *instructions,
                 _UNIT_MachineInstruction instruction,
                 _UNIT_MachineDestination destination,
                 _UNIT_MachineItem *argument_1,
                 _UNIT_MachineItem *argument_2)
{
    _UNIT_MachineOperation *operation = _UNIT_Alloc(instructions->context, sizeof(*operation));
    if (operation == NULL) {
        return _UNIT_FAIL;
    }

    *operation = (_UNIT_MachineOperation) {instruction, destination, argument_1, argument_2};
    return _UNIT_Vector_Append(instructions, operation);
}

UNIT_Status
_UNIT_Translation_Emit(_UNIT_BasicBlock *block,
                       _UNIT_MachineInstruction instruction,
                       _UNIT_MachineDestination destination,
                       _UNIT_MachineItem *argument_1,
                       _UNIT_MachineItem *argument_2)
{
    return append_operation(&block->instructions, instruction, destination, argument_1, argument_2);
}

// Blocks are translated once, in reachable order. Entry values are placeholders
// until every predecessor (including loop backedges) has been translated.
typedef struct {
    _UNIT_BasicBlock *block;
    UNIT_Size start;
    UNIT_Size end;
    _UNIT_Vector entry_stack;
    _UNIT_Vector stack;
    _UNIT_MachineItem **definitions;
    int8_t reachable;
    int8_t queued;
} BlockState;

typedef struct {
    BlockState *state;
    UNIT_Size local;
    _UNIT_MachineOperation *phi;
} PendingLocal;

typedef struct {
    const UNIT_Procedure *procedure;
    _UNIT_Translation *translation;
    _UNIT_Vector states;
    _UNIT_Vector pending_locals;
    _UNIT_SizeMap labels;
    _UNIT_SizeMap locals;
    UNIT_Size *memory_slots;
} Builder;

static void
free_block_state(UNIT_Context *context, void *ptr)
{
    BlockState *state = ptr;
    _UNIT_Vector_Clear(&state->entry_stack);
    _UNIT_Vector_Clear(&state->stack);
    if (state->definitions != NULL) {
        _UNIT_Dealloc(context, state->definitions);
    }

    _UNIT_Dealloc(context, state);
}

static UNIT_Status
invalid(Builder *builder, const char *message)
{
    _UNIT_SetError(builder->translation->context, UNIT_ERROR_INVALID_USAGE, message);
    return _UNIT_FAIL;
}

static int8_t
is_terminator(UNIT_OperationCode instruction)
{
    return instruction == UNIT_OP_JUMP || instruction == UNIT_OP_JUMP_IF_TRUE
           || instruction == UNIT_OP_JUMP_IF_FALSE || instruction == UNIT_OP_RETURN_VALUE
           || instruction == UNIT_OP_EXIT;
}

static UNIT_Status
create_block(Builder *builder, UNIT_Size start)
{
    _UNIT_Translation *translation = builder->translation;
    UNIT_Context *context = translation->context;
    _UNIT_BasicBlock *block = _UNIT_BasicBlock_New(context, _UNIT_Vector_SIZE(&builder->states));
    if (block == NULL || UNIT_FAILED(_UNIT_Vector_Append(&translation->blocks, block))) {
        return _UNIT_FAIL;
    }

    block->label_id = block->id;
    BlockState *state = _UNIT_Calloc(context, 1, sizeof(*state));
    if (state == NULL) {
        return _UNIT_FAIL;
    }

    state->block = block;
    state->start = start;
    state->end = _UNIT_Vector_SIZE(&builder->procedure->_instructions);
    if (UNIT_FAILED(_UNIT_Vector_Init(&state->entry_stack, context, 8, NULL))) {
        _UNIT_Dealloc(context, state);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&state->stack, context, 8, NULL))) {
        _UNIT_Vector_Clear(&state->entry_stack);
        _UNIT_Dealloc(context, state);
        return _UNIT_FAIL;
    }

    return _UNIT_Vector_Append(&builder->states, state);
}

static BlockState *
jump_state(Builder *builder, UNIT_Size label)
{
    UNIT_Size id;
    if (UNIT_FAILED(_UNIT_SizeMap_Get(&builder->labels, label, &id))) {
        invalid(builder, "jump target has no label definition");
        return NULL;
    }

    return _UNIT_Vector_GET(&builder->states, id);
}

static UNIT_Status
build_cfg(Builder *builder)
{
    const UNIT_Procedure *procedure = builder->procedure;
    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);
    if (UNIT_FAILED(create_block(builder, 0))) {
        return _UNIT_FAIL;
    }

    BlockState *preheader = _UNIT_Vector_GET(&builder->states, 0);
    preheader->end = 0;
    if (UNIT_FAILED(create_block(builder, 0))) {
        return _UNIT_FAIL;
    }

    int8_t terminated = 0;
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_GET(&procedure->_instructions, index);
        if (index > 0 && (terminated || op->instruction == _UNIT_OP_JUMP_MARKER)) {
            BlockState *previous = _UNIT_Vector_GET(&builder->states,
                                                    _UNIT_Vector_SIZE(&builder->states) - 1);
            previous->end = index;
            if (UNIT_FAILED(create_block(builder, index))) {
                return _UNIT_FAIL;
            }
        }

        if (op->instruction == _UNIT_OP_JUMP_MARKER) {
            UNIT_Size existing;
            if (!_UNIT_Vector_INDEX_IS_VALID(&procedure->_jump_labels, op->argument)) {
                return invalid(builder, "invalid jump label");
            }

            if (!UNIT_FAILED(_UNIT_SizeMap_Get(&builder->labels, op->argument, &existing))) {
                return invalid(builder, "jump label defined more than once");
            }

            if (UNIT_FAILED(_UNIT_SizeMap_Set(&builder->labels,
                                              op->argument,
                                              _UNIT_Vector_SIZE(&builder->states) - 1))) {
                return _UNIT_FAIL;
            }
        }

        switch (op->instruction) {
            case UNIT_OP_LOAD_LOCAL:
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME:
            case _UNIT_OP_STORE_LOCAL_NAME:
            case UNIT_OP_ADDRESS_OF: {
                UNIT_Size local;
                if (UNIT_FAILED(_UNIT_SizeMap_Get(&builder->locals, op->argument, &local))
                    && UNIT_FAILED(_UNIT_SizeMap_Set(&builder->locals,
                                                     op->argument,
                                                     builder->locals.len))) {
                    return _UNIT_FAIL;
                }

                break;
            }
            default: {
                break;
            }
        }
        terminated = is_terminator(op->instruction);
    }

    UNIT_Size local_count = builder->locals.len;
    builder->memory_slots = _UNIT_Alloc(procedure->context,
                                        sizeof(UNIT_Size) * (local_count ? local_count : 1));
    if (builder->memory_slots == NULL) {
        return _UNIT_FAIL;
    }

    for (UNIT_Size local = 0; local < local_count; ++local) {
        builder->memory_slots[local] = -1;
    }

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_GET(&procedure->_instructions, index);
        if (op->instruction == UNIT_OP_ADDRESS_OF) {
            UNIT_Size local = _UNIT_SizeMap_GET(&builder->locals, op->argument);
            if (builder->memory_slots[local] == -1) {
                builder->memory_slots[local] = builder->translation->num_memory_slots++;
            }
        }
    }

    UNIT_Size count = _UNIT_Vector_SIZE(&builder->states);
    for (UNIT_Size index = 0; index < count; ++index) {
        BlockState *state = _UNIT_Vector_GET(&builder->states, index);
        state->definitions = _UNIT_Calloc(procedure->context,
                                          local_count ? local_count : 1,
                                          sizeof(*state->definitions));
        if (state->definitions == NULL) {
            return _UNIT_FAIL;
        }

        if (state->end == state->start) {
            if (index + 1 < count) {
                BlockState *next = _UNIT_Vector_GET(&builder->states, index + 1);
                if (UNIT_FAILED(_UNIT_BasicBlock_AddSuccessor(state->block, next->block))) {
                    return _UNIT_FAIL;
                }
            }

            continue;
        }

        _UNIT_Operation *last = _UNIT_Vector_GET(&procedure->_instructions, state->end - 1);
        if (last->instruction == UNIT_OP_JUMP || last->instruction == UNIT_OP_JUMP_IF_TRUE
            || last->instruction == UNIT_OP_JUMP_IF_FALSE) {
            BlockState *target = jump_state(builder, last->argument);
            if (target == NULL || UNIT_FAILED(_UNIT_BasicBlock_AddSuccessor(state->block,
                                                                            target->block))) {
                return _UNIT_FAIL;
            }
        }

        if (index + 1 < count && last->instruction != UNIT_OP_JUMP
            && last->instruction != UNIT_OP_RETURN_VALUE && last->instruction != UNIT_OP_EXIT) {
            BlockState *next = _UNIT_Vector_GET(&builder->states, index + 1);
            if (UNIT_FAILED(_UNIT_BasicBlock_AddSuccessor(state->block, next->block))) {
                return _UNIT_FAIL;
            }
        }
    }

    // Reachability is independent of layout; dead instructions must not supply
    // PHI operands or participate in definite-assignment checks.
    BlockState *entry = _UNIT_Vector_GET(&builder->states, 0);
    entry->reachable = 1;
    int8_t changed;
    do {
        changed = 0;
        for (UNIT_Size index = 0; index < count; ++index) {
            BlockState *state = _UNIT_Vector_GET(&builder->states, index);
            if (!state->reachable) {
                continue;
            }

            for (UNIT_Size edge = 0; edge < _UNIT_Vector_SIZE(&state->block->successors); ++edge) {
                _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&state->block->successors, edge);
                BlockState *next = _UNIT_Vector_GET(&builder->states, successor->id);
                if (!next->reachable) {
                    next->reachable = 1;
                    changed = 1;
                }
            }
        }
    } while (changed);
    for (UNIT_Size index = 0; index < count; ++index) {
        BlockState *state = _UNIT_Vector_GET(&builder->states, index);
        _UNIT_Vector *predecessors = &state->block->predecessors;
        UNIT_Size length = 0;
        for (UNIT_Size edge = 0; edge < _UNIT_Vector_SIZE(predecessors); ++edge) {
            _UNIT_BasicBlock *pred = _UNIT_Vector_GET(predecessors, edge);
            BlockState *previous = _UNIT_Vector_GET(&builder->states, pred->id);
            if (previous->reachable) {
                predecessors->items[length++] = pred;
            }
        }

        predecessors->length = length;
    }

    return _UNIT_OK;
}

static _UNIT_MachineItem *
new_location(Builder *builder)
{
    return _UNIT_Translation_NewItem(builder->translation,
                                     _UNIT_TYPE_LOCATION,
                                     builder->translation->num_locations++,
                                     NULL);
}

static _UNIT_MachineOperation *
new_phi(Builder *builder, BlockState *state)
{
    _UNIT_MachineItem *destination = new_location(builder);
    _UNIT_MachineItem *arguments = _UNIT_Translation_NewItem(builder->translation,
                                                             _UNIT_TYPE_CONSTANT,
                                                             0,
                                                             NULL);
    if (destination == NULL || arguments == NULL) {
        return NULL;
    }

    arguments->phi_args = _UNIT_Vector_New(builder->translation->context,
                                           _UNIT_Vector_SIZE(&state->block->predecessors),
                                           _UNIT_Dealloc);
    if (arguments->phi_args == NULL) {
        // Leave a scalar item so translation cleanup does not dereference NULL.
        arguments->type = _UNIT_TYPE_CONSTANT;
        return NULL;
    }

    arguments->type = _UNIT_TYPE_PHI_ARGS;
    if (UNIT_FAILED(append_operation(&state->block->phis,
                                     _UNIT_I_PHI,
                                     _UNIT_MachineDestination_FromDestination(destination),
                                     arguments,
                                     NULL))) {
        return NULL;
    }

    return _UNIT_Vector_GET(&state->block->phis, _UNIT_Vector_SIZE(&state->block->phis) - 1);
}

static UNIT_Status
add_phi_input(_UNIT_MachineOperation *phi,
              _UNIT_BasicBlock *predecessor,
              _UNIT_MachineItem *value)
{
    _UNIT_Vector *inputs = phi->argument_1->phi_args;
    _UNIT_PhiInput *input = _UNIT_Alloc(inputs->context, sizeof(*input));
    if (input == NULL) {
        return _UNIT_FAIL;
    }

    *input = (_UNIT_PhiInput) {predecessor, value};
    return _UNIT_Vector_Append(inputs, input);
}

static _UNIT_MachineItem *
read_local(Builder *builder, BlockState *state, UNIT_Size local)
{
    if (state->definitions[local] != NULL) {
        return state->definitions[local];
    }

    if (_UNIT_Vector_SIZE(&state->block->predecessors) == 0) {
        invalid(builder, "local variable not assigned on every incoming path");
        return NULL;
    }

    _UNIT_MachineOperation *phi = new_phi(builder, state);
    if (phi == NULL) {
        return NULL;
    }

    PendingLocal *pending = _UNIT_Alloc(builder->translation->context, sizeof(*pending));
    if (pending == NULL) {
        return NULL;
    }

    *pending = (PendingLocal) {state, local, phi};
    if (UNIT_FAILED(_UNIT_Vector_Append(&builder->pending_locals, pending))) {
        return NULL;
    }

    state->definitions[local] = _UNIT_MachineDestination_GetPointer(phi->destination);
    return state->definitions[local];
}

// Comparisons and prepared calls are compile-time aggregates. Merge their
// constituent values, retaining the aggregate's shape for subsequent consumers.
static _UNIT_MachineItem *
make_stack_entry(Builder *builder, BlockState *state, _UNIT_MachineItem *source)
{
    if (source->type != _UNIT_TYPE_CALL_ARGS && source->type != _UNIT_TYPE_COMPARISON) {
        _UNIT_MachineOperation *phi = new_phi(builder, state);
        return phi == NULL ? NULL : _UNIT_MachineDestination_GetPointer(phi->destination);
    }

    _UNIT_MachineItem *item = _UNIT_Translation_NewItem(builder->translation,
                                                        source->type,
                                                        0,
                                                        NULL);
    if (item == NULL) {
        return NULL;
    }

    if (source->type == _UNIT_TYPE_COMPARISON) {
        item->comparison.type = source->comparison.type;
        item->comparison.left = make_stack_entry(builder, state, source->comparison.left);
        item->comparison.right = make_stack_entry(builder, state, source->comparison.right);
        if (item->comparison.left == NULL || item->comparison.right == NULL) {
            return NULL;
        }
    } else {
        UNIT_Size size = _UNIT_Vector_SIZE(source->call_args);
        item->call_args = _UNIT_Vector_New(builder->translation->context, size, NULL);
        if (item->call_args == NULL) {
            item->type = _UNIT_TYPE_CONSTANT;
            return NULL;
        }

        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *arg = make_stack_entry(builder,
                                                      state,
                                                      _UNIT_Vector_GET(source->call_args, index));
            if (arg == NULL) {
                return NULL;
            }

            _UNIT_Vector_APPEND(item->call_args, arg);
        }
    }

    return item;
}

static UNIT_Status
merge_stack_entry(Builder *builder,
                  BlockState *state,
                  _UNIT_BasicBlock *predecessor,
                  _UNIT_MachineItem *entry,
                  _UNIT_MachineItem *source)
{
    if (entry->type == _UNIT_TYPE_COMPARISON) {
        if (source->type != _UNIT_TYPE_COMPARISON
            || entry->comparison.type != source->comparison.type) {
            return invalid(builder, "incompatible comparison values at control-flow merge");
        }

        if (UNIT_FAILED(merge_stack_entry(builder,
                                          state,
                                          predecessor,
                                          entry->comparison.left,
                                          source->comparison.left))) {
            return _UNIT_FAIL;
        }

        return merge_stack_entry(builder,
                                 state,
                                 predecessor,
                                 entry->comparison.right,
                                 source->comparison.right);
    }

    if (entry->type == _UNIT_TYPE_CALL_ARGS) {
        if (source->type != _UNIT_TYPE_CALL_ARGS
            || _UNIT_Vector_SIZE(entry->call_args) != _UNIT_Vector_SIZE(source->call_args)) {
            return invalid(builder, "incompatible call arguments at control-flow merge");
        }

        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(entry->call_args); ++index) {
            if (UNIT_FAILED(merge_stack_entry(builder,
                                              state,
                                              predecessor,
                                              _UNIT_Vector_GET(entry->call_args, index),
                                              _UNIT_Vector_GET(source->call_args, index)))) {
                return _UNIT_FAIL;
            }
        }

        return _UNIT_OK;
    }

    if (source->type == _UNIT_TYPE_COMPARISON || source->type == _UNIT_TYPE_CALL_ARGS) {
        return invalid(builder, "incompatible stack values at control-flow merge");
    }

    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&state->block->phis); ++index) {
        _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&state->block->phis, index);
        if (_UNIT_MachineDestination_GetPointer(phi->destination) == entry) {
            return add_phi_input(phi, predecessor, source);
        }
    }

    _UNIT_Unreachable();
}

static _UNIT_MachineItem *
pop_stack(Builder *builder, BlockState *state, UNIT_OperationCode instruction)
{
    if (_UNIT_Vector_SIZE(&state->stack) == 0) {
        _UNIT_SetErrorFormat(builder->translation->context,
                             UNIT_ERROR_INVALID_USAGE,
                             "stack underflow at %s",
                             UNIT_OperationCode_GetName(instruction));
        return NULL;
    }

    return _UNIT_Vector_Pop(&state->stack);
}

static UNIT_Status
translate_block(Builder *builder, BlockState *state)
{
    _UNIT_Translation *translation = builder->translation;
    const UNIT_Procedure *procedure = builder->procedure;
    _UNIT_BasicBlock *block = state->block;
    _UNIT_Vector *stack = &state->stack;

#define CHECK(expr)                   \
        do { if (UNIT_FAILED(expr)) { \
                 return _UNIT_FAIL;   \
             }                        \
        } while (0)
#define ITEM(name, type, value, hint)                                                        \
        _UNIT_MachineItem *name = _UNIT_Translation_NewItem(translation, type, value, hint); \
        if (name == NULL) {                                                                  \
            return _UNIT_FAIL;                                                               \
        }

#define POP(name)                                                             \
        _UNIT_MachineItem *name = pop_stack(builder, state, op->instruction); \
        if (name == NULL) {                                                   \
            return _UNIT_FAIL;                                                \
        }

#define EMIT(inst, dest, a, b) CHECK(_UNIT_Translation_Emit(block, inst, dest, a, b))
#define RESULT(inst, a, b)                                                      \
        do {                                                                    \
            _UNIT_MachineItem *result = new_location(builder);                  \
            if (result == NULL) {                                               \
                return _UNIT_FAIL;                                              \
            }                                                                   \
            EMIT(inst, _UNIT_MachineDestination_FromDestination(result), a, b); \
            CHECK(_UNIT_Vector_Append(stack, result));                          \
        } while (0)

    const char *name = "block";
    if (state->start < state->end) {
        _UNIT_Operation *first = _UNIT_Vector_GET(&procedure->_instructions, state->start);
        if (first->instruction == _UNIT_OP_JUMP_MARKER) {
            UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels, first->argument);
            name = label->name;
        }
    }

    ITEM(label, _UNIT_TYPE_CONSTANT, block->label_id, name);
    EMIT(_UNIT_I_JUMP_LABEL, _UNIT_MachineDestination_FromDestination(label), NULL, NULL);
    for (UNIT_Size index = state->start; index < state->end; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_GET(&procedure->_instructions, index);
        switch (op->instruction) {
            case _UNIT_OP_JUMP_MARKER: {
                break;
            }
            case UNIT_OP_LOAD_INTEGER: {
                ITEM(value, _UNIT_TYPE_CONSTANT, op->argument, NULL);
                CHECK(_UNIT_Vector_Append(stack, value));
                break;
            }
            case UNIT_OP_LOAD_STRING: {
                if (!_UNIT_Vector_INDEX_IS_VALID(&procedure->_global_strings, op->argument)) {
                    return invalid(builder, "invalid string ID");
                }

                ITEM(value,
                     _UNIT_TYPE_CONSTANT,
                     op->argument,
                     _UNIT_Vector_GET(&procedure->_global_strings, op->argument));
                RESULT(_UNIT_I_LOAD_STRING, value, NULL);
                break;
            }
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME: {
                POP(value);
                UNIT_Size local = _UNIT_SizeMap_GET(&builder->locals, op->argument);
                _UNIT_MachineItem *result = new_location(builder);
                if (result == NULL) {
                    return _UNIT_FAIL;
                }

                EMIT(_UNIT_I_LOAD, _UNIT_MachineDestination_FromDestination(result), value, NULL);
                state->definitions[local] = result;
                if (builder->memory_slots[local] != -1) {
                    ITEM(slot, _UNIT_TYPE_MEMORY, builder->memory_slots[local], NULL);
                    EMIT(_UNIT_I_LOAD,
                         _UNIT_MachineDestination_FromDestination(slot),
                         result,
                         NULL);
                }

                break;
            }
            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME:
            case UNIT_OP_ADDRESS_OF: {
                UNIT_Size local = _UNIT_SizeMap_GET(&builder->locals, op->argument);
                if (builder->memory_slots[local] != -1) {
                    // Escaped locals can be initialized through their address
                    // (for example by scanf); their contents are not SSA values.
                    ITEM(slot, _UNIT_TYPE_MEMORY, builder->memory_slots[local], NULL);
                    RESULT(op->instruction ==
                           UNIT_OP_ADDRESS_OF ? _UNIT_I_ADDRESS_OF : _UNIT_I_LOAD,
                           slot,
                           NULL);
                } else {
                    _UNIT_MachineItem *value = read_local(builder, state, local);
                    if (value == NULL) {
                        return _UNIT_FAIL;
                    }

                    CHECK(_UNIT_Vector_Append(stack, value));
                }

                break;
            }
#define BINARY(opcode, inst)                                             \
        case opcode: {                                                   \
                POP(right); POP(left); RESULT(inst, left, right); break; \
        }

                BINARY(UNIT_OP_ADD, _UNIT_I_ADD)
                BINARY(UNIT_OP_SUBTRACT, _UNIT_I_SUB)
                BINARY(UNIT_OP_MULTIPLY, _UNIT_I_MUL)
                BINARY(UNIT_OP_DIVIDE, _UNIT_I_DIV)
                BINARY(UNIT_OP_MODULO, _UNIT_I_MOD)
#undef BINARY
            case UNIT_OP_JUMP: {
                BlockState *target = jump_state(builder, op->argument);
                if (target == NULL) {
                    return _UNIT_FAIL;
                }

                ITEM(target_item, _UNIT_TYPE_CONSTANT, target->block->label_id, NULL);
                EMIT(_UNIT_I_JUMP, _UNIT_MachineDestination_NULL, target_item, NULL);
                break;
            }
            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                if (index + 1 == _UNIT_Vector_SIZE(&procedure->_instructions)) {
                    return invalid(builder, "conditional jump has no fallthrough block");
                }

                POP(value);
                if (value->type != _UNIT_TYPE_COMPARISON) {
                    return invalid(builder, "JUMP_IF_FALSE/JUMP_IF_TRUE got non-comparison");
                }

                BlockState *target = jump_state(builder, op->argument);
                if (target == NULL) {
                    return _UNIT_FAIL;
                }

                ITEM(target_item, _UNIT_TYPE_CONSTANT, target->block->label_id, NULL);
                int8_t invert = op->instruction == UNIT_OP_JUMP_IF_FALSE;
                _UNIT_MachineInstruction instruction;
                switch (value->comparison.type) {
                    case UNIT_OP_COMPARE_EQUAL: {
                        instruction = invert ? _UNIT_I_JUMP_IF_NOT_EQUAL : _UNIT_I_JUMP_IF_EQUAL;
                        break;
                    }
                    case UNIT_OP_COMPARE_NOT_EQUAL: {
                        instruction = invert ? _UNIT_I_JUMP_IF_EQUAL : _UNIT_I_JUMP_IF_NOT_EQUAL;
                        break;
                    }
                    case UNIT_OP_COMPARE_LESS: {
                        instruction = invert ? _UNIT_I_JUMP_IF_GREATER_EQUAL : _UNIT_I_JUMP_IF_LESS;
                        break;
                    }
                    case UNIT_OP_COMPARE_LESS_EQUAL: {
                        instruction = invert ? _UNIT_I_JUMP_IF_GREATER : _UNIT_I_JUMP_IF_LESS_EQUAL;
                        break;
                    }
                    case UNIT_OP_COMPARE_GREATER: {
                        instruction = invert ? _UNIT_I_JUMP_IF_LESS_EQUAL : _UNIT_I_JUMP_IF_GREATER;
                        break;
                    }
                    case UNIT_OP_COMPARE_GREATER_EQUAL: {
                        instruction = invert ? _UNIT_I_JUMP_IF_LESS : _UNIT_I_JUMP_IF_GREATER_EQUAL;
                        break;
                    }
                    default: {
                        _UNIT_Unreachable();
                    }
                }
                EMIT(instruction,
                     _UNIT_MachineDestination_FromInput(target_item),
                     value->comparison.left,
                     value->comparison.right);
                break;
            }
            case UNIT_OP_RETURN_VALUE:
            case UNIT_OP_EXIT: {
                POP(value);
                EMIT(op->instruction == UNIT_OP_EXIT ? _UNIT_I_EXIT : _UNIT_I_RETURN_VALUE,
                     _UNIT_MachineDestination_NULL,
                     value,
                     NULL);
                // An exit discards the remainder of this path's operand stack.
                stack->length = 0;
                break;
            }
            case UNIT_OP_LOAD_ARGUMENT: {
                ITEM(argument, _UNIT_TYPE_CONSTANT, op->argument, NULL);
                RESULT(_UNIT_I_LOAD_ARGUMENT, argument, NULL);
                break;
            }
            case UNIT_OP_PREPARE_CALL: {
                if (op->argument < 0 || op->argument > _UNIT_Vector_SIZE(stack)) {
                    return invalid(builder, "invalid call argument count");
                }

                ITEM(arguments, _UNIT_TYPE_CONSTANT, 0, NULL);
                arguments->call_args = _UNIT_Vector_New(translation->context, op->argument, NULL);
                if (arguments->call_args == NULL) {
                    return _UNIT_FAIL;
                }

                arguments->type = _UNIT_TYPE_CALL_ARGS;
                for (UNIT_Size arg = 0; arg < op->argument; ++arg) {
                    _UNIT_Vector_APPEND(arguments->call_args, _UNIT_Vector_Pop(stack));
                }

                _UNIT_Vector_Reverse(arguments->call_args);
                CHECK(_UNIT_Vector_Append(stack, arguments));
                break;
            }
            case UNIT_OP_CALL_NAME:
            case UNIT_OP_CALL_PROCEDURE: {
                const char *symbol_name;
                if (op->instruction == UNIT_OP_CALL_NAME) {
                    if (!_UNIT_Vector_INDEX_IS_VALID(&procedure->_symbols, op->argument)) {
                        return invalid(builder, "invalid call symbol");
                    }

                    symbol_name = _UNIT_Vector_GET(&procedure->_symbols, op->argument);
                } else {
                    if (!_UNIT_Vector_INDEX_IS_VALID(&procedure->_subprocedures, op->argument)) {
                        return invalid(builder, "invalid subprocedure");
                    }

                    UNIT_Procedure *callee = _UNIT_Vector_GET(&procedure->_subprocedures,
                                                              op->argument);
                    symbol_name = callee->name;
                }

                ITEM(symbol, _UNIT_TYPE_CONSTANT, op->argument, symbol_name);
                POP(arguments);
                if (arguments->type != _UNIT_TYPE_CALL_ARGS) {
                    return invalid(builder, "call requires prepared arguments");
                }

                RESULT(_UNIT_I_CALL_SYMBOL, symbol, arguments);
                break;
            }
            case UNIT_OP_COMPARE_EQUAL:
            case UNIT_OP_COMPARE_NOT_EQUAL:
            case UNIT_OP_COMPARE_LESS:
            case UNIT_OP_COMPARE_LESS_EQUAL:
            case UNIT_OP_COMPARE_GREATER:
            case UNIT_OP_COMPARE_GREATER_EQUAL: {
                POP(right);
                POP(left);
                ITEM(comparison, _UNIT_TYPE_COMPARISON, 0, NULL);
                comparison->comparison.type = op->instruction;
                comparison->comparison.left = left;
                comparison->comparison.right = right;
                CHECK(_UNIT_Vector_Append(stack, comparison));
                break;
            }
            case UNIT_OP_COPY: {
                UNIT_Size depth = _UNIT_Vector_SIZE(stack);
                if (op->argument < 0 || op->argument >= depth) {
                    return invalid(builder, "invalid COPY depth");
                }

                CHECK(_UNIT_Vector_Append(stack,
                                          _UNIT_Vector_GET(stack, depth - op->argument - 1)));
                break;
            }
            case UNIT_OP_SWAP: {
                UNIT_Size top = _UNIT_Vector_SIZE(stack) - 1;
                if (op->argument <= 0 || op->argument > top) {
                    return invalid(builder, "invalid SWAP depth");
                }

                void *saved = stack->items[top];
                stack->items[top] = stack->items[top - op->argument];
                stack->items[top - op->argument] = saved;
                break;
            }
            case UNIT_OP_POP: {
                POP(unused);
                (void)unused;
                break;
            }
            case UNIT_OP_READ_BYTES:
            case UNIT_OP_WRITE_BYTES: {
                if (op->argument != 1 && op->argument != 2 && op->argument != 4 &&
                    op->argument != 8) {
                    return invalid(builder, "memory access must use 1, 2, 4 or 8 bytes");
                }

                ITEM(width, _UNIT_TYPE_CONSTANT, op->argument, NULL);
                if (op->instruction == UNIT_OP_READ_BYTES) {
                    POP(address);
                    RESULT(_UNIT_I_READ_BYTES, address, width);
                } else {
                    POP(value);
                    POP(address);
                    EMIT(_UNIT_I_WRITE_BYTES,
                         _UNIT_MachineDestination_FromInput(address),
                         value,
                         width);
                }

                break;
            }
            case UNIT_OP_CONVERT: {
                if (op->argument < UNIT_TYPE_INT8 || op->argument > UNIT_TYPE_UINT64) {
                    return invalid(builder, "invalid integer conversion type");
                }

                POP(value);
                ITEM(type, _UNIT_TYPE_CONSTANT, op->argument, integer_type_name(op->argument));
                RESULT(_UNIT_I_CONVERT, value, type);
                break;
            }
        }
    }
    if (_UNIT_Vector_SIZE(&block->successors) == 0 && _UNIT_Vector_SIZE(stack) != 0) {
        return invalid(builder, "procedure does not consume entire stack");
    }

#undef CHECK
#undef ITEM
#undef POP
#undef EMIT
#undef RESULT
    return _UNIT_OK;
}

static void
replace_item(_UNIT_MachineItem **item, _UNIT_MachineItem *from, _UNIT_MachineItem *to)
{
    if (*item == from) {
        *item = to;
    } else if (*item != NULL && (*item)->type == _UNIT_TYPE_CALL_ARGS) {
        _UNIT_Vector *args = (*item)->call_args;
        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(args); ++index) {
            _UNIT_MachineItem *arg = _UNIT_Vector_GET(args, index);
            replace_item(&arg, from, to);
            args->items[index] = arg;
        }
    } else if (*item != NULL && (*item)->type == _UNIT_TYPE_PHI_ARGS) {
        _UNIT_Vector *args = (*item)->phi_args;
        for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(args); ++index) {
            _UNIT_PhiInput *arg = _UNIT_Vector_GET(args, index);
            replace_item(&arg->value, from, to);
        }
    }
}

static void
replace_uses(_UNIT_Translation *translation, _UNIT_MachineItem *from, _UNIT_MachineItem *to)
{
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&translation->blocks); ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        _UNIT_Vector *vectors[] = {&block->phis, &block->instructions};
        for (UNIT_Size v = 0; v < 2; ++v) {
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(vectors[v]); ++i) {
                _UNIT_MachineOperation *op = _UNIT_Vector_GET(vectors[v], i);
                replace_item(&op->argument_1, from, to);
                replace_item(&op->argument_2, from, to);
                if (_UNIT_MachineDestination_IsInput(op->destination)) {
                    _UNIT_MachineItem *input = _UNIT_MachineDestination_GetPointer(op->destination);
                    replace_item(&input, from, to);
                    op->destination = _UNIT_MachineDestination_FromInput(input);
                }
            }
        }
    }
}

static void
simplify_phis(_UNIT_Translation *translation)
{
    int8_t changed;
    do {
        changed = 0;
        for (UNIT_Size b = 0; b < _UNIT_Vector_SIZE(&translation->blocks); ++b) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&block->phis);) {
                _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&block->phis, i);
                _UNIT_MachineItem *destination =
                    _UNIT_MachineDestination_GetPointer(phi->destination);
                _UNIT_MachineItem *same = NULL;
                int8_t trivial = 1;
                _UNIT_Vector *inputs = phi->argument_1->phi_args;
                for (UNIT_Size j = 0; j < _UNIT_Vector_SIZE(inputs); ++j) {
                    _UNIT_PhiInput *input = _UNIT_Vector_GET(inputs, j);
                    if (input->value == destination) {
                        continue;
                    }

                    if (same != NULL && same != input->value
                        && !(same->type == _UNIT_TYPE_CONSTANT
                             && input->value->type == _UNIT_TYPE_CONSTANT
                             && same->value == input->value->value)) {
                        trivial = 0;
                        break;
                    }

                    same = input->value;
                }

                if (!trivial || same == NULL) {
                    ++i;
                    continue;
                }

                replace_uses(translation, destination, same);
                _UNIT_Dealloc(translation->context, phi);
                for (UNIT_Size j = i + 1; j < _UNIT_Vector_SIZE(&block->phis); ++j) {
                    block->phis.items[j - 1] = block->phis.items[j];
                }

                --block->phis.length;
                changed = 1;
            }
        }
    } while (changed);
}

static UNIT_Status
translate_cfg(Builder *builder)
{
    _UNIT_Vector queue;
    if (UNIT_FAILED(_UNIT_Vector_Init(&queue,
                                      builder->translation->context,
                                      _UNIT_Vector_SIZE(&builder->states),
                                      NULL))) {
        return _UNIT_FAIL;
    }

    BlockState *entry = _UNIT_Vector_GET(&builder->states, 0);
    entry->queued = 1;
    _UNIT_Vector_APPEND(&queue, entry);
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&queue); ++index) {
        BlockState *state = _UNIT_Vector_GET(&queue, index);
        if (UNIT_FAILED(translate_block(builder, state))) {
            goto error;
        }

        for (UNIT_Size edge = 0; edge < _UNIT_Vector_SIZE(&state->block->successors); ++edge) {
            _UNIT_BasicBlock *successor = _UNIT_Vector_GET(&state->block->successors, edge);
            BlockState *next = _UNIT_Vector_GET(&builder->states, successor->id);
            if (next->queued) {
                continue;
            }

            next->queued = 1;
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&state->stack); ++i) {
                _UNIT_MachineItem *value = make_stack_entry(builder,
                                                            next,
                                                            _UNIT_Vector_GET(&state->stack, i));
                if (value == NULL || UNIT_FAILED(_UNIT_Vector_Append(&next->entry_stack, value))
                    || UNIT_FAILED(_UNIT_Vector_Append(&next->stack, value))) {
                    goto error;
                }
            }

            _UNIT_Vector_APPEND(&queue, next);
        }
    }

    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&queue); ++index) {
        BlockState *state = _UNIT_Vector_GET(&queue, index);
        for (UNIT_Size edge = 0; edge < _UNIT_Vector_SIZE(&state->block->predecessors); ++edge) {
            _UNIT_BasicBlock *pred = _UNIT_Vector_GET(&state->block->predecessors, edge);
            BlockState *previous = _UNIT_Vector_GET(&builder->states, pred->id);
            if (_UNIT_Vector_SIZE(&state->entry_stack) != _UNIT_Vector_SIZE(&previous->stack)) {
                invalid(builder, "inconsistent stack depth at control-flow merge");
                goto error;
            }

            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&state->entry_stack); ++i) {
                if (UNIT_FAILED(merge_stack_entry(builder,
                                                  state,
                                                  pred,
                                                  _UNIT_Vector_GET(&state->entry_stack, i),
                                                  _UNIT_Vector_GET(&previous->stack, i)))) {
                    goto error;
                }
            }
        }
    }

    // read_local can append additional pending PHIs. Caching each definition
    // before visiting predecessors makes this work for cyclic and irreducible CFGs.
    for (UNIT_Size index = 0; index < _UNIT_Vector_SIZE(&builder->pending_locals); ++index) {
        PendingLocal *pending = _UNIT_Vector_GET(&builder->pending_locals, index);
        _UNIT_Vector *predecessors = &pending->state->block->predecessors;
        for (UNIT_Size edge = 0; edge < _UNIT_Vector_SIZE(predecessors); ++edge) {
            _UNIT_BasicBlock *pred = _UNIT_Vector_GET(predecessors, edge);
            BlockState *previous = _UNIT_Vector_GET(&builder->states, pred->id);
            _UNIT_MachineItem *value = read_local(builder, previous, pending->local);
            if (value == NULL || UNIT_FAILED(add_phi_input(pending->phi, pred, value))) {
                goto error;
            }
        }
    }

    _UNIT_Vector_Clear(&queue);
    return _UNIT_OK;
error:
    _UNIT_Vector_Clear(&queue);
    return _UNIT_FAIL;
}

UNIT_Status
_UNIT_Translate(_UNIT_Translation *translation, const UNIT_Procedure *procedure)
{
    UNIT_Context *context = procedure->context;
    *translation = (_UNIT_Translation) {.context = context};
    if (UNIT_FAILED(_UNIT_Map_Init(&translation->strings,
                                   context,
                                   8,
                                   _UNIT_Map_CompareEqual,
                                   _UNIT_Map_HashDirect,
                                   NULL,
                                   _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&translation->blocks, context, 16, _UNIT_BasicBlock_Free))) {
        _UNIT_Map_Clear(&translation->strings);
        return _UNIT_FAIL;
    }

    Builder builder = {.procedure = procedure, .translation = translation};
    if (UNIT_FAILED(_UNIT_Vector_Init(&builder.states, context, 16, free_block_state))) {
        goto translation_error;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&builder.pending_locals, context, 16, _UNIT_Dealloc))) {
        goto states_error;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&builder.labels, context, 8))) {
        goto pending_error;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&builder.locals, context, 8))) {
        goto labels_error;
    }

    UNIT_Status status = build_cfg(&builder);
    if (!UNIT_FAILED(status)) {
        status = translate_cfg(&builder);
    }

    // Preserve source layout for fallthrough edges, while removing unreachable blocks.
    if (!UNIT_FAILED(status)) {
        UNIT_Size length = 0;
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&builder.states); ++i) {
            BlockState *state = _UNIT_Vector_GET(&builder.states, i);
            if (state->reachable) {
                translation->blocks.items[length++] = state->block;
            } else {
                _UNIT_BasicBlock_Free(context, state->block);
            }
        }

        translation->blocks.length = length;
        simplify_phis(translation);
        status = _UNIT_Translation_AnalyzeLiveness(translation);
    }

    if (builder.memory_slots != NULL) {
        _UNIT_Dealloc(context, builder.memory_slots);
    }

    _UNIT_SizeMap_Clear(&builder.locals);
    _UNIT_SizeMap_Clear(&builder.labels);
    _UNIT_Vector_Clear(&builder.pending_locals);
    _UNIT_Vector_Clear(&builder.states);
    if (UNIT_FAILED(status)) {
        _UNIT_Translation_Clear(translation);
    }

    return status;
labels_error:
    _UNIT_SizeMap_Clear(&builder.labels);
pending_error:
    _UNIT_Vector_Clear(&builder.pending_locals);
states_error:
    _UNIT_Vector_Clear(&builder.states);
translation_error:
    _UNIT_Translation_Clear(translation);
    return _UNIT_FAIL;
}

void
_UNIT_Translation_Clear(_UNIT_Translation *translation)
{
    _UNIT_Map_Clear(&translation->strings);
    _UNIT_Vector_Clear(&translation->blocks);
    _UNIT_MachineItem *item = translation->item_list_head;
    while (item != NULL) {
        _UNIT_MachineItem *next = item->next;
        if (item->type == _UNIT_TYPE_CALL_ARGS) {
            _UNIT_Vector_Free(item->call_args);
        } else if (item->type == _UNIT_TYPE_PHI_ARGS) {
            _UNIT_Vector_Free(item->phi_args);
        }

        if (item->hint != NULL) {
            _UNIT_Dealloc(translation->context, item->hint);
        }

        _UNIT_Dealloc(translation->context, item);
        item = next;
    }
}
