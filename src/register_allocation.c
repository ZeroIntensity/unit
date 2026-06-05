#include <unit/internal/basic_block.h>
#include <unit/internal/register_allocation.h>
#include <unit/internal/size_vector.h>

typedef struct {
    _UNIT_Translation *translation;
    _UNIT_SizeMap assignments;
    _UNIT_SizeMap spills;
    _UNIT_StackFrame *stack_frame;
    int8_t num_registers;
} _UNIT_RegisterAllocator;

static UNIT_Status
_UNIT_RegisterAllocator_Init(_UNIT_RegisterAllocator *allocator,
                             _UNIT_Translation *translation,
                             _UNIT_CompileContext *compile_context,
                             int8_t num_registers)
{
    assert(allocator != NULL);
    assert(translation != NULL);
    assert(compile_context != NULL);
    assert(num_registers > 0);
    UNIT_Context *context = translation->context;
    assert(context != NULL);
    if (UNIT_FAILED(_UNIT_SizeMap_Init(&allocator->assignments, context,
                                       num_registers))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&allocator->spills, context, 8))) {
        _UNIT_SizeMap_Clear(&allocator->assignments);
        return UNIT_FAIL;
    }

    allocator->translation = translation;
    allocator->stack_frame = &compile_context->stack_frame;
    allocator->num_registers = num_registers;

    return UNIT_OK;
}

static void
_UNIT_RegisterAllocator_Clear(_UNIT_RegisterAllocator *allocator)
{
    assert(allocator != NULL);
    _UNIT_SizeMap_Clear(&allocator->assignments);
    _UNIT_SizeMap_Clear(&allocator->spills);
}

static UNIT_Status
allocate_registers_for_block(_UNIT_RegisterAllocator *allocator, _UNIT_BasicBlock *block)
{
    assert(allocator != NULL);
    assert(block != NULL);
    _UNIT_SizeMap *assignments = &allocator->assignments;

    _UNIT_SizeSet registers_in_use;
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&registers_in_use, block->context, allocator->num_registers))) {
        return UNIT_FAIL;
    }

    _UNIT_SizeSet_ITER(&block->liveness.alive_at_start, location);
        UNIT_Size register_id;
        if (!UNIT_FAILED(_UNIT_SizeMap_Get(assignments, location, &register_id))) {
            if (UNIT_FAILED(_UNIT_SizeSet_Add(&registers_in_use, register_id))) {
                goto error;
            }
        }
    _UNIT_SizeSet_END_ITER();

    UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(&block->instructions,
                                                             index);

        if (operation->instruction == _UNIT_I_JUMP_LABEL
            || operation->destination == NULL
            || operation->destination->type != _UNIT_TYPE_LOCATION) {
            continue;
        }

        // Assign register to destination if it's a new location
        UNIT_Size location = operation->destination->value;
        UNIT_Size existing;


        if (!UNIT_FAILED(_UNIT_SizeMap_Get(assignments, location, &existing))) {
            // Location already assigned a register
            continue;
        }

        // Find a free register
        for (UNIT_Size register_id = 0; register_id < allocator->num_registers; ++register_id) {
            if (!_UNIT_SizeSet_Contains(&registers_in_use, register_id)) {
                if (UNIT_FAILED(_UNIT_SizeMap_Set(assignments, location, register_id))) {
                    goto error;
                }

                if (UNIT_FAILED(_UNIT_SizeSet_Add(&registers_in_use, register_id))) {
                    goto error;
                }
                break;
            }
        }
    }

    _UNIT_SizeSet_Clear(&registers_in_use);
    return UNIT_OK;
error:
    _UNIT_SizeSet_Clear(&registers_in_use);
    return UNIT_FAIL;
}

static UNIT_Status
potentially_rewrite_item(_UNIT_RegisterAllocator *allocator,
                         _UNIT_MachineItem *item)
{
    assert(allocator != NULL);
    if (item == NULL) {
        return UNIT_OK;
    }

    _UNIT_Translation *translation = allocator->translation;
    assert(translation != NULL);
    _UNIT_SizeMap *assignments = &allocator->assignments;
    _UNIT_SizeMap *spills = &allocator->spills;

    assert(item->type != _UNIT_TYPE_COMPARISON);
    if (item->type == _UNIT_TYPE_LOCATION) {
        UNIT_Size register_id;
        UNIT_Size slot_id;
        if (!UNIT_FAILED(_UNIT_SizeMap_Get(assignments, item->value,
                                           &register_id))) {
            item->type = _UNIT_TYPE_REGISTER;
            item->value = register_id;
        } else if (!UNIT_FAILED(_UNIT_SizeMap_Get(spills, item->value, &slot_id))) {
            item->type = _UNIT_TYPE_MEMORY;
            item->value = slot_id;
        } else {
            // Spill :(
            item->type = _UNIT_TYPE_MEMORY;
            UNIT_Size new_slot_id = _UNIT_StackFrame_AllocateSlotID(allocator->stack_frame);
            if (UNIT_FAILED(_UNIT_SizeMap_Set(spills, item->value, new_slot_id))) {
                return UNIT_FAIL;
            }

            item->value = new_slot_id;
        }
    } else if (item->type == _UNIT_TYPE_CALL_ARGS) {
        UNIT_Size count = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size index = 0; index < count; ++index) {
            _UNIT_MachineItem *arg = _UNIT_Vector_GET(item->call_args, index);
            assert(arg != NULL);
            if (UNIT_FAILED(potentially_rewrite_item(allocator, arg))) {
                return UNIT_FAIL;
            }
        }
    }

    return UNIT_OK;
}

static UNIT_Status
rewrite_block_locations(_UNIT_RegisterAllocator *allocator,
                        _UNIT_BasicBlock *block)
{
    assert(allocator != NULL);
    assert(block != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(&block->instructions,
                                                             index);
#define POTENTIALLY_REWRITE(name)                                                   \
        if (UNIT_FAILED(potentially_rewrite_item(allocator, operation->name))) {    \
            return UNIT_FAIL;                                                       \
        }

        POTENTIALLY_REWRITE(destination);
        POTENTIALLY_REWRITE(argument_1);
        POTENTIALLY_REWRITE(argument_2);

#undef POTENTIALLY_REWRITE
    }

    return UNIT_OK;
}

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    _UNIT_CompileContext *compile_context,
                                    int8_t num_registers)
{
    assert(translation != NULL);
    assert(compile_context != NULL);
    assert(num_registers > 0);
    _UNIT_RegisterAllocator allocator;
    if (UNIT_FAILED(_UNIT_RegisterAllocator_Init(&allocator, translation, compile_context, num_registers))) {
        return UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        if (UNIT_FAILED(allocate_registers_for_block(&allocator, block))) {
            _UNIT_RegisterAllocator_Clear(&allocator);
            return UNIT_FAIL;
        }
    }

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        assert(block != NULL);
        if (UNIT_FAILED(rewrite_block_locations(&allocator, block))) {
            _UNIT_RegisterAllocator_Clear(&allocator);
            return UNIT_FAIL;
        }
    }

    _UNIT_RegisterAllocator_Clear(&allocator);
    return UNIT_OK;
}
