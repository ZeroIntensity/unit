#include <unit/errors.h>
#include <unit/procedure.h>

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
        enum { STACK_UNKNOWN, STACK_CONSTANT, STACK_LOCAL } kind;
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

    #define APPEND(op) _UNIT_Vector_APPEND(&optimized, op);

    int8_t dead_code = 0;

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);

        // Labels end dead code regions
        if (op->instruction == _UNIT_OP_JUMP_MARKER) {
            dead_code = 0;
            RESET_STACK();
            APPEND(op);
            continue;
        }

        if (dead_code) {
            continue;
        }

        switch (op->instruction) {
            case UNIT_OP_LOAD_INTEGER: {
                PUSH(STACK_CONSTANT, op->argument);
                APPEND(op);
                break;
            }

            case UNIT_OP_LOAD_STRING:
            case UNIT_OP_LOAD_ARGUMENT: {
                PUSH(STACK_UNKNOWN, 0);
                APPEND(op);
                break;
            }

            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                // Check for duplicate load
                StackEntry top = PEEK();
                if (top.kind == STACK_LOCAL
                    && top.value == op->argument) {
                    _UNIT_Operation *copy = _UNIT_Alloc(context, sizeof(_UNIT_Operation));
                    if (copy == NULL) {
                        goto error;
                    }
                    copy->instruction = UNIT_OP_COPY;
                    copy->argument = 0;
                    APPEND(copy);
                    PUSH(STACK_LOCAL, op->argument);
                    break;
                }

                PUSH(STACK_LOCAL, op->argument);
                APPEND(op);
                break;
            }

            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME: {
                POP();
                APPEND(op);
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

                    // Remove the two LOAD_INTEGERs we already emitted
                    UNIT_Size opt_size = _UNIT_Vector_SIZE(&optimized);
                    _UNIT_Vector_Pop(&optimized);
                    _UNIT_Vector_Pop(&optimized);

                    _UNIT_Operation *folded = _UNIT_Alloc(context, sizeof(_UNIT_Operation));
                    if (folded == NULL) {
                        goto error;
                    }
                    folded->instruction = UNIT_OP_LOAD_INTEGER;
                    folded->argument = result;
                    APPEND(folded);
                    PUSH(STACK_CONSTANT, result);
                } else {
                    PUSH(STACK_UNKNOWN, 0);
                    APPEND(op);
                }
                break;
            }

            case UNIT_OP_COMPARE_EQUAL:
            case UNIT_OP_COMPARE_NOT_EQUAL:
            case UNIT_OP_COMPARE_GREATER:
            case UNIT_OP_COMPARE_GREATER_EQUAL:
            case UNIT_OP_COMPARE_LESS:
            case UNIT_OP_COMPARE_LESS_EQUAL: {
                // TODO: We can do actual folding here
                POP();
                POP();
                PUSH(STACK_UNKNOWN, 0);
                APPEND(op);
                break;
            }

            case UNIT_OP_POP: {
                StackEntry top = POP();
                if (top.kind == STACK_CONSTANT || top.kind == STACK_LOCAL) {
                    _UNIT_Vector_Pop(&optimized);
                } else {
                    APPEND(op);
                }
                break;
            }

            case UNIT_OP_RETURN_VALUE:
            case UNIT_OP_EXIT:
            case UNIT_OP_JUMP_TO: {
                APPEND(op);
                dead_code = 1;
                RESET_STACK();
                break;
            }

            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE: {
                POP();
                APPEND(op);
                RESET_STACK();
                break;
            }

            case UNIT_OP_PREPARE_CALL: {
                // Pop N arguments off the abstract stack
                for (int64_t i = 0; i < op->argument; ++i) {
                    POP();
                }

                APPEND(op);
                break;
            }

            case UNIT_OP_CALL_NAME:
            case UNIT_OP_CALL_PROCEDURE: {
                PUSH(STACK_UNKNOWN, 0);
                APPEND(op);
                break;
            }

            case UNIT_OP_COPY: {
                if (op->argument < stack_depth) {
                    StackEntry copied = stack[stack_depth - 1 - op->argument];
                    PUSH(copied.kind, copied.value);
                } else {
                    PUSH(STACK_UNKNOWN, 0);
                }
                APPEND(op);
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
                APPEND(op);
                break;
            }

            case UNIT_OP_READ_BYTES: {
                POP();
                PUSH(STACK_UNKNOWN, 0);
                APPEND(op);
                break;
            }

            case UNIT_OP_WRITE_BYTES: {
                POP();
                POP();
                APPEND(op);
                break;
            }

            case UNIT_OP_ADDRESS_OF:
                PUSH(STACK_UNKNOWN, 0);
                APPEND(op);
                break;

            default: {
                _UNIT_Unreachable();
            }
        }
    }

#undef POP
#undef PUSH
#undef PEEK
#undef RESET_STACK
#undef APPEND

    // Replace instruction vector
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

static UNIT_Status
remap_offsets(UNIT_Procedure *target, UNIT_Size local_offset,
              UNIT_Size label_offset, _UNIT_Vector *output)
{
    assert(target != NULL);
    _UNIT_Vector *instructions = &target->_instructions;
    UNIT_Context *context = target->context;
    assert(context != NULL);
    assert(instructions != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(instructions);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *target_op = _UNIT_Vector_GET(instructions, index);

        if (target_op->instruction == UNIT_OP_LOAD_ARGUMENT) {
            // Arguments are already on the stack from the caller
            continue;
        }

        if (target_op->instruction == UNIT_OP_RETURN_VALUE) {
            // The value we want is already on the stack
            break;
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
            case UNIT_OP_ADDRESS_OF:
                remapped->argument += local_offset;
                break;

            case UNIT_OP_JUMP_TO:
            case UNIT_OP_JUMP_IF_TRUE:
            case UNIT_OP_JUMP_IF_FALSE:
            case _UNIT_OP_JUMP_MARKER:
                remapped->argument += label_offset;
                break;
            default:
                break;
        }

        _UNIT_Vector_APPEND(output, remapped);
    }

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

#define APPEND(op) _UNIT_Vector_APPEND(&optimized, op)

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);

        if (op->instruction != UNIT_OP_CALL_PROCEDURE) {
            APPEND(op);
            continue;
        }

        UNIT_Procedure *target = _UNIT_Vector_GET(&procedure->_subprocedures,
                                                   op->argument);

        if (!should_inline(target, procedure)) {
            APPEND(op);
            continue;
        }

        _UNIT_Operation *prepare_call = _UNIT_Vector_Pop(&optimized);
        (void)prepare_call;
        assert(prepare_call->instruction == UNIT_OP_PREPARE_CALL);

        if (UNIT_FAILED(copy_locals(&target->_local_variables, &procedure->_local_variables))) {
            goto error;
        }


        // We need to reserve label slots so remapped indices are valid.
        // This is super hacky though.
        UNIT_Size num_jump_labels = _UNIT_Vector_SIZE(&target->_jump_labels);
        for (UNIT_Size i = 0; i < num_jump_labels; ++i) {
            if (UNIT_Procedure_CreateJumpLabel(procedure, "inlined") == NULL) {
                goto error;
            }
        }

        UNIT_Size local_offset = local_count + _UNIT_Vector_SIZE(&target->_local_variables);
        UNIT_Size label_offset = label_count + num_jump_labels;
        if (UNIT_FAILED(remap_offsets(target, local_offset, label_offset, &optimized))) {
            goto error;
        }
    }

#undef APPEND

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

    #define APPEND(op) _UNIT_Vector_APPEND(&optimized, op)

    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Operation *op = _UNIT_Vector_STEAL(instructions, index);

        switch (op->instruction) {
            case UNIT_OP_STORE_LOCAL:
            case _UNIT_OP_STORE_LOCAL_NAME: {
                UNIT_Size local_index = op->argument;
                if (info[local_index].load_count == 0) {
                    // This is a dead store. Consume the value with pop, which
                    // can be folded out later.
                    _UNIT_Operation *pop = _UNIT_Alloc(context,
                                                       sizeof(_UNIT_Operation));
                    if (pop == NULL) {
                        goto error;
                    }
                    pop->instruction = UNIT_OP_POP;
                    pop->argument = 0;
                    APPEND(pop);
                    continue;
                }
                APPEND(op);

                if (index + 1 <= size) {
                    continue;
                }

                _UNIT_Operation *next = _UNIT_Vector_GET(instructions, index + 1);
                if ((next->instruction == UNIT_OP_LOAD_LOCAL || next->instruction == _UNIT_OP_LOAD_LOCAL_NAME)
                     && (next->argument == op->argument)) {
                    // Redundant load-after-store; replace it with a copy
                    _UNIT_Operation *copy = _UNIT_Alloc(context,
                                                        sizeof(_UNIT_Operation));
                    if (copy == NULL) {
                        goto error;
                    }
                    copy->instruction = UNIT_OP_COPY;
                    copy->argument = 0;
                    APPEND(copy);

                    // Skip the load
                    ++index;
                }
                break;
            }

            case UNIT_OP_LOAD_LOCAL:
            case _UNIT_OP_LOAD_LOCAL_NAME: {
                UNIT_Size local_index = op->argument;
                if (info[local_index].store_count == 1
                    && info[local_index].constant_value_known) {
                    // Propagate constant
                    _UNIT_Operation *load = _UNIT_Alloc(context,
                                                        sizeof(_UNIT_Operation));
                    if (load == NULL) {
                        goto error;
                    }
                    load->instruction = UNIT_OP_LOAD_INTEGER;
                    load->argument = info[local_index].constant_value;
                    APPEND(load);
                    continue;
                }

                APPEND(op);
                break;
            }

            default:
                APPEND(op);
                break;
        }
    }

#undef APPEND

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
    if (UNIT_FAILED(UNIT_Procedure_OptimizeInline(procedure))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(UNIT_Procedure_OptimizeLocals(procedure))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(UNIT_Procedure_OptimizeFold(procedure))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}
