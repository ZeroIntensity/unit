#include <unit/errors.h>
#include <unit/procedure.h>

static UNIT_Status
create_artificial_instruction(_UNIT_Vector *instructions,
                              UNIT_Instruction opcode, int64_t oparg)
{
    assert(instructions != NULL);

    _UNIT_Operation *operation = _UNIT_Alloc(instructions->context, sizeof(_UNIT_Operation));
    if (operation == NULL) {
        return _UNIT_FAIL;
    }

    operation->instruction = opcode;
    operation->argument = oparg;

    if (UNIT_FAILED(_UNIT_Vector_Append(instructions, operation))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_OptimizeFold(UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;

    _UNIT_Vector *instructions = &procedure->_instructions;
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);

    _UNIT_Vector optimized;
    if (UNIT_FAILED(_UNIT_Vector_Init(&optimized, context, size, _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    typedef struct {
        enum { STACK_UNKNOWN, STACK_CONSTANT, STACK_LOCAL, STACK_ADDRESS, STACK_COMPARE } kind;
        int64_t value;
    } StackEntry;

    // TODO: We should consider using an actual vector here
    StackEntry stack[256];
    UNIT_Size stack_depth = 0;

    #define PUSH(k, v) do {                             \
        if (stack_depth < 256) {                        \
            stack[stack_depth].kind = (k);              \
            stack[stack_depth].value = (v);             \
            stack_depth++;                              \
        }                                               \
    } while (0)

    #define POP() (stack_depth > 0 ? stack[--stack_depth] \
                   : (StackEntry){ STACK_UNKNOWN, 0 })

    #define PEEK() (stack_depth > 0 ? stack[stack_depth - 1] \
                    : (StackEntry){ STACK_UNKNOWN, 0 })

    #define RESET_STACK() stack_depth = 0

    #define ADD_NEW_INSTRUCTION(inst, op)                                           \
        if (UNIT_FAILED(create_artificial_instruction(&optimized, inst, op))) {     \
            return _UNIT_FAIL;                                                      \
        }

    int8_t dead_code = 0;

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);

        // Labels end dead code regions
        if (op->instruction == _UNIT_OP_JUMP_MARKER) {
            dead_code = 0;
            RESET_STACK();
            _UNIT_Vector_APPEND(&optimized, op);
            continue;
        }

        if (dead_code) {
            continue;
        }

        switch (op->instruction) {
            case UNIT_OP_LOAD_INTEGER: {
                PUSH(STACK_CONSTANT, op->argument);
                break;
            }

            case UNIT_OP_LOAD_STRING:
            case UNIT_OP_LOAD_ARGUMENT: {
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                // Check for duplicate load
                StackEntry top = PEEK();
                if (top.kind == STACK_LOCAL
                    && top.value == op->argument) {
                    ADD_NEW_INSTRUCTION(UNIT_OP_COPY, 0);
                    PUSH(STACK_LOCAL, op->argument);
                    break;
                }

                PUSH(STACK_LOCAL, op->argument);
                break;
            }

            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME: {
                POP();
                break;
            }

            case UNIT_OP_ADD:
            case UNIT_OP_SUBTRACT:
            case UNIT_OP_MULTIPLY:
            case UNIT_OP_DIVIDE:
            case UNIT_OP_MODULO: {
                StackEntry right = POP();
                StackEntry left = POP();
                if (left.kind == STACK_CONSTANT
                    && right.kind == STACK_CONSTANT) {
                    int64_t result;
                    switch (op->instruction) {
                        case UNIT_OP_ADD:
                            result = left.value + right.value;
                            break;
                        case UNIT_OP_SUBTRACT:
                            result = left.value - right.value;
                            break;
                        case UNIT_OP_MULTIPLY:
                            result = left.value * right.value;
                            break;
                        case UNIT_OP_DIVIDE:
                            if (right.value == 0) {
                                _UNIT_SetError(context, UNIT_ERROR_INVALID_USAGE, "division by zero");
                                return _UNIT_FAIL;
                            }
                            result = left.value / right.value;
                            break;
                        case UNIT_OP_MODULO:
                            if (right.value == 0) {
                                _UNIT_SetError(context, UNIT_ERROR_INVALID_USAGE, "division by zero");
                                return _UNIT_FAIL;
                            }
                            result = left.value % right.value;
                        default:
                            _UNIT_Unreachable();
                    }

                    // Remove the two loads
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);

                    ADD_NEW_INSTRUCTION(UNIT_OP_LOAD_INTEGER, result);
                    PUSH(STACK_CONSTANT, result);
                    continue;
                } else {
                    PUSH(STACK_UNKNOWN, 0);
                }
                break;
            }

            #define SIMPLE_COMPARE_FOLD(op)                                         \
                if (left.kind == STACK_CONSTANT && right.kind == STACK_CONSTANT) {  \
                    /* Remove the two values from the stack */                      \
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);                            \
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);                            \
                    PUSH(STACK_COMPARE, left.value op right.value);                 \
                    break;                                                          \
                }

            case UNIT_OP_COMPARE_EQUAL: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(==);

                if (left.kind == STACK_LOCAL && right.kind == STACK_LOCAL) {
                    if (left.value == right.value) {
                        // Two comparisons of the same local will always be true
                        PUSH(STACK_COMPARE, true);
                        break;
                    }
                }

                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COMPARE_NOT_EQUAL: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(!=);

                if (left.kind == STACK_LOCAL && right.kind == STACK_LOCAL) {
                    if (left.value == right.value) {
                        PUSH(STACK_COMPARE, false);
                        break;
                    }
                }

                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COMPARE_GREATER: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(>);
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COMPARE_GREATER_EQUAL: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(>=);
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COMPARE_LESS: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(<);
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COMPARE_LESS_EQUAL: {
                StackEntry right = POP();
                StackEntry left = POP();
                SIMPLE_COMPARE_FOLD(<=);
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_POP: {
                POP();
                _UNIT_Operation *previous = _UNIT_Vector_GET(&optimized, _UNIT_Vector_SIZE(&optimized) - 1);
                assert(previous != NULL);
                UNIT_Instruction kind = previous->instruction;
                switch (kind) {
                    case UNIT_OP_LOAD_INTEGER:
                    case UNIT_OP_ADD:
                    case UNIT_OP_SUBTRACT:
                    case UNIT_OP_MULTIPLY:
                    case UNIT_OP_DIVIDE:
                    case UNIT_OP_MODULO:
                    case UNIT_OP_COPY:
                    case UNIT_OP_SWAP:
                    case UNIT_OP_COMPARE_EQUAL:
                    case UNIT_OP_COMPARE_NOT_EQUAL:
                    case UNIT_OP_COMPARE_GREATER:
                    case UNIT_OP_COMPARE_GREATER_EQUAL:
                    case UNIT_OP_COMPARE_LESS:
                    case UNIT_OP_COMPARE_LESS_EQUAL:
                    case UNIT_OP_READ_BYTES:
                    case UNIT_OP_ADDRESS_OF:
                    case UNIT_OP_LOAD_LOCAL:
                        _UNIT_Vector_Pop(&optimized);
                        continue;
                    default:
                        break;
                }
                break;
            }

            case UNIT_OP_RETURN_VALUE:
            case UNIT_OP_EXIT:
            case UNIT_OP_JUMP_TO: {
                dead_code = 1;
                RESET_STACK();
                break;
            }

            case UNIT_OP_JUMP_IF_TRUE: {
                StackEntry top = POP();
                if (top.kind == STACK_COMPARE) {
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);

                    if (top.value) {
                        ADD_NEW_INSTRUCTION(UNIT_OP_JUMP_TO, op->argument);
                        dead_code = 1;
                    }
                    continue;
                }
                RESET_STACK();
                break;
            }

            case UNIT_OP_JUMP_IF_FALSE: {
                StackEntry top = POP();
                if (top.kind == STACK_COMPARE) {
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);

                    if (!top.value) {
                        ADD_NEW_INSTRUCTION(UNIT_OP_JUMP_TO, op->argument);
                        dead_code = 1;
                    }
                    continue;
                }

                RESET_STACK();
                break;
            }

            case UNIT_OP_PREPARE_CALL: {
                for (int64_t i = 0; i < op->argument; ++i) {
                    POP();
                }

                break;
            }

            case UNIT_OP_CALL_NAME:
            case UNIT_OP_CALL_PROCEDURE: {
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_COPY: {
                if (op->argument < stack_depth) {
                    StackEntry copied = stack[stack_depth - 1 - op->argument];
                    PUSH(copied.kind, copied.value);
                } else {
                    PUSH(STACK_UNKNOWN, 0);
                }

                break;
            }

            case UNIT_OP_SWAP: {
                assert(op->argument < stack_depth);
                if (op->argument < stack_depth) {
                    StackEntry tmp = stack[stack_depth - 1];
                    stack[stack_depth - 1] = stack[stack_depth - 1 - op->argument];
                    stack[stack_depth - 1 - op->argument] = tmp;
                } else {
                    RESET_STACK();
                }

                break;
            }

            case UNIT_OP_READ_BYTES: {
                StackEntry entry = POP();
                if (entry.kind == STACK_ADDRESS) {
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);
                    ADD_NEW_INSTRUCTION(UNIT_OP_LOAD_LOCAL, op->argument);
                    break;
                }
                PUSH(STACK_UNKNOWN, 0);
                break;
            }

            case UNIT_OP_WRITE_BYTES: {
                POP();
                POP();
                break;
            }

            case UNIT_OP_ADDRESS_OF:
                PUSH(STACK_ADDRESS, op->argument);
                break;

            default: {
                _UNIT_Unreachable();
            }
        }

        _UNIT_Vector_Append(&optimized, op);
    }

#undef POP
#undef PUSH
#undef PEEK
#undef RESET_STACK
#undef APPEND
#undef ADD_NEW_INSTRUCTION

    _UNIT_Vector old = procedure->_instructions;
    procedure->_instructions = optimized;
    _UNIT_Vector_Clear(&old);
    return _UNIT_OK;

error:
    _UNIT_Vector_Clear(&optimized);
    return _UNIT_FAIL;
}

static int8_t
should_inline(const UNIT_Procedure *target, const UNIT_Procedure *caller)
{
    assert(target != NULL);
    assert(caller != NULL);
    if (target == caller) {
        // Never inline recursive calls
        return 0;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&target->_instructions);
    return size <= 50; // Probably needs tuning
}

static UNIT_Status
copy_locals(_UNIT_Vector *in, _UNIT_Vector *out)
{
    assert(in != NULL);
    assert(out != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(in);
    for (UNIT_Size index = 0; index < size; ++index) {
        const char *name = _UNIT_Vector_GET(in, index);
        assert(name != NULL);

        char *copy = _UNIT_StrDup(out->context, name);
        if (copy == NULL) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(out, copy))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

typedef struct {
    UNIT_Size local_offset;
    UNIT_Size label_offset;
    UNIT_Size symbol_offset;
    UNIT_Size string_offset;
    UNIT_Size args_offset; // Number of arguments
} Offsets;

static UNIT_Status
remap_offsets(UNIT_Procedure *procedure,
              UNIT_Procedure *target,
              Offsets *offsets,
              _UNIT_Vector *output)
{
    #define ADD_NEW_INSTRUCTION(inst, op)                                           \
        if (UNIT_FAILED(create_artificial_instruction(output, inst, op))) {         \
            return _UNIT_FAIL;                                                      \
        }

    assert(procedure != NULL);
    assert(target != NULL);
    assert(offsets != NULL);
    assert(output != NULL);

    _UNIT_Vector *instructions = &target->_instructions;
    UNIT_Context *context = target->context;
    assert(context != NULL);
    assert(instructions != NULL);

    UNIT_Size nargs = offsets->args_offset;
    UNIT_Size local_offset = offsets->local_offset;

    // When functions are inlined, when implement returns as locals.
    // There might be a more efficient way to do this.
    char return_local_name[64];
    snprintf(return_local_name, sizeof(return_local_name),
             "_inlined_%s_return_value", target->name);
    UNIT_Local return_local;
    if (UNIT_FAILED(UNIT_Procedure_CreateLocal(procedure, return_local_name, &return_local))) {
        return _UNIT_FAIL;
    }

    char return_name[128];
    snprintf(return_name, sizeof(return_name), "_inlined_%s_return", target->name);
    UNIT_JumpLabel *end_label = UNIT_Procedure_CreateJumpLabel(procedure, return_name);
    if (end_label == NULL) {
        return _UNIT_FAIL;
    }

    // Usually these loads/stores will get optimized away in another pass
    for (UNIT_Size i = 0; i < nargs; ++i) {
        int64_t arg_local_id = local_offset + nargs - 1 - i;
        ADD_NEW_INSTRUCTION(UNIT_OP_STORE_LOCAL, arg_local_id);
    }

    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *target_op = _UNIT_Vector_GET(instructions, index);
        assert(target_op != NULL);

        if (target_op->instruction == UNIT_OP_LOAD_ARGUMENT) {
            int64_t arg_local_id = local_offset + target_op->argument;
            ADD_NEW_INSTRUCTION(UNIT_OP_LOAD_LOCAL, arg_local_id);

            continue;
        }

        if (target_op->instruction == UNIT_OP_RETURN_VALUE) {
            ADD_NEW_INSTRUCTION(_UNIT_OP_STORE_LOCAL_NAME, return_local.id);
            ADD_NEW_INSTRUCTION(UNIT_OP_JUMP_TO, end_label->id);

            continue;
        }

        _UNIT_Operation *remapped = _UNIT_Alloc(context, sizeof(_UNIT_Operation));
        if (remapped == NULL) {
            return _UNIT_FAIL;
        }
        *remapped = *target_op;

        switch (target_op->instruction) {
            case UNIT_OP_STORE_LOCAL:
            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME:
            case _UNIT_OP_LOAD_LOCAL_NAME:
            case UNIT_OP_ADDRESS_OF: {
                remapped->argument += local_offset;
                break;
            }

            case UNIT_OP_JUMP_TO:
            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE:
            case _UNIT_OP_JUMP_MARKER: {
                remapped->argument += offsets->label_offset;
                break;
            }

            case UNIT_OP_CALL_NAME: {
                remapped->argument += offsets->symbol_offset;
                break;
            }

            case UNIT_OP_LOAD_STRING: {
                remapped->argument += offsets->string_offset;
                break;
            }

            default:
                break;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(output, remapped))) {
            return _UNIT_FAIL;
        }
    }

    ADD_NEW_INSTRUCTION(_UNIT_OP_JUMP_MARKER, end_label->id);
    ADD_NEW_INSTRUCTION(_UNIT_OP_LOAD_LOCAL_NAME, return_local.id);

#undef ADD_NEW_INSTRUCTION

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_OptimizeInline(UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;

    UNIT_Size local_count = _UNIT_Vector_SIZE(&procedure->_local_variables);
    UNIT_Size label_count = _UNIT_Vector_SIZE(&procedure->_jump_labels);

    _UNIT_Vector *instructions = &procedure->_instructions;
    UNIT_Size size = _UNIT_Vector_SIZE(instructions);

    _UNIT_Vector optimized;
    if (UNIT_FAILED(_UNIT_Vector_Init(&optimized, context, size, NULL))) {
        return _UNIT_FAIL;
    }

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);

        if (op->instruction != UNIT_OP_CALL_PROCEDURE) {
            _UNIT_Vector_APPEND(&optimized, op);
            continue;
        }

        UNIT_Procedure *target = _UNIT_Vector_GET(&procedure->_subprocedures,
                                                   op->argument);

        if (!should_inline(target, procedure)) {
            _UNIT_Vector_APPEND(&optimized, op);
            continue;
        }

        _UNIT_Operation *prepare_call = _UNIT_Vector_Pop(&optimized);
        assert(prepare_call->instruction == UNIT_OP_PREPARE_CALL);
        UNIT_Size nargs = prepare_call->argument;

        UNIT_Size local_offset = _UNIT_Vector_SIZE(&procedure->_local_variables);
        UNIT_Size label_offset = _UNIT_Vector_SIZE(&procedure->_jump_labels);

        if (UNIT_FAILED(copy_locals(&target->_local_variables, &procedure->_local_variables))) {
            goto error;
        }

        // We need to reserve label slots so remapped indices are valid.
        // This is super hacky though.
        UNIT_Size num_jump_labels = _UNIT_Vector_SIZE(&target->_jump_labels);
        for (UNIT_Size i = 0; i < num_jump_labels; ++i) {
            UNIT_JumpLabel *label = _UNIT_Vector_GET(&target->_jump_labels, i);
            assert(label != NULL);
            char name[128];
            snprintf(name, sizeof(name), "_inlined_%s_%s", target->name, label->name);
            if (UNIT_Procedure_CreateJumpLabel(procedure, name) == NULL) {
                goto error;
            }
        }

        UNIT_Size symbol_offset = _UNIT_Vector_SIZE(&procedure->_symbols);
        UNIT_Size target_symbols = _UNIT_Vector_SIZE(&target->_symbols);
        for (UNIT_Size i = 0; i < target_symbols; ++i) {
            const char *name = _UNIT_Vector_GET(&target->_symbols, i);
            char *copy = _UNIT_StrDup(context, name);
            if (copy == NULL) {
                return _UNIT_FAIL;
            }

            if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_symbols, copy))) {
                return _UNIT_FAIL;
            }
        }

        UNIT_Size string_offset = _UNIT_Vector_SIZE(&procedure->_global_strings);
        UNIT_Size target_strings = _UNIT_Vector_SIZE(&target->_global_strings);
        for (UNIT_Size j = 0; j < target_strings; ++j) {
            const char *str = _UNIT_Vector_GET(&target->_global_strings, j);
            char *copy = _UNIT_StrDup(context, str);
            if (copy == NULL) return _UNIT_FAIL;
            if (UNIT_FAILED(_UNIT_Vector_Append(&procedure->_global_strings, copy))) {
                return _UNIT_FAIL;
            }
        }

        Offsets offsets = {
            .local_offset = local_offset,
            .label_offset = label_offset,
            .symbol_offset = symbol_offset,
            .string_offset = string_offset,
            .args_offset = nargs,
        };

        if (UNIT_FAILED(remap_offsets(procedure, target, &offsets, &optimized))) {
            goto error;
        }
    }

    _UNIT_Vector_Clear(&procedure->_instructions);
    procedure->_instructions = optimized;
    return _UNIT_OK;

error:
    _UNIT_Vector_Clear(&optimized);
    return _UNIT_FAIL;
}

typedef struct {
    UNIT_Size store_count;
    UNIT_Size load_count;
    int8_t constant_value_known;
    int64_t constant_value;
} LocalInfo;

static UNIT_Status
gather_local_info(_UNIT_Vector *instructions, LocalInfo **info_ptr)
{
    UNIT_Size max_local = 0;

    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_GET(instructions, index);
        switch (op->instruction) {
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME:
            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME:
            case UNIT_OP_ADDRESS_OF: {
                if (op->argument >= max_local) {
                    max_local = op->argument + 1;
                }
                break;
            }
            default:
                break;
        }
    }

    if (max_local == 0) {
        *info_ptr = NULL;
        return _UNIT_OK;
    }

    LocalInfo *info = _UNIT_Calloc(instructions->context,
                                   max_local, sizeof(LocalInfo));
    if (info == NULL) {
        return _UNIT_FAIL;
    }
    *info_ptr = info;

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_GET(instructions, index);
        UNIT_Size local_index;

        switch (op->instruction) {
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME:
                local_index = op->argument;
                info[local_index].store_count++;

                if (index > 0) {
                    _UNIT_Operation *previous = _UNIT_Vector_GET(instructions, index - 1);
                    if (previous->instruction == UNIT_OP_LOAD_INTEGER
                        && info[local_index].store_count == 1) {
                        info[local_index].constant_value_known = 1;
                        info[local_index].constant_value = previous->argument;
                    } else {
                        info[local_index].constant_value_known = 0;
                    }
                }
                break;

            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME:
                info[op->argument].load_count++;
                break;

            case UNIT_OP_ADDRESS_OF:
                // When the address is taken, our optimization breaks down.
                info[op->argument].store_count = -1;
                info[op->argument].load_count = -1;
                break;

            default:
                break;
        }
    }

    return _UNIT_OK;
}

UNIT_Status
UNIT_Procedure_OptimizeLocals(UNIT_Procedure *procedure)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;

    _UNIT_Vector *instructions = &procedure->_instructions;
    // gather_local_info() will use the context on the instructions vector
    assert(instructions->context == context);

    LocalInfo *info;
    if (UNIT_FAILED(gather_local_info(instructions, &info))) {
        return _UNIT_FAIL;
    }

    if (info == NULL) {
        // Nothing to optimize
        return _UNIT_OK;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(instructions);

    _UNIT_Vector optimized;
    if (UNIT_FAILED(_UNIT_Vector_Init(&optimized, context, size, _UNIT_Dealloc))) {
        _UNIT_Dealloc(context, info);
        return _UNIT_FAIL;
    }

    #define ADD_NEW_INSTRUCTION(inst, op)                                           \
        if (UNIT_FAILED(create_artificial_instruction(&optimized, inst, op))) {     \
            goto error;                                                             \
        }

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);
        assert(op != NULL);

        switch (op->instruction) {
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME: {
                UNIT_Size local_index = op->argument;
                if (info[local_index].load_count == 0) {
                    // This is a dead store. Consume the value with pop, which
                    // can be folded out later.
                    ADD_NEW_INSTRUCTION(UNIT_OP_POP, 0);
                    continue;
                }

                if (index + 1 >= size) {
                    break;
                }

                _UNIT_Operation *next = _UNIT_Vector_GET(instructions, index + 1);
                if ((next->instruction == UNIT_OP_LOAD_LOCAL || next->instruction == _UNIT_OP_LOAD_LOCAL_NAME)
                     && (next->argument == op->argument)) {
                    // Redundant load-after-store; replace it with a copy
                    ADD_NEW_INSTRUCTION(UNIT_OP_COPY, 0);
                    _UNIT_Vector_APPEND(&optimized, op);

                    // Skip the load
                    ++index;
                    continue;
                }

                break;
            }

            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                UNIT_Size local_index = op->argument;
                if (info[local_index].store_count == 1
                    && info[local_index].constant_value_known) {
                    // Propagate constant
                    ADD_NEW_INSTRUCTION(UNIT_OP_LOAD_INTEGER, info[local_index].constant_value);
                    continue;
                }

                break;
            }

            default:
                break;
        }

        _UNIT_Vector_APPEND(&optimized, op);
    }

#undef ADD_NEW_INSTRUCTION

    _UNIT_Dealloc(context, info);
    _UNIT_Vector_Clear(instructions);
    procedure->_instructions = optimized;
    return _UNIT_OK;

error:
    _UNIT_Dealloc(context, info);
    _UNIT_Vector_Clear(instructions);
    return _UNIT_FAIL;
}

UNIT_Status
UNIT_Procedure_Optimize(UNIT_Procedure *procedure)
{
    for (int i = 0; i <= 5; ++i) {
        if (UNIT_FAILED(UNIT_Procedure_OptimizeInline(procedure))) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(UNIT_Procedure_OptimizeLocals(procedure))) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(UNIT_Procedure_OptimizeFold(procedure))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}
