#include "test_util.h"

#include <string.h>

#include <unit/internal/compilation/architectures.h>
#include <unit/internal/ir/basic_block.h>
#include <unit/internal/ir/register_allocation.h>

static void
emit(UNIT_Procedure *proc, UNIT_OperationCode instruction, int64_t argument)
{
    ASSERT_OK(proc->context, UNIT_Procedure_AddOperation(proc, instruction, argument));
}

static UNIT_JumpLabel *
label(UNIT_Procedure *proc, const char *name)
{
    UNIT_JumpLabel *result = UNIT_Procedure_CreateJumpLabel(proc, name);
    ASSERT(result != NULL);
    return result;
}

static void
mark(UNIT_Procedure *proc, UNIT_JumpLabel *target)
{
    ASSERT_OK(proc->context, UNIT_Procedure_UseLabel(proc, target));
}

static void
jump(UNIT_Procedure *proc, UNIT_OperationCode instruction, UNIT_JumpLabel *target)
{
    ASSERT_OK(proc->context, UNIT_Procedure_AddJump(proc, instruction, target));
}

static UNIT_Size
block_index(_UNIT_Translation *translation, _UNIT_BasicBlock *block)
{
    for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&translation->blocks); ++i) {
        if (_UNIT_Vector_GET(&translation->blocks, i) == block) {
            return i;
        }
    }

    ASSERT(0);
    return -1;
}

static void
check_use(_UNIT_MachineItem *item,
          UNIT_Size block,
          UNIT_Size instruction,
          UNIT_Size *definitions,
          UNIT_Size *positions,
          int8_t *dominators,
          UNIT_Size count)
{
    if (item == NULL) {
        return;
    }

    if (item->type == _UNIT_TYPE_CALL_ARGS) {
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(item->call_args); ++i) {
            check_use(_UNIT_Vector_GET(item->call_args, i),
                      block,
                      instruction,
                      definitions,
                      positions,
                      dominators,
                      count);
        }
    } else if (item->type == _UNIT_TYPE_LOCATION) {
        UNIT_Size definition = definitions[item->value];
        ASSERT(definition >= 0);
        ASSERT(dominators[block * count + definition]);
        ASSERT(definition != block || positions[item->value] < instruction);
    } else {
        ASSERT(item->type == _UNIT_TYPE_CONSTANT || item->type == _UNIT_TYPE_MEMORY);
    }
}

// Verify the SSA invariants independently of translation: exactly one definition,
// dominance of every use, and precisely one PHI operand per predecessor edge.
static UNIT_Size
check_ssa(_UNIT_Translation *translation)
{
    UNIT_Size count = _UNIT_Vector_SIZE(&translation->blocks);
    UNIT_Size locations = translation->num_locations;
    UNIT_Size *definitions = malloc((locations + 1) * sizeof(*definitions));
    UNIT_Size *positions = malloc((locations + 1) * sizeof(*positions));
    int8_t *dominators = malloc(count * count);
    ASSERT(definitions != NULL && positions != NULL && dominators != NULL);
    for (UNIT_Size i = 0; i < locations; ++i) {
        definitions[i] = -1;
    }

    memset(dominators, 1, count * count);
    memset(dominators, 0, count);
    dominators[0] = 1;
    int8_t changed;
    do {
        changed = 0;
        for (UNIT_Size b = 1; b < count; ++b) {
            _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
            ASSERT(_UNIT_Vector_SIZE(&block->predecessors) > 0);
            for (UNIT_Size d = 0; d < count; ++d) {
                int8_t dominates = 1;
                for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&block->predecessors); ++p) {
                    UNIT_Size pred = block_index(translation,
                                                 _UNIT_Vector_GET(&block->predecessors, p));
                    dominates &= dominators[pred * count + d];
                }

                dominates |= b == d;
                if (dominators[b * count + d] != dominates) {
                    dominators[b * count + d] = dominates;
                    changed = 1;
                }
            }
        }
    } while (changed);
    UNIT_Size phis = 0;
    for (UNIT_Size b = 0; b < count; ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        _UNIT_Vector *vectors[] = {&block->phis, &block->instructions};
        phis += _UNIT_Vector_SIZE(&block->phis);
        for (UNIT_Size v = 0; v < 2; ++v) {
            for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(vectors[v]); ++i) {
                _UNIT_MachineOperation *op = _UNIT_Vector_GET(vectors[v], i);
                if (_UNIT_MachineDestination_IsNull(op->destination)
                    || _UNIT_MachineDestination_IsInput(op->destination)) {
                    continue;
                }

                _UNIT_MachineItem *dest = _UNIT_MachineDestination_GetPointer(op->destination);
                if (dest->type == _UNIT_TYPE_LOCATION) {
                    ASSERT(dest->value >= 0 && dest->value < locations);
                    ASSERT_EQ(definitions[dest->value], -1);
                    definitions[dest->value] = b;
                    positions[dest->value] = v == 0 ? -1 : i;
                }
            }
        }
    }

    for (UNIT_Size b = 0; b < count; ++b) {
        _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation->blocks, b);
        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&block->phis); ++i) {
            _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&block->phis, i);
            ASSERT_EQ(phi->instruction, _UNIT_I_PHI);
            ASSERT_EQ(phi->argument_1->type, _UNIT_TYPE_PHI_ARGS);
            ASSERT(!_UNIT_SizeSet_Contains(&block->liveness.alive_at_start,
                                           _UNIT_MachineDestination_GetPointer(
                                               phi->destination)->value));
            _UNIT_Vector *inputs = phi->argument_1->phi_args;
            ASSERT_EQ(_UNIT_Vector_SIZE(inputs), _UNIT_Vector_SIZE(&block->predecessors));
            for (UNIT_Size p = 0; p < _UNIT_Vector_SIZE(&block->predecessors); ++p) {
                _UNIT_BasicBlock *pred = _UNIT_Vector_GET(&block->predecessors, p);
                UNIT_Size found = 0;
                for (UNIT_Size a = 0; a < _UNIT_Vector_SIZE(inputs); ++a) {
                    _UNIT_PhiInput *input = _UNIT_Vector_GET(inputs, a);
                    if (input->predecessor != pred) {
                        continue;
                    }

                    ++found;
                    check_use(input->value,
                              block_index(translation, pred),
                              _UNIT_Vector_SIZE(&pred->instructions),
                              definitions,
                              positions,
                              dominators,
                              count);
                    if (input->value->type == _UNIT_TYPE_LOCATION) {
                        ASSERT(_UNIT_SizeSet_Contains(&pred->liveness.alive_at_end,
                                                      input->value->value));
                    }
                }

                ASSERT_EQ(found, 1);
            }
        }

        for (UNIT_Size i = 0; i < _UNIT_Vector_SIZE(&block->instructions); ++i) {
            _UNIT_MachineOperation *op = _UNIT_Vector_GET(&block->instructions, i);
            check_use(op->argument_1, b, i, definitions, positions, dominators, count);
            check_use(op->argument_2, b, i, definitions, positions, dominators, count);
            if (_UNIT_MachineDestination_IsInput(op->destination)) {
                check_use(_UNIT_MachineDestination_GetPointer(op->destination),
                          b,
                          i,
                          definitions,
                          positions,
                          dominators,
                          count);
            }
        }
    }

    free(definitions);
    free(positions);
    free(dominators);
    return phis;
}

static void
verify(UNIT_Procedure *proc, UNIT_Size minimum_phis)
{
    _UNIT_Translation translation;
    ASSERT_OK(proc->context, _UNIT_Translate(&translation, proc));
    ASSERT(check_ssa(&translation) >= minimum_phis);
    FILE *stream = tmpfile();
    ASSERT(stream != NULL);
    ASSERT_OK(proc->context, _UNIT_Translation_PrintInstructions(&translation, proc->name, stream));
    if (minimum_phis > 0) {
        rewind(stream);
        char line[1024];
        int found = 0;
        while (fgets(line, sizeof(line), stream) != NULL) {
            found |= strstr(line, "PHI([block") != NULL;
        }

        ASSERT(found);
    }

    fclose(stream);
    _UNIT_Translation_Clear(&translation);
}

static void
run(UNIT_Procedure *proc, int64_t argument, int64_t expected)
{
#if defined(__x86_64__) || defined(_M_X64)
    for (int optimize = 0; optimize < 2; ++optimize) {
        UNIT_Procedure_SetFlags(proc,
                                optimize ? UNIT_FLAG_NONE : UNIT_FLAG_NO_OPTIMIZE_TRANSLATION);
        UNIT_CompiledProcedure *compiled = UNIT_Compile(proc, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            UNIT_PrintError(proc->context, stderr);
        }

        ASSERT(compiled != NULL);
        UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(compiled, NULL);
        ASSERT(buffer != NULL);
        int64_t (*function)(int64_t, int64_t, int64_t, int64_t) =
            (int64_t (*)(int64_t,
                         int64_t,
                         int64_t,
                         int64_t))UNIT_ExecutableBuffer_GetPointer(buffer);
        int64_t actual = function(argument, 13, 17, 19);
        ASSERT_EQ(actual, expected);
        UNIT_ExecutableBuffer_Free(buffer);
        UNIT_CompiledProcedure_Free(compiled);
    }

#endif
}

static void
branch_on_zero(UNIT_Procedure *proc, UNIT_JumpLabel *target)
{
    emit(proc, UNIT_OP_LOAD_ARGUMENT, 0);
    emit(proc, UNIT_OP_LOAD_INTEGER, 0);
    emit(proc, UNIT_OP_COMPARE_EQUAL, 0);
    jump(proc, UNIT_OP_JUMP_IF_TRUE, target);
}

static void
test_stack_diamond(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "stack_diamond"));
    UNIT_JumpLabel *other = label(&proc, "other");
    UNIT_JumpLabel *join = label(&proc, "join");
    branch_on_zero(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 40);
    jump(&proc, UNIT_OP_JUMP, join);
    // Dead code is neither a predecessor nor a stack/local definition.
    emit(&proc, UNIT_OP_LOAD_LOCAL, 99);
    emit(&proc, UNIT_OP_POP, 0);
    mark(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 2);
    mark(&proc, join);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 3);
    emit(&proc, UNIT_OP_ADD, 0);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 1);
    run(&proc, 0, 5);
    run(&proc, 1, 43);
    UNIT_Procedure_Clear(&proc);
}

static void
test_critical_edge(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "critical_edge"));
    UNIT_JumpLabel *join = label(&proc, "join");
    emit(&proc, UNIT_OP_LOAD_INTEGER, 7);
    emit(&proc, UNIT_OP_STORE_LOCAL, 0);
    branch_on_zero(&proc, join);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 11);
    emit(&proc, UNIT_OP_STORE_LOCAL, 0);
    mark(&proc, join);
    emit(&proc, UNIT_OP_LOAD_LOCAL, 0);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 1);
    run(&proc, 0, 7);
    run(&proc, 1, 11);
    UNIT_Procedure_Clear(&proc);
}

static void
make_swap_loop(UNIT_Context *context, UNIT_Procedure *proc, UNIT_Size values)
{
    ASSERT_OK(context, UNIT_Procedure_Init(proc, context, "swap_loop"));
    UNIT_JumpLabel *loop = label(proc, "loop");
    UNIT_JumpLabel *done = label(proc, "done");
    emit(proc, UNIT_OP_LOAD_ARGUMENT, 0);
    emit(proc, UNIT_OP_STORE_LOCAL, 0);
    for (UNIT_Size i = 0; i < values; ++i) {
        emit(proc, UNIT_OP_LOAD_INTEGER, i + 1);
    }

    mark(proc, loop);
    emit(proc, UNIT_OP_LOAD_LOCAL, 0);
    emit(proc, UNIT_OP_LOAD_INTEGER, 0);
    emit(proc, UNIT_OP_COMPARE_EQUAL, 0);
    jump(proc, UNIT_OP_JUMP_IF_TRUE, done);
    for (UNIT_Size depth = 1; depth < values; ++depth) {
        emit(proc, UNIT_OP_SWAP, depth);
    }

    emit(proc, UNIT_OP_LOAD_LOCAL, 0);
    emit(proc, UNIT_OP_LOAD_INTEGER, 1);
    emit(proc, UNIT_OP_SUBTRACT, 0);
    emit(proc, UNIT_OP_STORE_LOCAL, 0);
    jump(proc, UNIT_OP_JUMP, loop);
    mark(proc, done);
    // Weighted sum observes the position of every incoming value.
    for (UNIT_Size i = values; i > 0; --i) {
        emit(proc, UNIT_OP_STORE_LOCAL, i);
    }

    emit(proc, UNIT_OP_LOAD_INTEGER, 0);
    for (UNIT_Size i = 1; i <= values; ++i) {
        emit(proc, UNIT_OP_LOAD_LOCAL, i);
        emit(proc, UNIT_OP_LOAD_INTEGER, i * i);
        emit(proc, UNIT_OP_MULTIPLY, 0);
        emit(proc, UNIT_OP_ADD, 0);
    }

    emit(proc, UNIT_OP_RETURN_VALUE, 0);
}

static void
test_parallel_copies_and_spills(UNIT_Context *context)
{
    for (UNIT_Size values = 2; values <= 12; values += 10) {
        UNIT_Procedure proc;
        make_swap_loop(context, &proc, values);
        verify(&proc, values + 1);
        int64_t stack[12];
        for (UNIT_Size i = 0; i < values; ++i) {
            stack[i] = i + 1;
        }

        for (int64_t iterations = 0; iterations < 7; ++iterations) {
            int64_t expected = 0;
            for (UNIT_Size i = 0; i < values; ++i) {
                expected += stack[i] * (i + 1) * (i + 1);
            }

            run(&proc, iterations, expected);
            int64_t saved = stack[0];
            for (UNIT_Size i = 1; i < values; ++i) {
                stack[i - 1] = stack[i];
            }

            stack[values - 1] = saved;
        }

        UNIT_Procedure_Clear(&proc);
    }
}

static void
test_address_taken_local(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "address_taken"));
    UNIT_JumpLabel *join = label(&proc, "join");
    emit(&proc, UNIT_OP_LOAD_INTEGER, 5);
    emit(&proc, UNIT_OP_STORE_LOCAL, 0);
    emit(&proc, UNIT_OP_ADDRESS_OF, 0);
    emit(&proc, UNIT_OP_STORE_LOCAL, 1);
    branch_on_zero(&proc, join);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 11);
    emit(&proc, UNIT_OP_STORE_LOCAL, 0);
    mark(&proc, join);
    emit(&proc, UNIT_OP_LOAD_LOCAL, 1);
    emit(&proc, UNIT_OP_READ_BYTES, 8);
    emit(&proc, UNIT_OP_LOAD_LOCAL, 1);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 23);
    emit(&proc, UNIT_OP_WRITE_BYTES, 8);
    emit(&proc, UNIT_OP_LOAD_LOCAL, 0);
    emit(&proc, UNIT_OP_ADD, 0);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 0);
    run(&proc, 0, 28);
    run(&proc, 1, 34);
    UNIT_Procedure_Clear(&proc);
}

static void
test_argument_lifetimes(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "arguments"));
    for (UNIT_Size i = 0; i < 4; ++i) {
        emit(&proc, UNIT_OP_LOAD_ARGUMENT, i);
    }

    for (UNIT_Size i = 0; i < 3; ++i) {
        emit(&proc, UNIT_OP_ADD, 0);
    }

    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 0);
    run(&proc, 7, 56);
    UNIT_Procedure_Clear(&proc);
}

static void
test_irreducible_loop(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "irreducible"));
    UNIT_JumpLabel *a = label(&proc, "a");
    UNIT_JumpLabel *b = label(&proc, "b");
    UNIT_JumpLabel *done = label(&proc, "done");
    emit(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
    emit(&proc, UNIT_OP_STORE_LOCAL, 0);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 0);
    emit(&proc, UNIT_OP_STORE_LOCAL, 1);
    emit(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 3);
    emit(&proc, UNIT_OP_COMPARE_LESS, 0);
    jump(&proc, UNIT_OP_JUMP_IF_TRUE, b);
    jump(&proc, UNIT_OP_JUMP, a);
    for (int side = 0; side < 2; ++side) {
        mark(&proc, side ? b : a);
        emit(&proc, UNIT_OP_LOAD_LOCAL, 1);
        emit(&proc, UNIT_OP_LOAD_INTEGER, side ? 10 : 1);
        emit(&proc, UNIT_OP_ADD, 0);
        emit(&proc, UNIT_OP_STORE_LOCAL, 1);
        emit(&proc, UNIT_OP_LOAD_LOCAL, 0);
        emit(&proc, UNIT_OP_LOAD_INTEGER, 1);
        emit(&proc, UNIT_OP_SUBTRACT, 0);
        emit(&proc, UNIT_OP_STORE_LOCAL, 0);
        emit(&proc, UNIT_OP_LOAD_LOCAL, 0);
        emit(&proc, UNIT_OP_LOAD_INTEGER, 0);
        emit(&proc, UNIT_OP_COMPARE_GREATER, 0);
        jump(&proc, UNIT_OP_JUMP_IF_TRUE, side ? a : b);
        jump(&proc, UNIT_OP_JUMP, done);
    }

    mark(&proc, done);
    emit(&proc, UNIT_OP_LOAD_LOCAL, 1);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 4);
    run(&proc, 1, 10);
    run(&proc, 2, 11);
    run(&proc, 3, 12);
    run(&proc, 4, 22);
    run(&proc, 9, 45);
    UNIT_Procedure_Clear(&proc);
}

static void
test_three_predecessors(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "three_predecessors"));
    UNIT_JumpLabel *zero = label(&proc, "zero");
    UNIT_JumpLabel *one = label(&proc, "one");
    UNIT_JumpLabel *join = label(&proc, "join");
    branch_on_zero(&proc, zero);
    emit(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 1);
    emit(&proc, UNIT_OP_COMPARE_EQUAL, 0);
    jump(&proc, UNIT_OP_JUMP_IF_TRUE, one);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 30);
    jump(&proc, UNIT_OP_JUMP, join);
    mark(&proc, zero);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 10);
    jump(&proc, UNIT_OP_JUMP, join);
    mark(&proc, one);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 20);
    mark(&proc, join);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    _UNIT_Translation translation;
    ASSERT_OK(context, _UNIT_Translate(&translation, &proc));
    ASSERT_EQ(check_ssa(&translation), 1);
    _UNIT_BasicBlock *block = _UNIT_Vector_GET(&translation.blocks,
                                               _UNIT_Vector_SIZE(&translation.blocks) - 1);
    _UNIT_MachineOperation *phi = _UNIT_Vector_GET(&block->phis, 0);
    ASSERT_EQ(_UNIT_Vector_SIZE(phi->argument_1->phi_args), 3);
    _UNIT_Translation_Clear(&translation);
    run(&proc, 0, 10);
    run(&proc, 1, 20);
    run(&proc, 2, 30);
    UNIT_Procedure_Clear(&proc);
}

static void
test_aggregate_merges(UNIT_Context *context)
{
    for (int comparison = 0; comparison < 2; ++comparison) {
        UNIT_Procedure proc;
        ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "aggregate"));
        UNIT_JumpLabel *other = label(&proc, "other");
        UNIT_JumpLabel *join = label(&proc, "join");
        UNIT_JumpLabel *yes = label(&proc, "yes");
        branch_on_zero(&proc, other);
        for (int side = 0; side < 2; ++side) {
            if (side) {
                mark(&proc, other);
            }

            if (comparison) {
                emit(&proc, UNIT_OP_LOAD_ARGUMENT, 1);
                emit(&proc, UNIT_OP_LOAD_INTEGER, side ? 20 : 10);
                emit(&proc, UNIT_OP_COMPARE_LESS, 0);
            } else {
                emit(&proc, UNIT_OP_LOAD_INTEGER, side ? -7 : -11);
                emit(&proc, UNIT_OP_PREPARE_CALL, 1);
            }

            if (!side) {
                jump(&proc, UNIT_OP_JUMP, join);
            }
        }

        mark(&proc, join);
        if (comparison) {
            jump(&proc, UNIT_OP_JUMP_IF_TRUE, yes);
            emit(&proc, UNIT_OP_LOAD_INTEGER, 0);
            emit(&proc, UNIT_OP_RETURN_VALUE, 0);
            mark(&proc, yes);
            emit(&proc, UNIT_OP_LOAD_INTEGER, 1);
        } else {
            // Prepare arguments on each incoming path; CALL_NAME consumes the
            // merged aggregate without an additional PREPARE_CALL.
            ASSERT_OK(context, _UNIT_Vector_Append(&proc._symbols, _UNIT_StrDup(context, "llabs")));
            emit(&proc, UNIT_OP_CALL_NAME, 0);
        }

        emit(&proc, UNIT_OP_RETURN_VALUE, 0);
        verify(&proc, 1);
        run(&proc, 0, comparison ? 1 : 7);
        run(&proc, 1, comparison ? 0 : 11);
        UNIT_Procedure_Clear(&proc);
    }
}

static void
test_trivial_phi(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "trivial_phi"));
    UNIT_JumpLabel *other = label(&proc, "other");
    UNIT_JumpLabel *join = label(&proc, "join");
    branch_on_zero(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 42);
    jump(&proc, UNIT_OP_JUMP, join);
    mark(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 42);
    mark(&proc, join);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    _UNIT_Translation translation;
    ASSERT_OK(context, _UNIT_Translate(&translation, &proc));
    ASSERT_EQ(check_ssa(&translation), 0);
    _UNIT_Translation_Clear(&translation);
    run(&proc, 0, 42);
    run(&proc, 1, 42);
    UNIT_Procedure_Clear(&proc);
}

static void
test_folded_phi_inputs(UNIT_Context *context)
{
    UNIT_Procedure proc;
    ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "folded_phi_inputs"));
    UNIT_JumpLabel *other = label(&proc, "other");
    UNIT_JumpLabel *join = label(&proc, "join");
    branch_on_zero(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 40);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 2);
    emit(&proc, UNIT_OP_ADD, 0);
    jump(&proc, UNIT_OP_JUMP, join);
    mark(&proc, other);
    emit(&proc, UNIT_OP_LOAD_INTEGER, INT64_MAX);
    emit(&proc, UNIT_OP_LOAD_INTEGER, 1);
    emit(&proc, UNIT_OP_ADD, 0);
    mark(&proc, join);
    emit(&proc, UNIT_OP_RETURN_VALUE, 0);
    verify(&proc, 1);
    run(&proc, 0, INT64_MIN);
    run(&proc, 1, 42);
    UNIT_Procedure_Clear(&proc);
}

static void
test_invalid_merges(UNIT_Context *context)
{
    for (int stack_mismatch = 0; stack_mismatch < 2; ++stack_mismatch) {
        UNIT_Procedure proc;
        ASSERT_OK(context, UNIT_Procedure_Init(&proc, context, "invalid_merge"));
        UNIT_JumpLabel *join = label(&proc, "join");
        branch_on_zero(&proc, join);
        emit(&proc, UNIT_OP_LOAD_INTEGER, 1);
        if (!stack_mismatch) {
            emit(&proc, UNIT_OP_STORE_LOCAL, 0);
        }

        mark(&proc, join);
        emit(&proc, stack_mismatch ? UNIT_OP_LOAD_INTEGER : UNIT_OP_LOAD_LOCAL, 0);
        emit(&proc, UNIT_OP_RETURN_VALUE, 0);
        _UNIT_Translation translation;
        ASSERT(UNIT_FAILED(_UNIT_Translate(&translation, &proc)));
        UNIT_Procedure_Clear(&proc);
    }
}

int
main(void)
{
    UNIT_Context context;
    ASSERT_OK(&context, UNIT_Context_Init(&context));
    RUN_TEST(test_stack_diamond, &context);
    RUN_TEST(test_critical_edge, &context);
    RUN_TEST(test_parallel_copies_and_spills, &context);
    RUN_TEST(test_address_taken_local, &context);
    RUN_TEST(test_argument_lifetimes, &context);
    RUN_TEST(test_irreducible_loop, &context);
    RUN_TEST(test_three_predecessors, &context);
    RUN_TEST(test_aggregate_merges, &context);
    RUN_TEST(test_trivial_phi, &context);
    RUN_TEST(test_folded_phi_inputs, &context);
    RUN_TEST(test_invalid_merges, &context);
    UNIT_Context_Clear(&context);
    return 0;
}
