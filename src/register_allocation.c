#include <unit/internal/translation.h>
#include <unit/internal/size_vector.h>


static UNIT_Status
allocate_registers_for_block(_UNIT_BasicBlock *block,
                             _UNIT_SizeMap *assignments, int8_t num_registers)
{
    assert(assignments != NULL);
    assert(num_registers >= 0);
    assert(block != NULL);

    _UNIT_SizeSet registers_in_use;
    if (UNIT_FAILED(_UNIT_SizeSet_Init(&registers_in_use, block->context, num_registers))) {
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
            || operation->destination->type != LOCATION) {
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
        for (UNIT_Size register_id = 0; register_id < num_registers; ++register_id) {
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

static void
potentially_rewrite_item(_UNIT_Translation *translation,
                         _UNIT_SizeMap *assignments, _UNIT_MachineItem *item)
{
    assert(translation != NULL);
    assert(assignments != NULL);
    if (item == NULL) {
        return;
    }

    assert(item->type != COMPARISON);
    if (item->type == LOCATION) {
        UNIT_Size register_id;
        if (!UNIT_FAILED(_UNIT_SizeMap_Get(assignments, item->value,
                                           &register_id))) {
            item->type = REGISTER;
            item->value = register_id;
        } else {
            // Spill :(
            item->type = MEMORY;
            item->value = ++translation->num_memory_slots;
        }
    } else if (item->type == CALL_ARGS) {
        UNIT_Size count = _UNIT_Vector_SIZE(item->call_args);
        for (UNIT_Size i = 0; i < count; ++i) {
            potentially_rewrite_item(translation, assignments,
                                     _UNIT_Vector_GET(item->call_args, i));
        }
    }
}

static void
rewrite_block_locations(_UNIT_Translation *translation,
                        _UNIT_BasicBlock *block, _UNIT_SizeMap *assignments)
{
    assert(block != NULL);
    assert(assignments != NULL);
    assert(translation != NULL);
    UNIT_Size size = _UNIT_Vector_SIZE(&block->instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_MachineOperation *operation = _UNIT_Vector_GET(&block->instructions,
                                                             index);
        potentially_rewrite_item(translation, assignments, operation->destination);
        potentially_rewrite_item(translation, assignments, operation->argument_1);
        potentially_rewrite_item(translation, assignments, operation->argument_2);
    }
}

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    int8_t num_registers)
{
    assert(translation != NULL);
    assert(num_registers > 1);
    // Maps location ID -> register ID
    _UNIT_SizeMap assignments;
    if (UNIT_FAILED(_UNIT_SizeMap_Init(&assignments, translation->context,
                                       num_registers))) {
        return UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&translation->blocks);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        if (UNIT_FAILED(allocate_registers_for_block(block,
                                                     &assignments,
                                                     num_registers))) {
            return UNIT_FAIL;
        }
    }

    // Now rewrite all LOCATION items
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, index);
        assert(block != NULL);
        rewrite_block_locations(translation, block, &assignments);
    }

    _UNIT_SizeMap_Clear(&assignments);
    return UNIT_OK;
}
