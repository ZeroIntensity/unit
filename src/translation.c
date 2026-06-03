#include <stdio.h>
#include <stdlib.h>

#include <unit/errors.h>
#include <unit/internal/allocation.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

#define NAME(name) case name: return #name

const char *
instruction_name(UNIT_Instruction instruction)
{
    switch (instruction) {
        NAME(UNIT_OP_LOAD_CONSTANT_INTEGER);
        NAME(UNIT_OP_LOAD_CONSTANT_STRING);
        NAME(UNIT_OP_LOAD_LOCAL);
        NAME(UNIT_OP_STORE_LOCAL);
        NAME(UNIT_OP_ADD);
        NAME(UNIT_OP_SUBTRACT);
        NAME(UNIT_OP_MULTIPLY);
        NAME(UNIT_OP_DIVIDE);
        NAME(_UNIT_OP_JUMP_MARKER);
        NAME(UNIT_OP_JUMP_TO);
        NAME(UNIT_OP_CALL_NAME);
        NAME(UNIT_OP_EXIT);
        NAME(UNIT_OP_POP_TOP);
        NAME(UNIT_OP_PREPARE_CALL);
        NAME(UNIT_OP_RETURN_VALUE);
        NAME(UNIT_OP_JUMP_IF_FALSE);
        NAME(UNIT_OP_JUMP_IF_TRUE);
        NAME(UNIT_OP_COMPARE);
        NAME(UNIT_OP_ADDRESS_OF);
    }
    fprintf(stderr, "unknown machine instruction\n");
    abort();
}

const char *
machine_instruction_name(_UNIT_MachineInstruction machine_instruction)
{
    switch (machine_instruction) {
        NAME(_UNIT_I_ADD);
        NAME(_UNIT_I_MOVE);
        NAME(_UNIT_I_JUMP_LABEL);
        NAME(_UNIT_I_CALL_SYMBOL);
        NAME(_UNIT_I_JUMP);
        NAME(_UNIT_I_EXIT);
        NAME(_UNIT_I_RETURN_VALUE);
        NAME(_UNIT_I_COMPARE_EQUAL);
        NAME(_UNIT_I_JUMP_IF_EQUAL);
        NAME(_UNIT_I_JUMP_IF_NOT_EQUAL);
        NAME(_UNIT_I_JUMP_IF_LESS);
        NAME(_UNIT_I_JUMP_IF_LESS_EQUAL);
        NAME(_UNIT_I_JUMP_IF_GREATER);
        NAME(_UNIT_I_JUMP_IF_GREATER_EQUAL);
        NAME(_UNIT_I_LOAD_STRING);
        NAME(_UNIT_I_ADDRESS_OF);
    }
    fprintf(stderr, "unknown machine instruction\n");
    abort();
}

#undef NAME

UNIT_Status
emit_machine_instruction(UNIT_Context *context,
                         _UNIT_Vector *instructions,
                         _UNIT_MachineInstruction instruction,
                         _UNIT_MachineItem *destination,
                         _UNIT_MachineItem *arg1,
                         _UNIT_MachineItem *arg2)
{
    _UNIT_MachineOperation *operation = _UNIT_Alloc(context,
                                                    sizeof(_UNIT_MachineOperation));
    if (operation == NULL) {
        if (destination != NULL) {
            _UNIT_Dealloc(context, destination);
        }
        if (arg1 != NULL) {
            _UNIT_Dealloc(context, arg1);
        }
        if (arg2 != NULL) {
            _UNIT_Dealloc(context, arg2);
        }
        return UNIT_FAIL;
    }

    operation->instruction = instruction;
    operation->destination = destination;
    operation->argument_1 = arg1;
    operation->argument_2 = arg2;

    if (UNIT_FAILED(_UNIT_Vector_Append(instructions,
                                        operation))) {
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

void
print_machine_item(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    if (item->type == CONSTANT) {
        printf("%d", item->value);
    } else if (item->type == LOCATION) {
        printf("loc_%d", item->value);
    } else if (item->type == CALL_ARGS) {
        printf("[");
        UNIT_Size size = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size index = 0; index < size; ++index) {
            _UNIT_MachineItem *arg_item = _UNIT_Vector_GET(item->call_args, index);
            assert(arg_item != NULL);
            print_machine_item(arg_item);
            if (index + 1 != size) {
                printf(", ");
            }
        }
        printf("]");
    } else if (item->type == COMPARISON) {
        print_machine_item(item->comparison.left);
        switch (item->comparison.type) {
            case UNIT_COMPARE_EQUAL:
                printf(" == ");
                break;
            case UNIT_COMPARE_NOT_EQUAL:
                printf(" != ");
                break;
            case UNIT_COMPARE_LESS_THAN:
                printf(" < ");
                break;
            case UNIT_COMPARE_LESS_EQUAL:
                printf(" <= ");
                break;
            case UNIT_COMPARE_GREATER_THAN:
                printf(" > ");
                break;
            case UNIT_COMPARE_GREATER_EQUAL:
                printf(" >= ");
                break;
        }
        print_machine_item(item->comparison.right);
    } else if (item->type == MEMORY) {
        printf("stack_address_%d", item->value);
    } else {
        assert(item->type == REGISTER);
        printf("register_%d", item->value);
    }

    if (item->hint != NULL) {
        printf(" (%s)", item->hint);
    }
}

static void
print_instruction_stream(_UNIT_Vector *instructions)
{
    assert(instructions != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(instructions,
                                                             index);
        assert(operation != NULL);
        if (operation->instruction == _UNIT_I_JUMP_LABEL) {
            assert(operation->destination != NULL);
            assert(operation->destination->hint != NULL);
            // There should be a "block ..." right before this
            printf(", label %s (%d):\n", operation->destination->hint,
                   operation->destination->value);
            continue;
        }
        assert(operation != NULL);
        printf("  %s", machine_instruction_name(operation->instruction));
        if (operation->destination != NULL) {
            printf(" ");
            print_machine_item(operation->destination);
        }

        if (operation->argument_1 != NULL) {
            assert(operation->destination != NULL);
            printf(", ");
            print_machine_item(operation->argument_1);
        }

        if (operation->argument_2 != NULL) {
            assert(operation->destination != NULL);
            assert(operation->argument_1 != NULL);
            printf(", ");
            print_machine_item(operation->argument_2);
        }
        printf("\n");
    }
}

void
_UNIT_Translation_PrintInstructions(_UNIT_Translation *translation)
{
    assert(translation != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        assert(block != NULL);
        printf("block %ld", block->id);
        if (block->label_id != _UNIT_BasicBlock_NO_LABEL) {
            // We don't have access to the label name here, but the
            // instructions do. The jump label will be the first instruction
            // and will print out the name for us, so we don't want to print
            // a newline.
        } else {
            printf("\n");
        }
        print_instruction_stream(&block->instructions);
    }
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
                 int32_t value, const char *hint)
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
get_jump_label(UNIT_Procedure *procedure, int32_t id)
{
    assert(procedure != NULL);
    assert(id >= 0);
    UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels, id);
    assert(label != NULL);
    return label;
}

static _UNIT_MachineItem *
get_jump_target_item(_UNIT_Translation *translation, UNIT_Procedure *procedure,
                     int32_t id, UNIT_JumpLabel **jump_label_ptr)
{
    UNIT_JumpLabel *label = get_jump_label(procedure, id);
    if (jump_label_ptr != NULL) {
        *jump_label_ptr = label;
    }
    _UNIT_MachineItem *item = new_machine_item(translation, CONSTANT, label->id, label->name);
    if (item == NULL) {
        return NULL;
    }
    return item;
}

/*
static UNIT_Status
extend_lifetime(_UNIT_Translation *translation, int32_t location)
{
    UNIT_Size current_index = _UNIT_Vector_SIZE(&translation->instructions);
    assert(_UNIT_SizeMap_GET(&translation->variables.created_at, location) >= 0);
    return _UNIT_SizeMap_Set(&translation->variables.destroyed_at,
                             location, current_index);
}*/

static _UNIT_MachineItem *
create_new_location(_UNIT_Translation *translation, _UNIT_BasicBlock *block, int32_t value)
{
    _UNIT_MachineItem *item = new_machine_item(translation, LOCATION, value, NULL);
    if (item == NULL) {
        return NULL;
    }

    assert(_UNIT_SizeSet_Contains(&block->liveness.created_locations, value) == 0);
    if (UNIT_FAILED(_UNIT_SizeSet_Add(&block->liveness.created_locations, value))) {
        return NULL;
    }

    return item;

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
                             "stack underflow at %s\n",
                             instruction_name(operation->instruction));
        return NULL;
    }
    _UNIT_MachineItem *result = _UNIT_Vector_Pop(stack);
    assert(result != NULL);

    if (result->type == LOCATION) {
        if (UNIT_FAILED(_UNIT_SizeSet_Add(&block->liveness.used_locations,
                                          result->value))) {
            return NULL;
        }
    }
    return result;
}

UNIT_Status
_UNIT_LivenessInfo_Init(_UNIT_LivenessInfo *liveness, UNIT_Context *context)
{
    assert(liveness != NULL);
    assert(context != NULL);
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->created_locations, context, 8))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->used_locations, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->alive_at_start, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        _UNIT_SizeSet_Clear(&liveness->used_locations);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeSet_Init(&liveness->alive_at_end, context, 8))) {
        _UNIT_SizeSet_Clear(&liveness->created_locations);
        _UNIT_SizeSet_Clear(&liveness->used_locations);
        _UNIT_SizeSet_Clear(&liveness->alive_at_start);
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

static UNIT_Status
set_add_and_track(_UNIT_SizeSet *set, UNIT_Size value, int8_t *changed)
{
    if (!_UNIT_SizeSet_Contains(set, value)) {
        if (UNIT_FAILED(_UNIT_SizeSet_Add(set, value))) {
            return UNIT_FAIL;
        }
        *changed = 1;
    }
    return UNIT_OK;
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
                return UNIT_FAIL;
            }
        _UNIT_SizeSet_END_ITER();
    }

    // alive_at_start = used_locations + (alive_at_end - created_locations)

    // Everything this block uses must be alive at its start
    _UNIT_SizeSet_ITER(&liveness->used_locations, location);
        if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_start,
                                          location, changed))) {
            return UNIT_FAIL;
        }
    _UNIT_SizeSet_END_ITER();

    // Everything alive at the end that this block didn't create
    // must also be alive at the start
    _UNIT_SizeSet_ITER(&liveness->alive_at_end, location);
        if (!_UNIT_SizeSet_Contains(&liveness->created_locations, location)) {
            if (UNIT_FAILED(set_add_and_track(&liveness->alive_at_start,
                                              location, changed))) {
                return UNIT_FAIL;
            }
        }
    _UNIT_SizeSet_END_ITER();

    return UNIT_OK;
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
            if (UNIT_FAILED(populate_liveness_info(&block->successors,
                                                   &block->liveness,
                                                   &changed))) {
                return UNIT_FAIL;
            }
        }
    }

    return UNIT_OK;
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

// Eagerly create basic blocks for every jump label.
UNIT_Status
create_jump_label_blocks(UNIT_Context *context, _UNIT_Vector *jump_labels)
{
    assert(jump_labels != NULL);
    UNIT_Size block_id = 0;
    UNIT_Size size = _UNIT_Vector_SIZE(jump_labels);
    for (UNIT_Size index = 0; index < size; ++index) {
        UNIT_JumpLabel *label = _UNIT_Vector_GET(jump_labels, index);
        assert(label != NULL);
        _UNIT_BasicBlock *label_block = _UNIT_BasicBlock_New(context, block_id++);
        if (label_block == NULL) {
            return UNIT_FAIL;
        }
        assert(label->_block == NULL);
        label->_block = label_block;
    }

    return UNIT_OK;

}

typedef struct {
    UNIT_Size location_id;
    // Assigned when address is taken, then location_id becomes invalid
    UNIT_Size stack_slot;
} _UNIT_LocalState;

typedef struct {
    UNIT_Context *context;
    _UNIT_Map locals_map;
    UNIT_Size next_stack_slot;
} _UNIT_LocalVariables;

static bool
compare_int32_deref(void *ptr_a, void *ptr_b)
{
    return *((int32_t *)ptr_a) == *((int32_t *)ptr_b);
}

static UNIT_Size
hash_int32_deref(void *key)
{
    return (UNIT_Size)(*(int32_t *)key);
}

static UNIT_Status
_UNIT_LocalVariables_Init(_UNIT_LocalVariables *locals, UNIT_Context *context)
{
    assert(locals != NULL);
    assert(context != NULL);
    locals->context = context;
    locals->next_stack_slot = 0;
    if (UNIT_FAILED(_UNIT_Map_Init(&locals->locals_map, context, 8, compare_int32_deref,
                                   hash_int32_deref, _UNIT_Dealloc, _UNIT_Dealloc))) {
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

UNIT_Status
create_new_local(_UNIT_LocalVariables *locals, int32_t name, UNIT_Size id)
{
    assert(locals != NULL);
    // Technically the name could be negative so we won't assert here.
    assert(id >= 0);
    UNIT_Context *context = locals->context;
    int32_t *copy = _UNIT_Alloc(context, sizeof(int32_t));
    if (copy == NULL) {
        return UNIT_FAIL;
    }

    *copy = name;

    _UNIT_LocalState *local_state = _UNIT_Alloc(context, sizeof(_UNIT_LocalState));
    if (local_state == NULL) {
        _UNIT_Dealloc(context, copy);
        return UNIT_FAIL;
    }

    local_state->location_id = id;
    local_state->stack_slot = -1;

    if (UNIT_FAILED(_UNIT_Map_Set(&locals->locals_map, copy, local_state))) {
        _UNIT_Dealloc(context, copy);
        _UNIT_Dealloc(context, local_state);
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

_UNIT_LocalState *
get_local(_UNIT_LocalVariables *locals, int32_t name)
{
    assert(locals != NULL);
    _UNIT_LocalState *local_state = _UNIT_Map_Get(&locals->locals_map, &name);
    assert(local_state != NULL);
    return local_state;
}

void
_UNIT_LocalVariables_Clear(_UNIT_LocalVariables *locals)
{
    _UNIT_Map_Clear(&locals->locals_map);
}

UNIT_Status
_UNIT_Translation_InitFromProcedure(_UNIT_Translation *translation,
                                    UNIT_Procedure *procedure)
{
    assert(translation != NULL);
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    translation->context = context;
    translation->item_list_head = NULL;

    _UNIT_Vector stack;
    if (UNIT_FAILED(_UNIT_Vector_Init(&stack, context, 16, NULL))) {
        return UNIT_FAIL;
    }

    _UNIT_LocalVariables locals;
    if (UNIT_FAILED(_UNIT_LocalVariables_Init(&locals, context))) {
        _UNIT_Vector_Clear(&stack);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Map_Init(&translation->strings, context, 8,
                      _UNIT_Map_CompareEqual, _UNIT_Map_HashDirect,
                      NULL, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_LocalVariables_Clear(&locals);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&translation->symbols, context, 8))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_Map_Clear(&translation->strings);
        _UNIT_LocalVariables_Clear(&locals);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&translation->blocks, context, 16,
                                      _UNIT_BasicBlock_Free))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_SizeMap_Clear(&translation->symbols);
        _UNIT_Map_Clear(&translation->strings);
        _UNIT_LocalVariables_Clear(&locals);
        return UNIT_FAIL;
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

    #define EMIT_EMPTY(inst)                                                                            \
        if (UNIT_FAILED(emit_machine_instruction(context, INSTRUCTIONS(), inst, NULL, NULL, NULL))) {   \
            goto error;                                                                                 \
        }

    #define EMIT_DEST(inst, dest)                                                                       \
        if (UNIT_FAILED(emit_machine_instruction(context, INSTRUCTIONS(), inst, dest, NULL, NULL))) {   \
            goto error;                                                                                 \
        }

    #define EMIT_DEST_ONE(inst, dest, arg1)                                                             \
        if (UNIT_FAILED(emit_machine_instruction(context, INSTRUCTIONS(), inst, dest, arg1, NULL))) {   \
            goto error;                                                                                 \
        }

    #define EMIT_DEST_TWO(inst, dest, arg1, arg2)                                                       \
        if (UNIT_FAILED(emit_machine_instruction(context, INSTRUCTIONS(), inst, dest, arg1, arg2))) {   \
            goto error;                                                                                 \
        }
    #define CREATE_DESTINATION(name)                                                                \
        _UNIT_MachineItem *name = create_new_location(translation, CURRENT_BLOCK(), UNIQUE_ID());   \
        if (name == NULL) {                                                                         \
            goto error;                                                                             \
        }                                                                                           \
        PUSH_ITEM(name);

    START_NEW_BLOCK();

    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions, index);
        switch (operation->instruction) {
            case UNIT_OP_LOAD_CONSTANT_INTEGER: {
                PUSH_NEW(CONSTANT, operation->argument);
                break;
            }

            case UNIT_OP_STORE_LOCAL: {
                UNIT_Size location_id = UNIQUE_ID();
                if (UNIT_FAILED(create_new_local(&locals, operation->argument, location_id))) {
                    return UNIT_FAIL;
                }

                _UNIT_MachineItem *location = create_new_location(translation, CURRENT_BLOCK(),
                                                                  location_id);
                if (location == NULL) {
                    goto error;
                }
                POP_TO_VAR(item);
                EMIT_DEST_ONE(_UNIT_I_MOVE, location, item);
                break;
            }

            case UNIT_OP_LOAD_LOCAL: {
                _UNIT_LocalState *local_state = get_local(&locals, operation->argument);
                if (local_state->stack_slot == -1) {
                    PUSH_NEW(LOCATION, local_state->location_id);
                } else {
                    _UNIT_MachineItem *slot = new_machine_item(translation, MEMORY,
                                                               local_state->stack_slot, NULL);
                    CREATE_DESTINATION(destination);
                    EMIT_DEST_ONE(_UNIT_I_MOVE, destination, slot);
                }
                break;
            }

            case UNIT_OP_ADD: {
                POP_TO_VAR(left);
                POP_TO_VAR(right);

                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_ADD, destination, left, right);
                break;
            }

            case UNIT_OP_CALL_NAME: {
                ARGUMENT_TO_ITEM(symbol, CONSTANT);
                symbol->hint = _UNIT_Vector_GET(&procedure->_symbols, operation->argument);
                POP_TO_VAR(args);
                if (args->type != CALL_ARGS) {
                    // TODO: Display machine item here
                    _UNIT_SetError(context, UNIT_ERROR_INVALID_USAGE,
                                   "CALL_NAME popped item of non-args type");
                    goto error;
                }

                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_CALL_SYMBOL, destination, symbol, args);
                break;
            }

            case UNIT_OP_LOAD_CONSTANT_STRING: {
                const char *text = _UNIT_Vector_GET(&procedure->_global_strings, operation->argument);
                assert(text != NULL);
                _UNIT_MachineItem *result = new_machine_item(translation, CONSTANT,
                                                             operation->argument, text);
                if (result == NULL) {
                    goto error;
                }
                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_LOAD_STRING, destination, result);
                break;
            }

            case UNIT_OP_EXIT: {
                POP_TO_VAR(exit_code);
                EMIT_DEST(_UNIT_I_EXIT, exit_code);
                break;
            }

            case UNIT_OP_POP_TOP: {
                POP_TO_VAR(_unused);
                break;
            }

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
                _UNIT_MachineItem *args = _UNIT_Alloc(context, sizeof(_UNIT_MachineItem));
                if (args == NULL) {
                    _UNIT_Vector_Free(vector);
                    goto error;
                }
                args->call_args = vector;
                args->type = CALL_ARGS;
                args->hint = NULL;
                attach_item_to_translation(translation, args);
                PUSH_ITEM(args);
                break;
            }

            case UNIT_OP_RETURN_VALUE: {
                POP_TO_VAR(value);
                EMIT_DEST(_UNIT_I_RETURN_VALUE, value);
                START_NEW_BLOCK();
                break;
            }
            case UNIT_OP_COMPARE: {
                POP_TO_VAR(left);
                POP_TO_VAR(right);
                UNIT_ComparisonType type = operation->argument;
                CREATE_DESTINATION(destination);

                destination->type = COMPARISON;
                destination->comparison.type = type;
                destination->comparison.left = left;
                destination->comparison.right = right;
                // We intentionally don't emit any instructions here to allow
                // for a jump to fuse it the comparison.
                break;
            }

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

                EMIT_DEST(_UNIT_I_JUMP, item);
                ADD_BLOCK_SUCCESSOR(CURRENT_BLOCK(), label->_block);
                START_NEW_BLOCK();
                break;
            }

            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                POP_TO_VAR(value);
                if (value->type != COMPARISON) {
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
                    case UNIT_COMPARE_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_NOT_EQUAL : _UNIT_I_JUMP_IF_EQUAL;
                        break;
                    case UNIT_COMPARE_NOT_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_EQUAL : _UNIT_I_JUMP_IF_NOT_EQUAL;
                        break;
                    case UNIT_COMPARE_GREATER_THAN:
                        fused = invert ? _UNIT_I_JUMP_IF_LESS_EQUAL : _UNIT_I_JUMP_IF_GREATER;
                        break;
                    case UNIT_COMPARE_LESS_THAN:
                        fused = invert ? _UNIT_I_JUMP_IF_GREATER_EQUAL : _UNIT_I_JUMP_IF_LESS;
                        break;
                    case UNIT_COMPARE_LESS_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_GREATER : _UNIT_I_JUMP_IF_LESS_EQUAL;
                        break;
                    case UNIT_COMPARE_GREATER_EQUAL:
                        fused = invert ? _UNIT_I_JUMP_IF_LESS : _UNIT_I_JUMP_IF_GREATER_EQUAL;
                        break;
                    default:
                        _UNIT_Unreachable();
                }

                EMIT_DEST_TWO(fused, jump_target,
                              value->comparison.left,
                              value->comparison.right);
                _UNIT_BasicBlock *saved_block = CURRENT_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, label->_block);
                START_NEW_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, CURRENT_BLOCK());
                break;
            }

            case UNIT_OP_ADDRESS_OF: {
                _UNIT_LocalState *local_state = get_local(&locals, operation->argument);
                // When a variable has its address used, then we can no longer
                // use it in registers.
                if (local_state->stack_slot == -1) {
                    local_state->stack_slot = locals.next_stack_slot++;
                    _UNIT_MachineItem *variable = new_machine_item(translation, LOCATION,
                                                                   local_state->location_id, NULL);
                    if (variable == NULL) {
                        goto error;
                    }

                    _UNIT_MachineItem *slot = new_machine_item(translation, MEMORY,
                                                               local_state->stack_slot, NULL);
                    EMIT_DEST_ONE(_UNIT_I_MOVE, slot, variable);
                }

                _UNIT_MachineItem *value;
                if (local_state->stack_slot == -1) {
                    value = new_machine_item(translation, LOCATION,
                                             local_state->location_id, NULL);
                } else {
                    value = new_machine_item(translation, MEMORY,
                                             local_state->stack_slot, NULL);
                }

                if (value == NULL) {
                    goto error;
                }

                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_ADDRESS_OF, destination, value);
                break;
            }

            default:
                break;
        }
    }

    _UNIT_Vector_Clear(&stack);
    _UNIT_LocalVariables_Clear(&locals);
    // This is so we can determine the size of the frame later
    translation->num_memory_slots = locals.next_stack_slot;
    return analyze_liveness(translation);
error:
    _UNIT_Vector_Clear(&stack);
    _UNIT_LocalVariables_Clear(&locals);
    _UNIT_SizeMap_Clear(&translation->symbols);
    _UNIT_Map_Clear(&translation->strings);
    _UNIT_Vector_Clear(&translation->blocks);
    return UNIT_FAIL;
}

void
_UNIT_Translation_Clear(_UNIT_Translation *translation)
{
    assert(translation != NULL);
    _UNIT_Map_Clear(&translation->strings);
    _UNIT_SizeMap_Clear(&translation->symbols);
    _UNIT_Vector_Clear(&translation->blocks);

    _UNIT_MachineItem *head = translation->item_list_head;
    while (head != NULL) {
        _UNIT_MachineItem *next = head->next;
        if (head->type == CALL_ARGS) {
            _UNIT_Vector_Free(head->call_args);
        }
        _UNIT_Dealloc(translation->context, head);
        head = next;
    }
}
