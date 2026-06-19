#include <stdbool.h>

#include <unit/procedure.h>

#include <unit/internal/allocation.h>

void
free_jump_label(UNIT_Context *context, void *ptr)
{
    UNIT_JumpLabel *label = (UNIT_JumpLabel *)ptr;
    _UNIT_Dealloc(context, label->name);
    _UNIT_Dealloc(context, ptr);
}

UNIT_Status
UNIT_Procedure_Init(UNIT_Procedure *procedure,
                    UNIT_Context *context, const char *name)
{
    assert(procedure != NULL);
    assert(context != NULL);
    assert(name != NULL);
    procedure->context = context;
    procedure->name = _UNIT_StrDup(context, name);
    if (procedure->name == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_instructions,
                                      context, 64, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, procedure->name);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_global_strings,
                                      context, 4, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, procedure->name);
        _UNIT_Vector_Clear(&procedure->_instructions);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_jump_labels,
                                      context, 4, free_jump_label))) {
        _UNIT_Dealloc(context, procedure->name);
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_symbols,
                                      context, 4, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, procedure->name);
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        _UNIT_Vector_Clear(&procedure->_jump_labels);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_local_variables,
                                      context, 8, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, procedure->name);
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        _UNIT_Vector_Clear(&procedure->_jump_labels);
        _UNIT_Vector_Clear(&procedure->_symbols);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&procedure->_subprocedures, context, 4, NULL))) {
        _UNIT_Dealloc(context, procedure->name);
        _UNIT_Vector_Clear(&procedure->_instructions);
        _UNIT_Vector_Clear(&procedure->_global_strings);
        _UNIT_Vector_Clear(&procedure->_jump_labels);
        _UNIT_Vector_Clear(&procedure->_symbols);
        _UNIT_Vector_Clear(&procedure->_local_variables);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

void
UNIT_Procedure_Clear(UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    _UNIT_Dealloc(procedure->context, procedure->name);
    _UNIT_Vector_Clear(&procedure->_instructions);
    _UNIT_Vector_Clear(&procedure->_global_strings);
    _UNIT_Vector_Clear(&procedure->_jump_labels);
    _UNIT_Vector_Clear(&procedure->_symbols);
    _UNIT_Vector_Clear(&procedure->_local_variables);
    _UNIT_Vector_Clear(&procedure->_subprocedures);
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
                            int64_t argument)
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

    label->name = _UNIT_StrDup(procedure->context, name);
    if (label->name == NULL) {
        _UNIT_Dealloc(procedure->context, label);
        return NULL;
    }

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

UNIT_Size
add_symbol(UNIT_Procedure *procedure, const char *name)
{
    assert(procedure != NULL);
    assert(name != NULL);
    char *copied = _UNIT_StrDup(procedure->context, name);
    if (copied == NULL) {
        return -1;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&procedure->_symbols);
    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_symbols,
                                        copied))) {
        return -1;
    }

    return index;
}

UNIT_Status
UNIT_Procedure_AddCallName(UNIT_Procedure *procedure,
                           const char *name,
                           UNIT_Size num_arguments)
{
    assert(procedure != NULL);
    assert(name != NULL);
    if (UNIT_FAILED(UNIT_Procedure_AddOperation(procedure,
                                                UNIT_OP_PREPARE_CALL,
                                                num_arguments))) {
        return _UNIT_FAIL;
    }

    UNIT_Size index = add_symbol(procedure, name);
    if (index == -1) {
        return _UNIT_FAIL;
    }

    return UNIT_Procedure_AddOperation(procedure, UNIT_OP_CALL_NAME, index);
}

UNIT_Status
UNIT_Procedure_AddCallProcedure(UNIT_Procedure *procedure,
                                UNIT_Procedure *target,
                                uint8_t nargs)
{
    assert(procedure != NULL);
    assert(target != NULL);
    assert(nargs >= 0);
    if (procedure == target) {
        return UNIT_Procedure_AddCallName(procedure, procedure->name, nargs);
    }

    if (UNIT_FAILED(UNIT_Procedure_AddOperation(procedure, UNIT_OP_PREPARE_CALL, nargs))) {
        return _UNIT_FAIL;
    }

    char *copied = _UNIT_StrDup(procedure->context, target->name);
    if (copied == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_symbols,
                                        copied))) {
        return _UNIT_FAIL;
    }

    UNIT_Size index = _UNIT_Vector_SIZE(&procedure->_subprocedures);
    if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_subprocedures, target))) {
        return _UNIT_FAIL;
    }

    return UNIT_Procedure_AddOperation(procedure, UNIT_OP_CALL_PROCEDURE, index);
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
UNIT_Procedure_CreateLocal(UNIT_Procedure *procedure, const char *name,
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

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_AddStoreName(UNIT_Procedure *procedure, UNIT_Local local)
{
    assert(procedure != NULL);
    assert(_UNIT_Vector_GET(&procedure->_local_variables, local.id) != NULL);
    assert(local.id >= 0);

    return UNIT_Procedure_AddOperation(procedure, _UNIT_OP_STORE_LOCAL_NAME, local.id);
}

UNIT_Status
UNIT_Procedure_AddLoadName(UNIT_Procedure *procedure, UNIT_Local local)
{
    assert(procedure != NULL);
    assert(_UNIT_Vector_GET(&procedure->_local_variables, local.id) != NULL);
    assert(local.id >= 0);

    return UNIT_Procedure_AddOperation(procedure, _UNIT_OP_LOAD_LOCAL_NAME, local.id);
}


const char *
UNIT_Instruction_GetName(UNIT_Instruction instruction)
{
#define NAME(name) case UNIT_OP_ ##name: return #name
    switch (instruction) {
        NAME(LOAD_STRING);
        NAME(LOAD_INTEGER);

        case _UNIT_OP_LOAD_LOCAL_NAME:
        NAME(LOAD_LOCAL);
        case _UNIT_OP_STORE_LOCAL_NAME:
        NAME(STORE_LOCAL);

        NAME(ADD);
        NAME(SUBTRACT);
        NAME(MULTIPLY);
        NAME(DIVIDE);
        NAME(MODULO);

        case _UNIT_OP_JUMP_MARKER:
            return "_JUMP_MARKER";
        NAME(JUMP_TO);
        NAME(JUMP_IF_FALSE);
        NAME(JUMP_IF_TRUE);

        NAME(EXIT);
        NAME(RETURN_VALUE);
        NAME(LOAD_ARGUMENT);

        NAME(PREPARE_CALL);
        NAME(CALL_NAME);
        NAME(CALL_PROCEDURE);

        NAME(COMPARE_EQUAL);
        NAME(COMPARE_NOT_EQUAL);
        NAME(COMPARE_GREATER);
        NAME(COMPARE_GREATER_EQUAL);
        NAME(COMPARE_LESS);
        NAME(COMPARE_LESS_EQUAL);

        NAME(COPY);
        NAME(SWAP);
        NAME(POP);

        NAME(ADDRESS_OF);
        NAME(READ_BYTES);
        NAME(WRITE_BYTES);

        NAME(CONVERT);
    }
    _UNIT_Unreachable();
}

void
UNIT_Procedure_PrintInstructions(const UNIT_Procedure *procedure, FILE *stream)
{
    assert(procedure != NULL);
    assert(stream != NULL);

    fprintf(stream, "procedure \"%s\":\n", procedure->name);
    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions,
                                                      index);
        assert(operation != NULL);
        if (operation->instruction == _UNIT_OP_JUMP_MARKER) {
            UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels, operation->argument);
            assert(label != NULL);
            assert(label->name != NULL);
            fprintf(stream, "    label %s [%d]:\n", label->name, label->id);
            continue;
        }

        fprintf(stream, "        %s", UNIT_Instruction_GetName(operation->instruction));
        if (operation->argument != 0
            || operation->instruction == UNIT_OP_LOAD_INTEGER
            || operation->instruction == UNIT_OP_LOAD_ARGUMENT) {
            fprintf(stream, "  %ld", (long)operation->argument);
        }

        switch (operation->instruction) {
            case UNIT_OP_CALL_NAME: {
                const char *name = _UNIT_Vector_GET(&procedure->_symbols,
                                                    operation->argument);
                assert(name != NULL);
                fprintf(stream, "  (%s)", name);
                break;
            }
            case UNIT_OP_CALL_PROCEDURE: {
                UNIT_Procedure *subprocedure = _UNIT_Vector_GET(&procedure->_subprocedures,
                                                                operation->argument);
                assert(subprocedure != NULL);
                fprintf(stream, "  (%p: %s)", subprocedure, subprocedure->name);
                break;
            }
            case UNIT_OP_LOAD_STRING: {
                const char *text = _UNIT_Vector_GET(&procedure->_global_strings,
                                                    operation->argument);
                assert(text != NULL);
                fprintf(stream, "  (%s)", text);
                break;
            }
            case _UNIT_OP_STORE_LOCAL_NAME:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                const char *name = _UNIT_Vector_GET(&procedure->_local_variables,
                                                    operation->argument);
                assert(name != NULL);
                fprintf(stream, "  (%s)", name);
                break;
            }
            case UNIT_OP_JUMP_TO:
            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels,
                                                         operation->argument);
                assert(label != NULL);
                fprintf(stream, "  (%s [%d])", label->name, label->id);
                break;

            }

            default:
                break;
        }

        fprintf(stream, "\n");
    }
}
