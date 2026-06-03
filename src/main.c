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

    NEW_JUMP_LABEL(loop);
    NEW_JUMP_LABEL(win);
    NEW_JUMP_LABEL(greater);
    NEW_JUMP_LABEL(less);

    ADDOP_STR("Guessing game");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);

    //ADDOP_CALL("rand", 0);
    ADDOP_INT(UNIT_OP_LOAD_CONSTANT_INTEGER, 35);
    ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);

    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_STR("Debug; number is %d\n");
    ADDOP_CALL("printf", 2);

    USE_LABEL(loop);
    ADDOP_INT(UNIT_OP_LOAD_CONSTANT_INTEGER, 0);
    ADDOP_INT(UNIT_OP_STORE_LOCAL, 1);

    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 1);
    ADDOP(UNIT_OP_ADDRESS_OF);
    ADDOP_STR("%d");
    ADDOP_CALL("scanf", 2);
    ADDOP(UNIT_OP_POP_TOP);

    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 1);
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_COMPARE, UNIT_COMPARE_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, win);

    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 1);
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_COMPARE, UNIT_COMPARE_GREATER_THAN);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, greater);
    ADDOP_JUMP(UNIT_OP_JUMP_TO, less);

    USE_LABEL(greater);

    ADDOP_STR("Higher");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);
    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    USE_LABEL(less);

    ADDOP_STR("Lower");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);
    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    USE_LABEL(win);
    ADDOP_STR("You win!");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP_TOP);

    ADDOP_INT(UNIT_OP_LOAD_CONSTANT_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

    //ADDOP_INT(UNIT_OP_EXIT, 0);

    if (UNIT_FAILED(UNIT_CompileProcedure(&procedure))) {
        goto error;
    }

    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    return 0;
error:
    UNIT_Error_Print(&context, stderr);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    return 1;
}
