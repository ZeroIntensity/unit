#include <stdio.h>
#include <stdlib.h>

#include <unit/errors.h>

#include <unit/internal/allocation.h>
#include <unit/internal/basic_block.h>
#include <unit/internal/size_vector.h>
#include <unit/internal/translation.h>

#define NAME(name) case name: return #name

const char *
instruction_name(UNIT_Instruction instruction)
{
    switch (instruction) {
        NAME(UNIT_OP_LOAD_INTEGER);
        NAME(UNIT_OP_LOAD_STRING);
        NAME(_UNIT_OP_LOAD_LOCAL_NAME);
        NAME(_UNIT_OP_STORE_LOCAL_NAME);
        NAME(UNIT_OP_LOAD_LOCAL);
        NAME(UNIT_OP_STORE_LOCAL);
        NAME(UNIT_OP_ADDRESS_OF);
        NAME(UNIT_OP_ADD);
        NAME(UNIT_OP_SUBTRACT);
        NAME(UNIT_OP_MULTIPLY);
        NAME(UNIT_OP_DIVIDE);
        NAME(UNIT_OP_MODULO);
        NAME(_UNIT_OP_JUMP_MARKER);
        NAME(UNIT_OP_JUMP_TO);
        NAME(UNIT_OP_JUMP_IF_FALSE);
        NAME(UNIT_OP_JUMP_IF_TRUE);
        NAME(UNIT_OP_PREPARE_CALL);
        NAME(UNIT_OP_CALL_NAME);
        NAME(UNIT_OP_POP_TOP);
        NAME(UNIT_OP_RETURN_VALUE);
        NAME(UNIT_OP_EXIT);
        NAME(UNIT_OP_COMPARE_EQUAL);
        NAME(UNIT_OP_COMPARE_NOT_EQUAL);
        NAME(UNIT_OP_COMPARE_GREATER);
        NAME(UNIT_OP_COMPARE_GREATER_EQUAL);
        NAME(UNIT_OP_COMPARE_LESS);
        NAME(UNIT_OP_COMPARE_LESS_EQUAL);
        NAME(UNIT_OP_COPY);
        NAME(UNIT_OP_SWAP);
        NAME(UNIT_OP_DEREFERENCE);
        NAME(UNIT_OP_WRITE_THROUGH);
    }
    fprintf(stderr, "unknown machine instruction\n");
    abort();
}

const char *
machine_instruction_name(_UNIT_MachineInstruction machine_instruction)
{
    switch (machine_instruction) {
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
        NAME(_UNIT_I_ADD);
        NAME(_UNIT_I_SUB);
        NAME(_UNIT_I_MUL);
        NAME(_UNIT_I_DIV);
        NAME(_UNIT_I_MOD);
        NAME(_UNIT_I_DEREFERENCE);
        NAME(_UNIT_I_WRITE_THROUGH);
    }
    fprintf(stderr, "unknown machine instruction\n");
    abort();
}

#undef NAME

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
                         _UNIT_MachineItem *destination,
                         _UNIT_MachineItem *arg1,
                         _UNIT_MachineItem *arg2)
{
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

void
print_machine_item(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    if (item->type == _UNIT_TYPE_CONSTANT) {
        printf("%d", item->value);
    } else if (item->type == _UNIT_TYPE_LOCATION) {
        printf("loc_%d", item->value);
    } else if (item->type == _UNIT_TYPE_CALL_ARGS) {
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
    } else if (item->type == _UNIT_TYPE_COMPARISON) {
        print_machine_item(item->comparison.left);
        switch (item->comparison.type) {
            case UNIT_OP_COMPARE_EQUAL:
                printf(" == ");
                break;
            case UNIT_OP_COMPARE_NOT_EQUAL:
                printf(" != ");
                break;
            case UNIT_OP_COMPARE_LESS:
                printf(" < ");
                break;
            case UNIT_OP_COMPARE_LESS_EQUAL:
                printf(" <= ");
                break;
            case UNIT_OP_COMPARE_GREATER:
                printf(" > ");
                break;
            case UNIT_OP_COMPARE_GREATER_EQUAL:
                printf(" >= ");
                break;
            default:
                _UNIT_Unreachable();
        }
        print_machine_item(item->comparison.right);
    } else if (item->type == _UNIT_TYPE_MEMORY) {
        printf("stack_address_%d", item->value);
    } else {
        assert(item->type == _UNIT_TYPE_REGISTER);
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
_UNIT_Translation_PrintInstructions(const _UNIT_Translation *translation)
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
                             "stack underflow at %s",
                             instruction_name(operation->instruction));
        return NULL;
    }
    _UNIT_MachineItem *result = _UNIT_Vector_Pop(stack);
    assert(result != NULL);

    if (result->type == _UNIT_TYPE_LOCATION) {
        if (UNIT_FAILED(_UNIT_SizeSet_Add(&block->liveness.used_locations,
                                          result->value))) {
            return NULL;
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
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

_UNIT_LocalState *
create_new_local(_UNIT_LocalVariables *locals, int32_t name, UNIT_Size id)
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

    _UNIT_LocalState *local_state = _UNIT_Alloc(context, sizeof(_UNIT_LocalState));
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

_UNIT_LocalState *
get_local(_UNIT_LocalVariables *locals, int32_t name)
{
    assert(locals != NULL);
    _UNIT_LocalState *local_state = _UNIT_Map_Get(&locals->locals_map, &name);
    return local_state;
}

void
_UNIT_LocalVariables_Clear(_UNIT_LocalVariables *locals)
{
    _UNIT_Map_Clear(&locals->locals_map);
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

    _UNIT_LocalVariables locals;
    if (UNIT_FAILED(_UNIT_LocalVariables_Init(&locals, context))) {
        _UNIT_Vector_Clear(&stack);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Map_Init(&translation->strings, context, 8,
                      _UNIT_Map_CompareEqual, _UNIT_Map_HashDirect,
                      NULL, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_LocalVariables_Clear(&locals);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&translation->symbols, context, 8))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_Map_Clear(&translation->strings);
        _UNIT_LocalVariables_Clear(&locals);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&translation->blocks, context, 16,
                                      _UNIT_BasicBlock_Free))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_SizeMap_Clear(&translation->symbols);
        _UNIT_Map_Clear(&translation->strings);
        _UNIT_LocalVariables_Clear(&locals);
        return _UNIT_FAIL;
    }


    _UNIT_SizeSet address_taken_locals;
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&address_taken_locals, context, 8))) {
        _UNIT_Vector_Clear(&stack);
        _UNIT_SizeMap_Clear(&translation->symbols);
        _UNIT_Map_Clear(&translation->strings);
        _UNIT_LocalVariables_Clear(&locals);
        _UNIT_Vector_Clear(&translation->blocks);
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);

    // TODO: This is ugly
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

    #define EMIT_EMPTY(inst)                                                                            \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, NULL, NULL, NULL))) {  \
            goto error;                                                                                 \
        }

    #define EMIT_DEST(inst, dest)                                                                       \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, dest, NULL, NULL))) {  \
            goto error;                                                                                 \
        }

    #define EMIT_DEST_ONE(inst, dest, arg1)                                                             \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, dest, arg1, NULL))) {  \
            goto error;                                                                                 \
        }

    #define EMIT_DEST_TWO(inst, dest, arg1, arg2)                                                       \
        if (UNIT_FAILED(emit_machine_instruction(context, CURRENT_BLOCK(), inst, dest, arg1, arg2))) {  \
            goto error;                                                                                 \
        }
    #define CREATE_DESTINATION(name)                                                                \
        _UNIT_MachineItem *name = create_new_location(translation, CURRENT_BLOCK(), UNIQUE_ID());   \
        if (name == NULL) {                                                                         \
            goto error;                                                                             \
        }                                                                                           \
        PUSH_ITEM(name);

    #define INST_ASSERT(condition, message)                                         \
        if (!(condition)) {                                                         \
            _UNIT_SetErrorFormat(_block->context, UNIT_ERROR_INVALID_USAGE,         \
                                 "error at %s (index %d): " message,                \
                                 instruction_name(operation->instruction), index);  \
            goto error;                                                             \
        }

    #define INST_ASSERT_FMT(condition, message, ...)                                \
        if (!(condition)) {                                                         \
            _UNIT_SetErrorFormat(_block->context, UNIT_ERROR_INVALID_USAGE,         \
                                 "error at %s (index %d): " message,                \
                                 instruction_name(operation->instruction), index,   \
                                 __VA_ARGS__);                                      \
            goto error;                                                             \
        }
    #define INST_ASSERT_OPARG(condition, message) INST_ASSERT_FMT(condition, message, operation->argument)

    START_NEW_BLOCK();

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions, index);
        switch (operation->instruction) {
            case UNIT_OP_LOAD_INTEGER: {
                PUSH_NEW(_UNIT_TYPE_CONSTANT, operation->argument);
                break;
            }

            case _UNIT_OP_STORE_LOCAL_NAME:
            case UNIT_OP_STORE_LOCAL: {
                const char *hint = NULL;
                if (operation->instruction == _UNIT_OP_STORE_LOCAL_NAME) {
                    hint = _UNIT_Vector_GET(&procedure->_local_variables,
                                            operation->argument);
                }

                UNIT_Size location_id = UNIQUE_ID();
                _UNIT_LocalState *local_state = create_new_local(&locals,
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
                EMIT_DEST_ONE(_UNIT_I_MOVE, location, item);

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
                    EMIT_DEST_ONE(_UNIT_I_MOVE, slot, location);
                }
                break;
            }

            case _UNIT_OP_LOAD_LOCAL_NAME:
            case UNIT_OP_LOAD_LOCAL: {
                _UNIT_LocalState *local_state = get_local(&locals, operation->argument);
                INST_ASSERT_OPARG(local_state != NULL, "local variable %d not assigned");

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
                    EMIT_DEST_ONE(_UNIT_I_MOVE, location, slot);
                }

                PUSH_ITEM(location);
                break;
            }

            case UNIT_OP_ADDRESS_OF: {
                _UNIT_LocalState *local_state = get_local(&locals, operation->argument);
                INST_ASSERT_OPARG(local_state != NULL, "local variable %d not assigned");
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

            case UNIT_OP_CALL_NAME: {
                ARGUMENT_TO_ITEM(symbol, _UNIT_TYPE_CONSTANT);
                symbol->hint = _UNIT_Vector_GET(&procedure->_symbols, operation->argument);
                POP_TO_VAR(args);
                INST_ASSERT(args->type == _UNIT_TYPE_CALL_ARGS, "got non-args item off stack");
                CREATE_DESTINATION(destination);
                EMIT_DEST_TWO(_UNIT_I_CALL_SYMBOL, destination, symbol, args);
                break;
            }

            case UNIT_OP_LOAD_STRING: {
                const char *text = _UNIT_Vector_GET(&procedure->_global_strings, operation->argument);
                INST_ASSERT_OPARG(text != NULL, "%d is not a known string ID");
                _UNIT_MachineItem *result = new_machine_item(translation, _UNIT_TYPE_CONSTANT,
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

            case UNIT_OP_RETURN_VALUE: {
                POP_TO_VAR(value);
                EMIT_DEST(_UNIT_I_RETURN_VALUE, value);
                START_NEW_BLOCK();
                break;
            }

            case UNIT_OP_COMPARE_EQUAL:
            case UNIT_OP_COMPARE_NOT_EQUAL:
            case UNIT_OP_COMPARE_GREATER:
            case UNIT_OP_COMPARE_GREATER_EQUAL:
            case UNIT_OP_COMPARE_LESS:
            case UNIT_OP_COMPARE_LESS_EQUAL: {
                POP_TO_VAR(left);
                POP_TO_VAR(right);
                CREATE_DESTINATION(destination);

                destination->type = _UNIT_TYPE_COMPARISON;
                destination->comparison.type = operation->instruction;
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

                EMIT_DEST_TWO(fused, jump_target,
                              value->comparison.left,
                              value->comparison.right);
                _UNIT_BasicBlock *saved_block = CURRENT_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, label->_block);
                START_NEW_BLOCK();
                ADD_BLOCK_SUCCESSOR(saved_block, CURRENT_BLOCK());
                break;
            }

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

            case UNIT_OP_COPY: {
                UNIT_Size index = _UNIT_Vector_SIZE(&stack) - operation->argument - 1;
                assert(index >= 0);
                _UNIT_MachineItem *item = _UNIT_Vector_GET(&stack, index);
                // XXX: Might cause problems without a copy?
                PUSH_ITEM(item);
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

            case UNIT_OP_DEREFERENCE: {
                POP_TO_VAR(address);
                CREATE_DESTINATION(destination);
                EMIT_DEST_ONE(_UNIT_I_DEREFERENCE, destination, address);
                break;
            }

            case UNIT_OP_WRITE_THROUGH: {
                POP_TO_VAR(value);
                POP_TO_VAR(address);
                EMIT_DEST_ONE(_UNIT_I_WRITE_THROUGH, address, value);
                break;
            }
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
    return _UNIT_FAIL;
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
        if (head->type == _UNIT_TYPE_CALL_ARGS) {
            _UNIT_Vector_Free(head->call_args);
        }
        _UNIT_Dealloc(translation->context, head);
        head = next;
    }
}
