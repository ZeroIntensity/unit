#include <unit/main.h>

int main(void)
{

    UNIT_Context context;

    if (UNIT_FAILED(UNIT_Context_Init(&context))) {
        return 1;
    }

    UNIT_Procedure procedure;
    if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "test"))) {
        UNIT_Context_Clear(&context);
        return 1;
    }

#define ADDOP_INT(name, value)                                                      \
    if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, name, value))) {        \
        goto error;                                                                 \
    }
#define ADDOP(name) ADDOP_INT(name, 0)

#define ADDOP_CALL(func, argc)                                                        \
    if (UNIT_FAILED(UNIT_Procedure_AddCall(&procedure, func, argc))) {                \
        goto error;                                                                 \
    }

#define NEW_JUMP_LABEL(name)                                                        \
    UNIT_JumpLabel *name = UNIT_Procedure_CreateJumpLabel(&procedure, #name);       \
    if (name == NULL) {                                                             \
        goto error;                                                                 \
    }

#define USE_LABEL(name)                                                         \
    if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, name))) {               \
        goto error;                                                             \
    }

#define ADDOP_JUMP(inst, label)                                             \
    if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, inst, label))) {     \
        goto error;                                                         \
    }

#define ADDOP_STR(str)                                                      \
    if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, str))) {       \
        goto error;                                                         \
    }

#define ADDOP_STORE_NAME(name)                                                  \
    UNIT_Local name;                                                            \
    if (UNIT_FAILED(UNIT_Procedure_AddStoreLocal(&procedure, #name, &name))) {  \
        goto error;                                                             \
    }

#define ADDOP_LOAD_NAME(name)                                           \
    if (UNIT_FAILED(UNIT_Procedure_AddLoadLocal(&procedure, name))) {   \
        goto error;                                                     \
    }

    NEW_JUMP_LABEL(correct);
    NEW_JUMP_LABEL(end);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
    ADDOP_STORE_NAME(number);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP_STORE_NAME(guess);

    ADDOP_STR("%d");
    ADDOP_INT(UNIT_OP_ADDRESS_OF, guess.id);
    ADDOP_CALL("scanf", 2);
    ADDOP(UNIT_OP_POP_TOP);

    ADDOP_STR("Debug: guess was %d\n");
    ADDOP_LOAD_NAME(guess);
    ADDOP_CALL("printf", 2);
    ADDOP(UNIT_OP_POP_TOP);

    ADDOP_LOAD_NAME(guess)
    ADDOP_LOAD_NAME(number)
    ADDOP_INT(UNIT_OP_COMPARE, UNIT_COMPARE_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);
    ADDOP_STR("wrong");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);

    ADDOP_JUMP(UNIT_OP_JUMP_TO, end);

    USE_LABEL(correct);
    ADDOP_STR("correct");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);

    USE_LABEL(end);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

    //ADDOP_INT(UNIT_OP_EXIT, 0);

    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_ARCH_AMD64);
    if (compiled == NULL) {
        goto error;
    }

    if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled,
                                                           "test.o", UNIT_FORMAT_ELF))) {
        UNIT_CompiledProcedure_Free(compiled);
        goto error;
    }

    UNIT_CompiledProcedure_Free(compiled);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    return 0;
error:
    UNIT_Error_Print(&context, stderr);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    return 1;
}
