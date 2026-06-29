#include <stdio.h>

#include <unit/errors.h>
#include <unit/procedure.h>

#include <unit/internal/allocation.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/translation.h>

#define NAME(name) case _UNIT_I_ ##name: return #name

const char *
machine_instruction_name(_UNIT_MachineInstruction machine_instruction)
{
    switch (machine_instruction) {
        NAME(LOAD);
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
#define INT_TYPE(name) case UNIT_TYPE_ ##name: return #name
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

static UNIT_Status
mark_last_use(_UNIT_BasicBlock *block, _UNIT_MachineItem *item)
{
    assert(block != NULL);
    _UNIT_SizeMap *last_uses = &block->liveness.last_uses;
    UNIT_Size index = _UNIT_Vector_SIZE(&block->instructions) - 1;

    assert(last_uses != NULL);
    assert(item != NULL);
    assert(index >= 0);
    if (item->type == _UNIT_TYPE_LOCATION) {
        return _UNIT_SizeMap_Set(last_uses, item->value, index);
    } else if (item->type == _UNIT_TYPE_CALL_ARGS) {
        UNIT_Size count = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size i = 0; i < count; ++i) {
            if (UNIT_FAILED(mark_last_use(block, _UNIT_Vector_GET(item->call_args, i)))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

UNIT_Status
emit_machine_instruction(UNIT_Context *context,
                         _UNIT_BasicBlock *block,
                         _UNIT_MachineInstruction instruction,
                         _UNIT_MachineDestination destination,
                         _UNIT_MachineItem *arg1,
                         _UNIT_MachineItem *arg2)
{
    assert(context != NULL);
    assert(block != NULL);
    _UNIT_MachineOperation *operation = _UNIT_Alloc(context,
                                                    sizeof(_UNIT_MachineOperation));
    if (operation == NULL) {
        return _UNIT_FAIL;
    }

    operation->instruction = instruction;
    operation->destination = destination;
    operation->argument_1 = arg1;
    operation->argument_2 = arg2;

    if (UNIT_FAILED(_UNIT_Vector_Append(&block->instructions,
                                        operation))) {
        return _UNIT_FAIL;
    }

    if (!_UNIT_MachineDestination_IsNull(destination)
        && _UNIT_MachineDestination_IsInput(destination)) {
        _UNIT_MachineItem *unwrapped = _UNIT_MachineDestination_GetPointer(destination);
        if (UNIT_FAILED(mark_last_use(block, unwrapped))) {
            return _UNIT_FAIL;
        }
    }

    if (arg1 != NULL) {
        if (UNIT_FAILED(mark_last_use(block, arg1))) {
            return _UNIT_FAIL;
        }
    }

    if (arg2 != NULL) {
        if (UNIT_FAILED(mark_last_use(block, arg2))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}


UNIT_Status
print_machine_item(FILE *stream, _UNIT_MachineItem *item, UNIT_Context *context)
{
#define PRINT(...)                                                          \
    if (fprintf(stream, __VA_ARGS__) < 0) {                                 \
        _UNIT_SetOSError(context, "printing machine item");                 \
        return _UNIT_FAIL;                                                  \
    }

    assert(item != NULL);
    if (item->type == _UNIT_TYPE_CONSTANT) {
        PRINT("%ld", item->value);
    } else if (item->type == _UNIT_TYPE_LOCATION) {
        PRINT("location_%ld", item->value);
    } else if (item->type == _UNIT_TYPE_CALL_ARGS) {
        PRINT("[");
        UNIT_Size size = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(item->call_args, index);
            assert(arg_item != NULL);
            if (UNIT_FAILED(print_machine_item(stream, arg_item, context))) {
                return _UNIT_FAIL;
            }

            if (index + 1 != size) {
                PRINT(", ");
            }
        }
        PRINT("]");
    } else if (item->type == _UNIT_TYPE_COMPARISON) {
        if (UNIT_FAILED(print_machine_item(stream, item->comparison.left, context))) {
            return _UNIT_FAIL;
        }
        switch (item->comparison.type) {
            case UNIT_OP_COMPARE_EQUAL:
                PRINT(" == ");
                break;
            case UNIT_OP_COMPARE_NOT_EQUAL:
                PRINT(" != ");
                break;
            case UNIT_OP_COMPARE_LESS:
                PRINT(" < ");
                break;
            case UNIT_OP_COMPARE_LESS_EQUAL:
                PRINT(" <= ");
                break;
            case UNIT_OP_COMPARE_GREATER:
                PRINT(" > ");
                break;
            case UNIT_OP_COMPARE_GREATER_EQUAL:
                PRINT(" >= ");
                break;
            default:
                _UNIT_Unreachable();
        }
        if (UNIT_FAILED(print_machine_item(stream, item->comparison.right, context))) {
            return _UNIT_FAIL;
        }
    } else if (item->type == _UNIT_TYPE_MEMORY) {
        PRINT("stack_slot_%ld", item->value);
    } else {
        assert(item->type == _UNIT_TYPE_REGISTER);
        PRINT("register_%ld", item->value);
    }

    if (item->hint != NULL) {
        PRINT(" (%s)", item->hint);
    }

#undef PRINT

    return _UNIT_OK;
}

static UNIT_Status
print_instruction_stream(FILE *stream, _UNIT_Vector *instructions)
{
#define PRINT(...)                                                                  \
    if (fprintf(stream, __VA_ARGS__) < 0) {                                         \
        _UNIT_SetOSError(instructions->context, "printing instruction stream");     \
        return _UNIT_FAIL;                                                          \
    }

    assert(instructions != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(instructions,
                                                             index);
        assert(operation != NULL);
        if (operation->instruction == _UNIT_I_JUMP_LABEL) {
            // There should be a "block ..." right before this
            _UNIT_MachineItem *destination = _UNIT_MachineDestination_GetPointer(operation->destination);
            PRINT(", label %s (%ld):\n", destination->hint, destination->value);
            continue;
        }
        assert(operation != NULL);
        PRINT("        ");
        _UNIT_MachineItem *destination = _UNIT_MachineDestination_GetPointerNullable(operation->destination);
        int8_t is_input = _UNIT_MachineDestination_IsInput(operation->destination);
        if (destination != NULL && !is_input) {
            if (UNIT_FAILED(print_machine_item(stream, destination,
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
            if (UNIT_FAILED(print_machine_item(stream, destination,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }
            PRINT(", ");
        }
        if (operation->argument_1 != NULL) {
            if (UNIT_FAILED(print_machine_item(stream, operation->argument_1,
                                               instructions->context))) {
                return _UNIT_FAIL;
            }
        }

        if (operation->argument_2 != NULL) {
            assert(operation->argument_1 != NULL || !_UNIT_MachineDestination_IsNull(operation->destination));
            PRINT(", ");
            if (UNIT_FAILED(print_machine_item(stream, operation->argument_2,
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
    if (fprintf(stream, __VA_ARGS__) < 0) {                                 \
        _UNIT_SetOSError(translation->context, "printing translation");     \
        return _UNIT_FAIL;                                                  \
    }

    assert(translation != NULL);
    assert(name != NULL);

    PRINT("translation for \"%s\":\n", name);
    UNIT_Size size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        assert(block != NULL);
        PRINT("    block %ld", block->id);
        if (block->label_id != _UNIT_BasicBlock_NO_LABEL) {
            // We don't have access to the label name here, but the
            // instructions do. The jump label will be the first instruction
            // and will print out the name for us, so we don't want to print
            // a newline.
        } else {
            PRINT("\n");
        }
        if (UNIT_FAILED(print_instruction_stream(stream, &block->instructions))) {
            return _UNIT_FAIL;
        }
    }

#undef PRINT
    return _UNIT_OK;
}

static inline void
attach_item_to_translation(_UNIT_Translation *translation, _UNIT_MachineItem *item)
{
    // Machine items have complicated lifetimes that are hard to reason about,
    // so we just collect them all sequentially during cleanup of the whole
    // translation.
    _UNIT_MachineItem *next = translation->item_list_head;
    item->next = next;
    translation->item_list_head = item;
}

static inline _UNIT_MachineItem *
new_machine_item(_UNIT_Translation *translation, _UNIT_MachineItem_Type type,
                 int64_t value, const char *hint)
{
    _UNIT_MachineItem *item = _UNIT_Alloc(translation->context, sizeof(_UNIT_MachineItem));
    if (item == NULL) {
        return NULL;
    }
    item->type = type;
    item->value = value;
    item->hint = hint;
    attach_item_to_translation(translation, item);
    return item;
}

static UNIT_JumpLabel *
get_jump_label(const UNIT_Procedure *procedure, int32_t id)
{
    assert(procedure != NULL);
    assert(id >= 0);
    UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels, id);
    return label;
}

static _UNIT_MachineItem *
get_jump_target_item(_UNIT_Translation *translation, const UNIT_Procedure *procedure,
                     int32_t id, UNIT_JumpLabel **jump_label_ptr)
{
    UNIT_JumpLabel *label = get_jump_label(procedure, id);
    if (label == NULL) {
        _UNIT_SetErrorFormat(translation->context, UNIT_ERROR_INVALID_USAGE,
                             "%d is not a known jump label", id);
        return NULL;
    }
    if (jump_label_ptr != NULL) {
        *jump_label_ptr = label;
    }
    _UNIT_MachineItem *item = new_machine_item(translation, _UNIT_TYPE_CONSTANT, label->id, label->name);
    if (item == NULL) {
        return NULL;
    }
    return item;
}

UNIT_Status
analyze_liveness(_UNIT_Translation *translation)
{
    assert(translation != NULL);
    UNIT_Size block_count = _UNIT_Vector_SIZE(&translation->blocks);

    int8_t changed = 1;
    while (changed) {
        changed = 0;
        for (UNIT_Size index = block_count; index > 0; --index) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks,
                                                       index - 1);
            assert(block != NULL);
            if (UNIT_FAILED(_UNIT_BasicBlock_PopulateLivenessStep(block, &changed))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

_UNIT_BasicBlock *
push_new_block(_UNIT_Translation *translation, UNIT_Size *block_id)
{
    assert(translation != NULL);
    _UNIT_BasicBlock *block = _UNIT_BasicBlock_New(translation->context, *block_id);
    if (block == NULL) {
        return NULL;
    }
    ++(*block_id);

    if (UNIT_FAILED(_UNIT_Vector_Append(&translation->blocks,
                                        block))) {
        return NULL;
    }

    return block;
}


static _UNIT_MachineItem *
create_new_location(_UNIT_Translation *translation, _UNIT_BasicBlock *block, int32_t value)
{
    _UNIT_MachineItem *item = new_machine_item(translation, _UNIT_TYPE_LOCATION, value, NULL);
    if (item == NULL) {
        return NULL;
    }

    assert(_UNIT_SizeSet_Contains(&block->liveness.created_locations, value) == 0);
    if (UNIT_FAILED(_UNIT_SizeSet_Add(&block->liveness.created_locations, value))) {
        return NULL;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&block->instructions);
    if (UNIT_FAILED(_UNIT_SizeMap_Set(&block->liveness.last_uses, value, index))) {
        return NULL;
    }

    return item;
}

static _UNIT_MachineItem *
stack_pop(_UNIT_BasicBlock *block, _UNIT_Vector *stack,
          _UNIT_Operation *operation)
{
    assert(block != NULL);
    assert(stack != NULL);
    assert(operation != NULL);
    assert(_UNIT_Vector_SIZE(stack) >= 0);
    if (_UNIT_Vector_SIZE(stack) == 0) {
        _UNIT_SetErrorFormat(block->context, UNIT_ERROR_INVALID_USAGE,
                             "stack underflow at %s",
                             UNIT_Instruction_GetName(operation->instruction));
        return NULL;
    }
    _UNIT_MachineItem *result = _UNIT_Vector_Pop(stack);
    assert(result != NULL);

    if (result->type == _UNIT_TYPE_LOCATION) {
        if (!_UNIT_SizeSet_Contains(&block->liveness.created_locations, result->value)) {
            _UNIT_SizeSet_Add(&block->liveness.used_locations, result->value);
        }
    }
    return result;
}

// Eagerly create basic blocks for every jump label.
UNIT_Status
create_jump_label_blocks(UNIT_Context *context, const _UNIT_Vector *jump_labels)
{
    assert(jump_labels != NULL);
    UNIT_Size block_id = 0;
    UNIT_Size size = _UNIT_Vector_SIZE(jump_labels);
    for (UNIT_Size index = 0; index < size; ++index) {
        UNIT_JumpLabel *label = _UNIT_Vector_GET(jump_labels, index);
        assert(label != NULL);
        _UNIT_BasicBlock *label_block = _UNIT_BasicBlock_New(context, block_id++);
        if (label_block == NULL) {
            return _UNIT_FAIL;
        }
        assert(label->_block == NULL);
        label->_block = label_block;
    }

    return _UNIT_OK;

}

typedef struct {
    UNIT_Size location_id;
    // Assigned when address is taken, then location_id becomes invalid
    UNIT_Size stack_slot;
} LocalState;

typedef struct {
    UNIT_Context *context;
    _UNIT_Map locals_map;
    UNIT_Size next_stack_slot;
} LocalVariables;

static bool
compare_int32_deref(const void *ptr_a, const void *ptr_b)
{
    return *((int32_t *)ptr_a) == *((int32_t *)ptr_b);
}

static UNIT_Size
hash_int32_deref(const void *key)
{
    return (UNIT_Size)(*(int32_t *)key);
}

static UNIT_Status
LocalVariables_Init(LocalVariables *locals, UNIT_Context *context)
{
    assert(locals != NULL);
    assert(context != NULL);
    locals->context = context;
    locals->next_stack_slot = 0;
    if (UNIT_FAILED(_UNIT_Map_Init(&locals->locals_map, context, 8, compare_int32_deref,
                                   hash_int32_deref, _UNIT_Dealloc, _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

LocalState *
create_new_local(LocalVariables *locals, int32_t name, UNIT_Size id)
{
    assert(locals != NULL);
    // Technically the name could be negative so we won't assert here.
    assert(id >= 0);
    UNIT_Context *context = locals->context;
    int32_t *copy = _UNIT_Alloc(context, sizeof(int32_t));
    if (copy == NULL) {
        return NULL;
    }

    *copy = name;

    LocalState *local_state = _UNIT_Alloc(context, sizeof(LocalState));
    if (local_state == NULL) {
        _UNIT_Dealloc(context, copy);
        return NULL;
    }

    local_state->location_id = id;
    local_state->stack_slot = -1;

    if (UNIT_FAILED(_UNIT_Map_Set(&locals->locals_map, copy, local_state))) {
        _UNIT_Dealloc(context, copy);
        _UNIT_Dealloc(context, local_state);
        return NULL;
    }

    return local_state;
}

LocalState *
get_local(LocalVariables *locals, int32_t name)
{
    assert(locals != NULL);
    LocalState *local_state = _UNIT_Map_Get(&locals->locals_map, &name);
    return local_state;
}

void
LocalVariables_Clear(LocalVariables *locals)
{
    _UNIT_Map_Clear(&locals->locals_map);
}

typedef struct {
    UNIT_Size label_id;
    UNIT_Size index;
    UNIT_Size location_id;
    int8_t is_stack;
} Snapshot;

static UNIT_Status
snapshot_locals(LocalVariables *locals, _UNIT_Vector *snapshots, UNIT_Size label_id)
{
    assert(locals != NULL);
    assert(snapshots != NULL);
    _UNIT_Map_ITER(&locals->locals_map, key, value);
        assert(key != NULL);
        assert(value != NULL);
        int32_t local_index = *(int32_t *)key;
        assert(local_index >= 0);
        LocalState *state = (LocalState *)value;
        Snapshot *snapshot = _UNIT_Alloc(locals->context,
                                              sizeof(Snapshot));
        if (snapshot == NULL) {
            return _UNIT_FAIL;
        }

        snapshot->label_id = label_id;
        snapshot->index = local_index;
        snapshot->location_id = state->location_id;
        snapshot->is_stack = 0;
        if (UNIT_FAILED(_UNIT_Vector_Append(snapshots, snapshot))) {
            return _UNIT_FAIL;
        }
    _UNIT_Map_END_ITER();

    return _UNIT_OK;
}

static UNIT_Status
handle_jump_snapshot(_UNIT_Translation *translation,
                     LocalVariables *locals,
                     _UNIT_Vector *stack,
                     _UNIT_Vector *snapshots,
                     _UNIT_BasicBlock *current_block,
                     UNIT_Size label_id,
                     UNIT_Size *unique_id)
{
    int8_t found_snapshot = 0;
    UNIT_Size snap_count = _UNIT_Vector_SIZE(snapshots);
    for (UNIT_Size index = 0; index < snap_count; ++index) {
        Snapshot *snap = _UNIT_Vector_GET(snapshots, index);
        if (snap->label_id != label_id) {
            continue;
        }

        found_snapshot = 1;
        UNIT_Size current_location;
        if (snap->is_stack) {
            if (snap->index >= _UNIT_Vector_SIZE(stack)) {
                continue;
            }

            _UNIT_MachineItem *item = _UNIT_Vector_GET(stack, snap->index);
            if (item->type == _UNIT_TYPE_LOCATION) {
                current_location = item->value;
            } else {
                _UNIT_MachineItem *dest = new_machine_item(translation, _UNIT_TYPE_LOCATION,
                                                           snap->location_id, NULL);
                if (dest == NULL) {
                    return _UNIT_FAIL;
                }
                if (UNIT_FAILED(emit_machine_instruction(translation->context, current_block,
                                                         _UNIT_I_LOAD,
                                                         _UNIT_MachineDestination_FromDestination(dest),
                                                         item, NULL))) {
                    return _UNIT_FAIL;
                }
                continue;
            }
        } else {
            LocalState *state = get_local(locals, snap->index);
            if (state == NULL) {
                continue;
            }

            current_location = state->location_id;
        }

        if (current_location == snap->location_id) {
            continue;
        }

        _UNIT_MachineItem *dest = new_machine_item(translation, _UNIT_TYPE_LOCATION,
                                                    snap->location_id, NULL);
        if (dest == NULL) {
            return _UNIT_FAIL;
        }
        _UNIT_MachineItem *src = new_machine_item(translation, _UNIT_TYPE_LOCATION,
                                                   current_location, NULL);
        if (src == NULL) {
            return _UNIT_FAIL;
        }
        if (UNIT_FAILED(emit_machine_instruction(translation->context, current_block,
                                                 _UNIT_I_LOAD,
                                                 _UNIT_MachineDestination_FromDestination(dest),
                                                 src, NULL))) {
            return _UNIT_FAIL;
        }
    }

    if (!found_snapshot) {
        if (UNIT_FAILED(snapshot_locals(locals, snapshots, label_id))) {
            return _UNIT_FAIL;
        }

        UNIT_Size depth = _UNIT_Vector_SIZE(stack);
        for (UNIT_Size index = 0; index < depth; ++index) {
            _UNIT_MachineItem *item = _UNIT_Vector_GET(stack, index);
            // We need to turn non-locations into locations so we can move them later
            if (item->type != _UNIT_TYPE_LOCATION) {
                ++(*unique_id);
                _UNIT_MachineItem *dest = create_new_location(translation, current_block, *unique_id);
                if (dest == NULL) {
                    return _UNIT_FAIL;
                }
                if (UNIT_FAILED(emit_machine_instruction(translation->context, current_block,
                                                         _UNIT_I_LOAD,
                                                         _UNIT_MachineDestination_FromDestination(dest),
                                                         item, NULL))) {
                    return _UNIT_FAIL;
                }

                _UNIT_Vector_SET(stack, index, dest);
                item = dest;
            }

            assert(item->type == _UNIT_TYPE_LOCATION);
            Snapshot *snapshot = _UNIT_Alloc(translation->context, sizeof(Snapshot));
            if (snapshot == NULL) {
                return _UNIT_FAIL;
            }
            snapshot->label_id = label_id;
            snapshot->index = index;
            snapshot->location_id = item->value;
            snapshot->is_stack = 1;
            if (UNIT_FAILED(_UNIT_Vector_Append(snapshots, snapshot))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

UNIT_Status
_UNIT_Translate(_UNIT_Translation *translation,
                const UNIT_Procedure *procedure)
{
    assert(translation != NULL);
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    translation->context = context;
    translation->item_list_head = NULL;

    _UNIT_Vector stack;
    if (UNIT_FAILED(_UNIT_Vector_Init(&stack, context, 16, NULL))) {
        return _UNIT_FAIL;
    }

    LocalVariables locals;
    if (UNIT_FAILED(LocalVariables_Init(&locals, context))) {
        _UNIT_Vector_Clear(&stack);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Map_Init(&translation->strings, context, 8,
                    _UNIT_Map_CompareEqual, _UNIT_Map_HashDirect,
                    NULL, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&stack);
        LocalVariables_Clear(&locals);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&translation->blocks, context, 16,
                                      _UNIT_BasicBlock_Free))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_Map_Clear(&translation->strings);
        LocalVariables_Clear(&locals);
        return _UNIT_FAIL;
    }

    _UNIT_SizeSet address_taken_locals;
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&address_taken_locals, context, 8))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_Map_Clear(&translation->strings);
        LocalVariables_Clear(&locals);
        _UNIT_Vector_Clear(&translation->blocks);
        return _UNIT_FAIL;
    }

    _UNIT_Vector locals_snapshots;
    if (UNIT_FAILED(_UNIT_Vector_Init(&locals_snapshots, context, 16, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_Map_Clear(&translation->strings);
        LocalVariables_Clear(&locals);
        _UNIT_Vector_Clear(&translation->blocks);
        _UNIT_SizeSet_Clear(&address_taken_locals);
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);

    // TODO: This is ugly. We should refactor this out into a more generic
    // analysis/metadata pass.
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions, index);
        assert(operation != NULL);
        if (operation->instruction == UNIT_OP_ADDRESS_OF) {
            if (UNIT_FAILED(_UNIT_SizeSet_Add(&address_taken_locals, operation->argument))) {
                goto error;
            }
        }
    }

    // We need to eagerly assign blocks for each jump label so we can resolve
    // successors immediately
    if (UNIT_FAILED(create_jump_label_blocks(context, &procedure->_jump_labels))) {
        goto error;
    }
    UNIT_Size _block_id = 0;
    UNIT_Size _location_id = 0;
    _UNIT_Vector *_instructions;
    _UNIT_BasicBlock *_block;

    #define UNIQUE_ID() (++_location_id)

    // Technically these two are redundant, but I think it improves readability.
    // It also makes refactoring easier if we want to change where these are
    // stored.
    #define CURRENT_BLOCK() _block
    #define INSTRUCTIONS() _instructions

    #define START_NEW_BLOCK()                                               \
        _block = push_new_block(translation, &_block_id);                   \
        if (_block == NULL) {                                               \
            goto error;                                                     \
        }                                                                   \
        _instructions = &_block->instructions;

    #define START_EXISTING_BLOCK(name)                                      \
        _block = name;                                                      \
        _instructions = &_block->instructions;                              \
        if (UNIT_FAILED(_UNIT_Vector_Append(&translation->blocks,           \
                                            name))) {                       \
            goto error;                                                     \
        }

    #define ADD_BLOCK_SUCCESSOR(block, name)                        \
        if (UNIT_FAILED(_UNIT_Vector_Append(&block->successors,     \
                                            name))) {               \
            goto error;                                             \
        }

    #define PUSH_ITEM(item)                                     \
        if (UNIT_FAILED(_UNIT_Vector_Append(&stack, item))) {   \
            goto error;                                         \
        }

    #define PUSH_NEW(tp, val)                                                   \
        _UNIT_MachineItem *item = new_machine_item(translation, tp, val, NULL); \
        if (item == NULL) {                                                     \
            goto error;                                                         \
        }                                                                       \
        PUSH_ITEM(item);

    #define POP_TO_VAR(varname)                                                         \
        _UNIT_MachineItem *varname = stack_pop(CURRENT_BLOCK(), &stack, operation);     \
        if (varname == NULL) {                                                          \
            goto error;                                                                 \
        }

    #define ARGUMENT_TO_ITEM(name, type)                                                            \
        _UNIT_MachineItem *name = new_machine_item(translation, type, operation->argument, NULL);   \
        if (name == NULL) {                                                                         \
            goto error;                                                                             \
        }

    #define EMIT_EMPTY(inst)                                                                                                        \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, _UNIT_MachineDestination_NULL, NULL, NULL))) {     \
            goto error;                                                                                                             \
        }

    #define EMIT_ONE(inst, arg1)                                                                                                    \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, _UNIT_MachineDestination_NULL, arg1, NULL))) {     \
            goto error;                                                                                                             \
        }

    #define EMIT_DEST(inst, dest)                                                                                   \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst,                                    \
                                                 _UNIT_MachineDestination_FromDestination(dest), NULL, NULL))) {    \
            goto error;                                                                                             \
        }

    #define EMIT_DEST_ONE(inst, dest, arg1)                                                                         \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst,                                    \
                                                 _UNIT_MachineDestination_FromDestination(dest), arg1, NULL))) {    \
            goto error;                                                                                             \
        }

    #define EMIT_DEST_TWO(inst, dest, arg1, arg2)                                                                   \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst,                                    \
                                                 _UNIT_MachineDestination_FromDestination(dest), arg1, arg2))) {    \
            goto error;                                                                                             \
        }

    #define EMIT_THREE(inst, arg1, arg2, arg3)                                                              \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst,                            \
                                                 _UNIT_MachineDestination_FromInput(arg1), arg2, arg3))) {  \
            goto error;                                                                                     \
        }

    #define CREATE_DESTINATION(name)                                                                \
        _UNIT_MachineItem *name = create_new_location(translation, CURRENT_BLOCK(), UNIQUE_ID());   \
        if (name == NULL) {                                                                         \
            goto error;                                                                             \
        }                                                                                           \
        PUSH_ITEM(name);

    #define INST_CHECK(condition, message)                                                  \
        if (!(condition)) {                                                                 \
            _UNIT_SetErrorFormat(_block->context, UNIT_ERROR_INVALID_USAGE,                 \
                                 "error at %s (index %d): " message,                        \
                                 UNIT_Instruction_GetName(operation->instruction), index);  \
            goto error;                                                                     \
        }

    #define INST_CHECK_FMT(condition, message, ...)                                         \
        if (!(condition)) {                                                                 \
            _UNIT_SetErrorFormat(_block->context, UNIT_ERROR_INVALID_USAGE,                 \
                                 "error at %s (index %d): " message,                        \
                                 UNIT_Instruction_GetName(operation->instruction), index,   \
                                 __VA_ARGS__);                                              \
            goto error;                                                                     \
        }
    #define INST_CHECK_OPARG(condition, message) INST_CHECK_FMT(condition, message, operation->argument)

    #define TAKE_SNAPSHOT()                                                             \
        if (UNIT_FAILED(handle_jump_snapshot(translation, &locals, &stack,              \
                                            &locals_snapshots, CURRENT_BLOCK(),         \
                                            label->id, &_location_id))) {               \
            goto error;                                                                 \
        }

    START_NEW_BLOCK();

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions, index);
        switch (operation->instruction) {
            /* Constants */

            case UNIT_OP_LOAD_INTEGER: {
                PUSH_NEW(_UNIT_TYPE_CONSTANT, operation->argument);
                break;
            }

            case UNIT_OP_LOAD_STRING: {
                const char *text = _UNIT_Vector_GET(&procedure->_global_strings, operation->argument);
                INST_CHECK_OPARG(text != NULL, "%d is not a known string ID");
                _UNIT_MachineItem *result = new_machine_item(translation, _UNIT_TYPE_CONSTANT,
                                                             operation->argument, text);
                if (result == NULL) {
                    goto error;
                }
                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_LOAD_STRING, destination, result);
                break;
            }

            /* Variables */

            case _UNIT_OP_STORE_LOCAL_NAME:
            case UNIT_OP_STORE_LOCAL: {
                const char *hint = NULL;
                if (operation->instruction == _UNIT_OP_STORE_LOCAL_NAME) {
                    hint = _UNIT_Vector_GET(&procedure->_local_variables,
                                            operation->argument);
                }

                UNIT_Size location_id = UNIQUE_ID();
                LocalState *local_state = create_new_local(&locals,
                                                                operation->argument,
                                                                location_id);
                if (local_state == NULL) {
                    goto error;
                }
                _UNIT_MachineItem *location = create_new_location(translation,
                                                                CURRENT_BLOCK(),
                                                                location_id);
                if (location == NULL) {
                    goto error;
                }
                location->hint = hint;

                POP_TO_VAR(item);
                EMIT_DEST_ONE(_UNIT_I_LOAD, location, item);

                if (_UNIT_SizeSet_Contains(&address_taken_locals, operation->argument)) {
                    local_state->stack_slot = locals.next_stack_slot++;
                    _UNIT_MachineItem *slot = new_machine_item(translation, _UNIT_TYPE_MEMORY,
                                                            local_state->stack_slot, hint);
                    if (slot == NULL) {
                        goto error;
                    }
                    // Sharing machine items is probably not a great idea but it's
                    // not causing any problems at the moment. We can easily make
                    // a copy of the location later if we need to.
                    EMIT_DEST_ONE(_UNIT_I_LOAD, slot, location);
                }
                break;
            }

            case _UNIT_OP_LOAD_LOCAL_NAME:
            case UNIT_OP_LOAD_LOCAL: {
                LocalState *local_state = get_local(&locals, operation->argument);
                INST_CHECK_OPARG(local_state != NULL, "local variable %d not assigned");

                const char *hint = NULL;
                if (operation->instruction == _UNIT_OP_LOAD_LOCAL_NAME) {
                    hint = _UNIT_Vector_GET(&procedure->_local_variables,
                                            operation->argument);
                }

                _UNIT_MachineItem *location = new_machine_item(translation, _UNIT_TYPE_LOCATION,
                                                           local_state->location_id, hint);
                if (location == NULL) {
                    goto error;
                }

                if (local_state->stack_slot != -1) {
                    _UNIT_MachineItem *slot = new_machine_item(translation, _UNIT_TYPE_MEMORY,
                                                               local_state->stack_slot, hint);
                    if (slot == NULL) {
                        goto error;
                    }
                    EMIT_DEST_ONE(_UNIT_I_LOAD, location, slot);
                }

                PUSH_ITEM(location);
                break;
            }

            /* Arithmetic */

#define BINARY_OPERATION(opcode, inst)                          \
            case opcode: {                                      \
                POP_TO_VAR(right);                              \
                POP_TO_VAR(left);                               \
                CREATE_DESTINATION(destination);                \
                EMIT_DEST_TWO(inst, destination, left, right);  \
                break;                                          \
            }

            BINARY_OPERATION(UNIT_OP_ADD, _UNIT_I_ADD);
            BINARY_OPERATION(UNIT_OP_SUBTRACT, _UNIT_I_SUB);
            BINARY_OPERATION(UNIT_OP_MULTIPLY, _UNIT_I_MUL);
            BINARY_OPERATION(UNIT_OP_DIVIDE, _UNIT_I_DIV);
            BINARY_OPERATION(UNIT_OP_MODULO, _UNIT_I_MOD);

#undef BINARY_OPERATION

            /* Jumps */


            case _UNIT_OP_JUMP_MARKER: {
                UNIT_JumpLabel *label;
                _UNIT_MachineItem *item = get_jump_target_item(translation, procedure,
                                                               operation->argument,
                                                               &label);
                if (item == NULL) {
                    goto error;
                }
                _UNIT_BasicBlock *block = label->_block;
                block->id = _block_id++;
                block->label_id = label->id;

                // Restore locations from forward-jump snapshot if one exists
                int8_t found_snapshot = 0;
                UNIT_Size snap_count = _UNIT_Vector_SIZE(&locals_snapshots);
                for (UNIT_Size index = 0; index < snap_count; ++index) {
                    Snapshot *snap = _UNIT_Vector_GET(&locals_snapshots, index);
                    if (snap->label_id != label->id) {
                        continue;
                    }
                    found_snapshot = 1;
                    if (snap->is_stack) {
                        if (snap->index < _UNIT_Vector_SIZE(&stack)) {
                            _UNIT_MachineItem *new_item = new_machine_item(translation,
                                                                            _UNIT_TYPE_LOCATION,
                                                                            snap->location_id, NULL);
                            if (new_item == NULL) {
                                goto error;
                            }
                            _UNIT_Vector_SET(&stack, snap->index, new_item);
                        }
                    } else {
                        LocalState *state = get_local(&locals, snap->index);
                        if (state != NULL) {
                            state->location_id = snap->location_id;
                        }
                    }
                }

                if (!found_snapshot) {
                    TAKE_SNAPSHOT();
                }

                // The jump label succeeds the current block because it fell
                // through.
                ADD_BLOCK_SUCCESSOR(CURRENT_BLOCK(), block);
                START_EXISTING_BLOCK(block);
                EMIT_DEST(_UNIT_I_JUMP_LABEL, item);
                break;
            }

            case UNIT_OP_JUMP_TO: {
                UNIT_JumpLabel *label;
                _UNIT_MachineItem *item = get_jump_target_item(translation, procedure,
                                                               operation->argument,
                                                               &label);
                if (item == NULL) {
                    goto error;
                }

                TAKE_SNAPSHOT();
                EMIT_ONE(_UNIT_I_JUMP, item);
                ADD_BLOCK_SUCCESSOR(CURRENT_BLOCK(), label->_block);
                START_NEW_BLOCK();
                break;
            }

            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                POP_TO_VAR(value);
                if (value->type != _UNIT_TYPE_COMPARISON) {
                    _UNIT_SetError(context, UNIT_ERROR_INVALID_USAGE,
                                   "JUMP_IF_FALSE/JUMP_IF_TRUE got non-comparison");
                    goto error;
                }
                UNIT_JumpLabel *label;
                _UNIT_MachineItem *jump_target = get_jump_target_item(translation, procedure,
                                                                      operation->argument,
                                                                      &label);
                if (jump_target == NULL) {
                    goto error;
                }
                _UNIT_MachineInstruction fused;
                int8_t invert = (operation->instruction == UNIT_OP_JUMP_IF_FALSE);

                switch (value->comparison.type) {
                    case UNIT_OP_COMPARE_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_NOT_EQUAL : _UNIT_I_JUMP_IF_EQUAL;
                        break;
                    case UNIT_OP_COMPARE_NOT_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_EQUAL : _UNIT_I_JUMP_IF_NOT_EQUAL;
                        break;
                    case UNIT_OP_COMPARE_GREATER:
                        fused = invert ? _UNIT_I_JUMP_IF_LESS_EQUAL : _UNIT_I_JUMP_IF_GREATER;
                        break;
                    case UNIT_OP_COMPARE_GREATER_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_LESS : _UNIT_I_JUMP_IF_GREATER_EQUAL;
                        break;
                    case UNIT_OP_COMPARE_LESS:
                        fused = invert ? _UNIT_I_JUMP_IF_GREATER_EQUAL : _UNIT_I_JUMP_IF_LESS;
                        break;
                    case UNIT_OP_COMPARE_LESS_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_GREATER : _UNIT_I_JUMP_IF_LESS_EQUAL;
                        break;
                    default:
                        _UNIT_Unreachable();
                }

                TAKE_SNAPSHOT();
                EMIT_THREE(fused, jump_target,
                           value->comparison.left,
                           value->comparison.right);
                _UNIT_BasicBlock *saved_block = CURRENT_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, label->_block);
                START_NEW_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, CURRENT_BLOCK());
                break;
            }

            /* Functions */

            case UNIT_OP_EXIT: {
                POP_TO_VAR(exit_code);
                EMIT_DEST(_UNIT_I_EXIT, exit_code);
                break;
            }

            case UNIT_OP_RETURN_VALUE: {
                POP_TO_VAR(value);
                EMIT_ONE(_UNIT_I_RETURN_VALUE, value);
                START_NEW_BLOCK();
                break;
            }

            case UNIT_OP_LOAD_ARGUMENT: {
                ARGUMENT_TO_ITEM(index, _UNIT_TYPE_CONSTANT);
                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_LOAD_ARGUMENT, destination, index);
                break;
            }

            /* Function calls */

            case UNIT_OP_PREPARE_CALL: {
                _UNIT_Vector *vector = _UNIT_Vector_New(context, operation->argument,
                                                        NULL);
                if (vector == NULL) {
                    goto error;
                }
                for (UNIT_Size index = 0; index < operation->argument; ++index) {
                    POP_TO_VAR(item);
                    _UNIT_Vector_APPEND(vector, item);
                }
                // We want arguments to be consumed from left to right
                _UNIT_Vector_Reverse(vector);

                _UNIT_MachineItem *args = _UNIT_Alloc(context, sizeof(_UNIT_MachineItem));
                if (args == NULL) {
                    _UNIT_Vector_Free(vector);
                    goto error;
                }
                args->call_args = vector;
                args->type = _UNIT_TYPE_CALL_ARGS;
                args->hint = NULL;
                attach_item_to_translation(translation, args);
                PUSH_ITEM(args);
                break;
            }

            case UNIT_OP_CALL_NAME: {
                ARGUMENT_TO_ITEM(symbol, _UNIT_TYPE_CONSTANT);
                symbol->hint = _UNIT_Vector_GET(&procedure->_symbols, operation->argument);
                POP_TO_VAR(args);
                INST_CHECK(args->type == _UNIT_TYPE_CALL_ARGS, "got non-args item off stack");
                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_CALL_SYMBOL, destination, symbol, args);
                break;
            }

            case UNIT_OP_CALL_PROCEDURE: {
                UNIT_Procedure *subprocedure = _UNIT_Vector_GET(&procedure->_subprocedures, operation->argument);
                assert(subprocedure != NULL);
                ARGUMENT_TO_ITEM(procedure_id, _UNIT_TYPE_CONSTANT);
                procedure_id->hint = subprocedure->name;

                // TODO: Inlining
                // if (should_inline(...)) _UNIT_Translate(translation, subprocedure)

                POP_TO_VAR(args);
                INST_CHECK(args->type == _UNIT_TYPE_CALL_ARGS, "got non-args item off stack");
                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_CALL_SYMBOL, destination, procedure_id, args);
                break;
            }

            /* Comparisons */

            case UNIT_OP_COMPARE_EQUAL:
            case UNIT_OP_COMPARE_NOT_EQUAL:
            case UNIT_OP_COMPARE_GREATER:
            case UNIT_OP_COMPARE_GREATER_EQUAL:
            case UNIT_OP_COMPARE_LESS:
            case UNIT_OP_COMPARE_LESS_EQUAL: {
                POP_TO_VAR(right);
                POP_TO_VAR(left);
                CREATE_DESTINATION(destination);

                destination->type = _UNIT_TYPE_COMPARISON;
                destination->comparison.type = operation->instruction;
                destination->comparison.left = left;
                destination->comparison.right = right;
                // We intentionally don't emit any instructions here to allow
                // for a jump to fuse it the comparison.
                break;
            }

            /* Stack manipulation */

            case UNIT_OP_COPY: {
                UNIT_Size index = _UNIT_Vector_SIZE(&stack) - operation->argument - 1;
                _UNIT_MachineItem *item = _UNIT_Vector_GET(&stack, index);
                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_LOAD, destination, item);
                break;
            }

            case UNIT_OP_SWAP: {
                POP_TO_VAR(top);
                UNIT_Size index = _UNIT_Vector_SIZE(&stack) - operation->argument - 1;
                assert(index >= 0);
                _UNIT_MachineItem *to_swap = _UNIT_Vector_STEAL(&stack, index);
                _UNIT_Vector_SET(&stack, index, top);
                PUSH_ITEM(to_swap);
                break;
            }

            case UNIT_OP_POP: {
                POP_TO_VAR(_unused);
                break;
            }

            /* Pointers */

            case UNIT_OP_ADDRESS_OF: {
                LocalState *local_state = get_local(&locals, operation->argument);
                INST_CHECK_OPARG(local_state != NULL, "local variable %d not assigned");
                assert(local_state->stack_slot != -1);

                _UNIT_MachineItem *value;
                if (local_state->stack_slot == -1) {
                    value = new_machine_item(translation, _UNIT_TYPE_LOCATION,
                                             local_state->location_id, NULL);
                } else {
                    value = new_machine_item(translation, _UNIT_TYPE_MEMORY,
                                             local_state->stack_slot, NULL);
                }

                if (value == NULL) {
                    goto error;
                }

                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_ADDRESS_OF, destination, value);
                break;
            }

            case UNIT_OP_READ_BYTES: {
                INST_CHECK_OPARG(operation->argument > 0, "must read at least 1 byte (got %d)");
                ARGUMENT_TO_ITEM(bytes, _UNIT_TYPE_CONSTANT);
                POP_TO_VAR(address);
                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_READ_BYTES, destination, address, bytes);
                break;
            }

            case UNIT_OP_WRITE_BYTES: {
                INST_CHECK_OPARG(operation->argument > 0, "must write at least 1 byte (got %d)");
                ARGUMENT_TO_ITEM(bytes, _UNIT_TYPE_CONSTANT);
                POP_TO_VAR(value);
                POP_TO_VAR(address);
                if (UNIT_FAILED(mark_last_use(CURRENT_BLOCK(), address))) {
                    goto error;
                }
                EMIT_THREE(_UNIT_I_WRITE_BYTES, address, value, bytes);
                break;
            }

            case UNIT_OP_CONVERT: {
                POP_TO_VAR(value);
                _UNIT_MachineItem *type_item = new_machine_item(translation,
                                                                _UNIT_TYPE_CONSTANT,
                                                                operation->argument,
                                                                integer_type_name(operation->argument));
                if (type_item == NULL) {
                    goto error;
                }
                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_CONVERT, destination, value, type_item);
                break;
            }
        }
    }

    if (_UNIT_Vector_SIZE(&stack) != 0) {
        _UNIT_SetError(context, UNIT_ERROR_INVALID_USAGE,
                       "procedure does not consume entire stack");
        goto error;
    }

    _UNIT_Vector_Clear(&stack);
    LocalVariables_Clear(&locals);
    _UNIT_SizeSet_Clear(&address_taken_locals);
    _UNIT_Vector_Clear(&locals_snapshots);
    // This is so we can determine the size of the frame later
    translation->num_memory_slots = locals.next_stack_slot;
    return analyze_liveness(translation);
error:
    _UNIT_Vector_Clear(&stack);
    LocalVariables_Clear(&locals);
    _UNIT_Map_Clear(&translation->strings);
    _UNIT_Vector_Clear(&translation->blocks);
    _UNIT_SizeSet_Clear(&address_taken_locals);
    _UNIT_Vector_Clear(&locals_snapshots);
    return _UNIT_FAIL;
}

void
_UNIT_Translation_Clear(_UNIT_Translation *translation)
{
    assert(translation != NULL);
    _UNIT_Map_Clear(&translation->strings);
    _UNIT_Vector_Clear(&translation->blocks);

    _UNIT_MachineItem *head = translation->item_list_head;
    while (head != NULL) {
        _UNIT_MachineItem *next = head->next;
        if (head->type == _UNIT_TYPE_CALL_ARGS) {
            _UNIT_Vector_Free(head->call_args);
        }
        _UNIT_Dealloc(translation->context, head);
        head = next;
    }
}
