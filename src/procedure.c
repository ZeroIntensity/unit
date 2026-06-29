#include <stdbool.h>
#include <string.h>

#include <unit/errors.h>
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
    procedure->flags = UNIT_FLAG_NONE;
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

typedef enum {
    DEBUG_TYPE_INT,
    DEBUG_TYPE_STRING,
    DEBUG_TYPE_ARITHMETIC_RESULT,
    DEBUG_TYPE_COMPARISON_RESULT,
    DEBUG_TYPE_ADDRESS,
    DEBUG_TYPE_BYTES,
    DEBUG_TYPE_LOCAL,
    DEBUG_TYPE_LOCAL_NAME,
    DEBUG_TYPE_ARGUMENT,
    DEBUG_TYPE_SYMBOL_CALL_RESULT,
    DEBUG_TYPE_PROCEDURE_CALL_RESULT,
} DebugStackType;

typedef struct {
    DebugStackType type;
    union {
        int64_t value;
        const char *string;
    };
} DebugStackItem;

/* Print a string with newlines represented as a "\n" */
static UNIT_Status
print_string(UNIT_Context *context, const char *string, FILE *stream)
{
    assert(context != NULL);
    assert(string != NULL);
    assert(stream != NULL);

    UNIT_Size length = strlen(string);
    for (UNIT_Size index = 0; index < length; ++index) {
        char character = string[index];
        if (character == '\n') {
            if (fputs("\\n", stream) == EOF) {
                goto error;
            }
            continue;
        }

        if (fputc(character, stream) == EOF) {
            goto error;
        }
    }

    return _UNIT_OK;
error:
    _UNIT_SetOSError(context, "printing string");
    return _UNIT_FAIL;
}

static UNIT_Status
print_debug_item(UNIT_Context *context, DebugStackItem *item, FILE *stream)
{
    assert(context != NULL);
    assert(item != NULL);
    assert(stream != NULL);

#define PRINT(...)                                      \
    if (fprintf(stream, __VA_ARGS__) < 0) {             \
        _UNIT_SetOSError(context, "printing stack");    \
        return _UNIT_FAIL;                              \
    }

    switch (item->type) {
        case DEBUG_TYPE_INT: {
            PRINT("%ld", item->value);
            break;
        }

        case DEBUG_TYPE_STRING: {
            PRINT("\"");
            print_string(context, item->string, stream);
            PRINT("\"");
            break;
        }

        case DEBUG_TYPE_ARITHMETIC_RESULT: {
            PRINT("arithmetic_result");
            break;
        }

        case DEBUG_TYPE_COMPARISON_RESULT: {
            PRINT("comparison_result");
            break;
        }

        case DEBUG_TYPE_ADDRESS: {
            PRINT("address_of_%ld", item->value);
            break;
        }

        case DEBUG_TYPE_BYTES: {
            PRINT("bytes");
            break;
        }

        case DEBUG_TYPE_LOCAL_NAME: {
            PRINT("local_%s", item->string);
            break;
        }

        case DEBUG_TYPE_LOCAL: {
            PRINT("local_%ld", item->value);
            break;
        }

        case DEBUG_TYPE_ARGUMENT: {
            PRINT("argument_%ld", item->value);
            break;
        }

        case DEBUG_TYPE_PROCEDURE_CALL_RESULT: {
            PRINT("call_%s_result", item->string);
            break;
        }

        case DEBUG_TYPE_SYMBOL_CALL_RESULT: {
            PRINT("call_%s_result", item->string);
            break;
        }
    }

#undef PRINT

    return _UNIT_OK;
}

static UNIT_Status
print_debug_stack(_UNIT_Vector *debug_stack, FILE *stream)
{
    assert(debug_stack != NULL);
    assert(stream != NULL);

#define PRINT(...)                                                  \
    if (fprintf(stream, __VA_ARGS__) < 0) {                         \
        _UNIT_SetOSError(debug_stack->context, "printing stack");   \
        return _UNIT_FAIL;                                          \
    }

    PRINT("[");
    UNIT_Size size = _UNIT_Vector_SIZE(debug_stack);
    assert(size >= 0);
    for (UNIT_Size index = 0; index < size; ++index) {
        DebugStackItem *item = _UNIT_Vector_GET(debug_stack, index);
        assert(item != NULL);
        if (UNIT_FAILED(print_debug_item(debug_stack->context, item, stream))) {
            return _UNIT_FAIL;
        }

        if (index + 1 != size) {
            PRINT(", ");
        }
    }

    PRINT("]");

#undef PRINT

    return _UNIT_OK;
}

char *
get_string(const _UNIT_Vector *table, const char *what, int64_t index)
{
    if (!_UNIT_Vector_INDEX_IS_VALID(table, index)) {
        _UNIT_SetErrorFormat(table->context, UNIT_ERROR_INVALID_USAGE,
                             "%ld is an invalid %s index", what, index);
        return NULL;
    }
    char *string = _UNIT_Vector_GET(table, index);
    assert(string != NULL);
    return string;
}

static UNIT_Status
deduce_stack_effect(const UNIT_Procedure *procedure, const _UNIT_Operation *op,
                    _UNIT_Vector *debug_stack)
{
    assert(op != NULL);
    assert(debug_stack != NULL);
    UNIT_Context *context = debug_stack->context;
    assert(context != NULL);

#define POP()                                                                   \
    if (_UNIT_Vector_SIZE(debug_stack) != 0) {                                  \
        _UNIT_Dealloc(context, _UNIT_Vector_Pop(debug_stack));                  \
    }                                                                           \

#define PUSH(tp, the_value)                                                     \
    do {                                                                        \
        DebugStackItem *item = _UNIT_Alloc(context, sizeof(DebugStackItem));    \
        if (item == NULL) {                                                     \
            return _UNIT_FAIL;                                                  \
        }                                                                       \
        item->type = tp;                                                        \
        item->value = (int64_t)(the_value);                                     \
        if (UNIT_FAILED(_UNIT_Vector_Append(debug_stack, item))) {              \
            return _UNIT_FAIL;                                                  \
        }                                                                       \
    } while (0)

    int64_t oparg = op->argument;

    switch (op->instruction) {
        case UNIT_OP_LOAD_INTEGER: {
            PUSH(DEBUG_TYPE_INT, oparg);
            break;
        }

        case UNIT_OP_LOAD_STRING: {
            char *string = get_string(&procedure->_global_strings, "string", oparg);
            if (string == NULL) {
                return _UNIT_FAIL;
            }
            PUSH(DEBUG_TYPE_STRING, string);
            break;
        }

        case UNIT_OP_LOAD_LOCAL: {
            PUSH(DEBUG_TYPE_LOCAL, oparg);
            break;
        }

        case _UNIT_OP_LOAD_LOCAL_NAME: {
            char *name = get_string(&procedure->_local_variables, "variable", oparg);
            if (name == NULL) {
                return _UNIT_FAIL;
            }

            PUSH(DEBUG_TYPE_LOCAL_NAME, name);
            break;
        }

        case _UNIT_OP_STORE_LOCAL_NAME:
        case UNIT_OP_STORE_LOCAL: {
            POP();
            break;
        }

        case UNIT_OP_PREPARE_CALL: {
            for (int64_t i = 0; i < oparg; ++i) {
                POP();
            }
            break;
        }

        case UNIT_OP_CALL_NAME: {
            char *symbol = get_string(&procedure->_symbols, "symbol", oparg);
            if (symbol == NULL) {
                return _UNIT_FAIL;
            }
            PUSH(DEBUG_TYPE_SYMBOL_CALL_RESULT, symbol);
            break;
        }

        case UNIT_OP_CALL_PROCEDURE: {
            if (!_UNIT_Vector_INDEX_IS_VALID(&procedure->_subprocedures, oparg)) {
                _UNIT_SetErrorFormat(context, UNIT_ERROR_INVALID_USAGE,
                                     "%ld is not a valid subprocedure index", oparg);
                return _UNIT_FAIL;
            }
            UNIT_Procedure *procedure = _UNIT_Vector_GET(&procedure->_subprocedures, oparg);
            assert(procedure != NULL);
            PUSH(DEBUG_TYPE_PROCEDURE_CALL_RESULT, procedure->name);
            break;
        }

#define BINARY_OP(name)                             \
    case name: {                                    \
        POP();                                      \
        POP();                                      \
        PUSH(DEBUG_TYPE_ARITHMETIC_RESULT, 0);      \
        break;                                      \
    }

        BINARY_OP(UNIT_OP_ADD);
        BINARY_OP(UNIT_OP_SUBTRACT);
        BINARY_OP(UNIT_OP_MULTIPLY);
        BINARY_OP(UNIT_OP_DIVIDE);
        BINARY_OP(UNIT_OP_MODULO);

#undef BINARY_OP

        case _UNIT_OP_JUMP_MARKER:
        case UNIT_OP_JUMP_TO: {
            break;
        }

        case UNIT_OP_JUMP_IF_TRUE:
        case UNIT_OP_JUMP_IF_FALSE:
        case UNIT_OP_EXIT:
        case UNIT_OP_RETURN_VALUE: {
            POP();
            break;
        }

        case UNIT_OP_LOAD_ARGUMENT: {
            PUSH(DEBUG_TYPE_ARGUMENT, oparg);
            break;
        }

#define COMPARISON(name)                            \
        case name: {                                \
            POP();                                  \
            POP();                                  \
            PUSH(DEBUG_TYPE_COMPARISON_RESULT, 0);  \
            break;                                  \
        }

        COMPARISON(UNIT_OP_COMPARE_EQUAL);
        COMPARISON(UNIT_OP_COMPARE_NOT_EQUAL);
        COMPARISON(UNIT_OP_COMPARE_GREATER);
        COMPARISON(UNIT_OP_COMPARE_GREATER_EQUAL);
        COMPARISON(UNIT_OP_COMPARE_LESS);
        COMPARISON(UNIT_OP_COMPARE_LESS_EQUAL);

#undef COMPARISON

        case UNIT_OP_COPY: {
            UNIT_Size offset = _UNIT_Vector_SIZE(debug_stack) - oparg - 1;
            if (!_UNIT_Vector_INDEX_IS_VALID(debug_stack, offset)) {
                _UNIT_SetErrorFormat(context, UNIT_ERROR_INVALID_USAGE,
                                     "invalid offset: %ld", oparg);
                return _UNIT_FAIL;
            }

            DebugStackItem *item = _UNIT_Vector_GET(debug_stack, offset);
            assert(item != NULL);

            DebugStackItem *copy = _UNIT_Alloc(context, sizeof(DebugStackItem));
            if (copy == NULL) {
                return _UNIT_FAIL;
            }

            memcpy(copy, item, sizeof(DebugStackItem));
            if (UNIT_FAILED(_UNIT_Vector_Append(debug_stack, copy))) {
                return _UNIT_FAIL;
            }

            break;
        }

        case UNIT_OP_SWAP: {
            UNIT_Size offset = _UNIT_Vector_SIZE(debug_stack) - oparg - 1;
            if (!_UNIT_Vector_INDEX_IS_VALID(debug_stack, offset)) {
                _UNIT_SetErrorFormat(context, UNIT_ERROR_INVALID_USAGE,
                                     "invalid offset: %ld", oparg);
                return _UNIT_FAIL;
            }

            // If the stack were empty, the above case would have failed.
            assert(_UNIT_Vector_SIZE(debug_stack) > 0);

            DebugStackItem *top = _UNIT_Vector_STEAL(debug_stack, 0);
            assert(top != NULL);
            DebugStackItem *at_offset = _UNIT_Vector_STEAL(debug_stack, offset);
            assert(at_offset != NULL);

            _UNIT_Vector_SET(debug_stack, 0, at_offset);
            _UNIT_Vector_SET(debug_stack, offset, top);

            break;
        }

        case UNIT_OP_POP: {
            POP();
            break;
        }

        case UNIT_OP_ADDRESS_OF: {
            PUSH(DEBUG_TYPE_ADDRESS, oparg);
            break;
        }

        case UNIT_OP_WRITE_BYTES: {
            POP();
            // fallthrough
        }
        case UNIT_OP_READ_BYTES: {
            POP();

            switch (oparg) {
                case 1:
                case 2:
                case 4:
                case 8:
                    break;

                default: {
                    _UNIT_SetErrorFormat(context, UNIT_ERROR_INVALID_USAGE,
                                         "can only read/write 1, 2, 4, or 8 bytes, not %ld");
                    break;
                }
            }

            if (op->instruction == UNIT_OP_READ_BYTES) {
                PUSH(DEBUG_TYPE_BYTES, oparg);
            }
            break;
        }

        case UNIT_OP_CONVERT: {
            break;
        }
    }

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_PrintInstructions(const UNIT_Procedure *procedure, FILE *stream,
                                 int8_t visualize_stack_effect)
{
    assert(procedure != NULL);
    assert(stream != NULL);

    _UNIT_Vector debug_stack;
    if (UNIT_FAILED(_UNIT_Vector_Init(&debug_stack, procedure->context,
                                      8, _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

#define PRINT(...)                                          \
    if (fprintf(stream, __VA_ARGS__) < 0) {                 \
        _UNIT_SetOSError(procedure->context, "printing");   \
        goto error;                                         \
    }

    PRINT("procedure \"%s\":\n", procedure->name);
    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *operation = _UNIT_Vector_GET(&procedure->_instructions,
                                                      index);
        assert(operation != NULL);
        if (operation->instruction == _UNIT_OP_JUMP_MARKER) {
            UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels, operation->argument);
            assert(label != NULL);
            assert(label->name != NULL);
            PRINT("    label %s [%d]:\n", label->name, label->id);
            continue;
        }

        PRINT("    %ld    %s", index, UNIT_Instruction_GetName(operation->instruction));
        if (operation->argument != 0
            || operation->instruction == UNIT_OP_LOAD_INTEGER
            || operation->instruction == UNIT_OP_LOAD_ARGUMENT
            || operation->instruction == UNIT_OP_STORE_LOCAL
            || operation->instruction == UNIT_OP_LOAD_LOCAL
            || operation->instruction == _UNIT_OP_STORE_LOCAL_NAME
            || operation->instruction == _UNIT_OP_LOAD_LOCAL_NAME
            || operation->instruction == UNIT_OP_ADDRESS_OF) {
            PRINT("  %ld", (long)operation->argument);
        }

        switch (operation->instruction) {
            case UNIT_OP_CALL_NAME: {
                const char *name = _UNIT_Vector_GET(&procedure->_symbols,
                                                    operation->argument);
                assert(name != NULL);
                PRINT(" (%s)", name);
                break;
            }

            case UNIT_OP_CALL_PROCEDURE: {
                UNIT_Procedure *subprocedure = _UNIT_Vector_GET(&procedure->_subprocedures,
                                                                operation->argument);
                assert(subprocedure != NULL);
                PRINT(" (%p: %s)", subprocedure, subprocedure->name);
                break;
            }
            case UNIT_OP_LOAD_STRING: {
                const char *text = _UNIT_Vector_GET(&procedure->_global_strings,
                                                    operation->argument);
                assert(text != NULL);
                PRINT(" (");
                print_string(procedure->context, text, stream);
                PRINT(")");
                break;
            }
            case _UNIT_OP_STORE_LOCAL_NAME:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                const char *name = _UNIT_Vector_GET(&procedure->_local_variables,
                                                    operation->argument);
                assert(name != NULL);
                PRINT(" (%s)", name);
                break;
            }
            case UNIT_OP_JUMP_TO:
            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                UNIT_JumpLabel *label = _UNIT_Vector_GET(&procedure->_jump_labels,
                                                         operation->argument);
                assert(label != NULL);
                PRINT(" (%s [%d])", label->name, label->id);
                break;

            }

            default:
                break;
        }

        PRINT("\n");

        if (visualize_stack_effect) {
            PRINT("      ");

            if (UNIT_FAILED(deduce_stack_effect(procedure, operation, &debug_stack))) {
                goto error;
            }

            if (UNIT_FAILED(print_debug_stack(&debug_stack, stream))) {
                goto error;
            }

            PRINT("\n");
        }
    }

    _UNIT_Vector_Clear(&debug_stack);
    return _UNIT_OK;
error:
    _UNIT_Vector_Clear(&debug_stack);
    return _UNIT_FAIL;
}

void
UNIT_Procedure_SetFlags(UNIT_Procedure *procedure, UNIT_Flags flags)
{
    assert(procedure != NULL);
    procedure->flags = flags;
}

UNIT_Flags
UNIT_Procedure_GetFlags(const UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    return procedure->flags;
}
