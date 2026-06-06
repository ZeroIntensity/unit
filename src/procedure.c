#include <stdbool.h>

#include <unit/procedure.h>

#include <unit/internal/allocation.h>

UNIT_Status
UNIT_Procedure_Init(UNIT_Procedure *procedure,
                    UNIT_Context *context, const char *name)
{
    assert(procedure != NULL);
    assert(context != NULL);
    assert(name != NULL);
    procedure->context = context;
    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_instructions,
                                      context, 64, _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_global_strings,
                                      context, 4, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&procedure->_instructions);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_jump_labels,
                                      context, 4, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_symbols,
                                      context, 4, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        _UNIT_Vector_Clear(&procedure->_jump_labels);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_local_variables,
                                      context, 8, _UNIT_Dealloc))) {
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        _UNIT_Vector_Clear(&procedure->_jump_labels);
        _UNIT_Vector_Clear(&procedure->_symbols);
        return _UNIT_FAIL;
    }

    procedure->name = name;
    return _UNIT_OK;
}

void
UNIT_Procedure_Clear(UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    _UNIT_Vector_Clear(&procedure->_instructions);
    _UNIT_Vector_Clear(&procedure->_global_strings);
    _UNIT_Vector_Clear(&procedure->_jump_labels);
    _UNIT_Vector_Clear(&procedure->_symbols);
    _UNIT_Vector_Clear(&procedure->_local_variables);
}

UNIT_Procedure *
UNIT_Procedure_New(UNIT_Context *context, UNIT_Procedure *procedure,
                   const char *name)
{
    _UNIT_Structure_NEW_IMPL(UNIT_Procedure, context, name);
}

_UNIT_Structure_DEFINE_PUBLIC_FREE(UNIT_Procedure);

UNIT_Status
UNIT_Procedure_AddOperation(UNIT_Procedure *procedure,
                            UNIT_Instruction instruction,
                            int32_t argument)
{
    assert(procedure != NULL);
    _UNIT_Operation *operation = _UNIT_Alloc(procedure->context,
                                             sizeof(_UNIT_Operation));
    if (operation == NULL) {
        return _UNIT_FAIL;
    }
    operation->instruction = instruction;
    operation->argument = argument;
    return _UNIT_Vector_Append(&procedure->_instructions,
                               operation);
}

UNIT_JumpLabel *
UNIT_Procedure_CreateJumpLabel(UNIT_Procedure *procedure, const char *name)
{
    assert(procedure != NULL);
    assert(name != NULL);
    UNIT_JumpLabel *label = _UNIT_Alloc(procedure->context, sizeof(UNIT_JumpLabel));
    if (label == NULL) {
        return NULL;
    }
    label->name = name;
    label->id = _UNIT_Vector_SIZE(&procedure->_jump_labels);
    label->_block = NULL;
    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_jump_labels,
                                        label))) {
        return NULL;
    }

    return label;
}

UNIT_Status
UNIT_Procedure_UseLabel(UNIT_Procedure *procedure,
                         UNIT_JumpLabel *jump_label)
{
    assert(procedure != NULL);
    assert(jump_label != NULL);

    if (UNIT_FAILED(UNIT_Procedure_AddOperation(procedure, _UNIT_OP_JUMP_MARKER, jump_label->id))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_AddJump(UNIT_Procedure *procedure,
                       UNIT_Instruction instruction,
                       UNIT_JumpLabel *jump_label)
{
    assert(procedure != NULL);
    assert(jump_label != NULL);
    return UNIT_Procedure_AddOperation(procedure, instruction, jump_label->id);
}

UNIT_Status
UNIT_Procedure_AddCall(UNIT_Procedure *procedure,
                       const char *name,
                       UNIT_Size num_arguments)
{
    assert(procedure != NULL);
    assert(name != NULL);
    char *copied = _UNIT_StrDup(procedure->context, name);
    if (copied == NULL) {
        return _UNIT_FAIL;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&procedure->_symbols);
    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_symbols,
                                        copied))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(UNIT_Procedure_AddOperation(procedure,
                                                UNIT_OP_PREPARE_CALL,
                                                num_arguments))) {
        return _UNIT_FAIL;
    }

    return UNIT_Procedure_AddOperation(procedure, UNIT_OP_CALL_NAME, index);
}

UNIT_Status
UNIT_Procedure_AddStringLoad(UNIT_Procedure *procedure, const char *str)
{
    assert(procedure != NULL);
    assert(str != NULL);
    char *copied = _UNIT_StrDup(procedure->context, str);
    if (copied == NULL) {
        return _UNIT_FAIL;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&procedure->_global_strings);
    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_global_strings,
                                        copied))) {
        return _UNIT_FAIL;
    }

    return UNIT_Procedure_AddOperation(procedure, UNIT_OP_LOAD_STRING,
                                       index);
}

UNIT_Status
UNIT_Procedure_AddStoreLocal(UNIT_Procedure *procedure, const char *name,
                             UNIT_Local *local_ptr)
{
    UNIT_Size index = _UNIT_Vector_SIZE(&procedure->_local_variables);
    char *copy = _UNIT_StrDup(procedure->context, name);
    if (copy == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_local_variables,
                                        copy))) {
        return _UNIT_FAIL;
    }

    local_ptr->id = index;

    return UNIT_Procedure_AddOperation(procedure, _UNIT_OP_STORE_LOCAL_NAME, index);
}

UNIT_Status
UNIT_Procedure_AddLoadLocal(UNIT_Procedure *procedure, UNIT_Local local)
{
    assert(procedure != NULL);
    assert(_UNIT_Vector_GET(&procedure->_local_variables, local.id) != NULL);
    return UNIT_Procedure_AddOperation(procedure, _UNIT_OP_LOAD_LOCAL_NAME, local.id);
}
