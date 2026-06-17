#include <unit/unit.h>

int main(void)
{

    UNIT_Context context;

    if (UNIT_FAILED(UNIT_Context_Init(&context))) {
        return 1;
    }

    UNIT_Procedure procedure;
    if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
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

    NEW_JUMP_LABEL(loop);
    NEW_JUMP_LABEL(correct);
    NEW_JUMP_LABEL(greater);
    NEW_JUMP_LABEL(invalid);
    NEW_JUMP_LABEL(set_seed_from_time);
    NEW_JUMP_LABEL(invalid_seed);
    NEW_JUMP_LABEL(start_game);

    ADDOP_INT(UNIT_OP_LOAD_ARGUMENT, 0);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_COMPARE_LESS_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, set_seed_from_time);

    ADDOP_INT(UNIT_OP_LOAD_ARGUMENT, 1); // argv
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, sizeof(char*));
    ADDOP(UNIT_OP_ADD);
    // [argv + 8 (&argv[1])]
    ADDOP_INT(UNIT_OP_READ_BYTES, 8);
    // [argv[1]]

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP_STORE_NAME(seed);

    ADDOP_STR("%d");
    ADDOP_INT(UNIT_OP_ADDRESS_OF, seed.id);
    // [argv[1], "%d", &seed]
    ADDOP_CALL("sscanf", 3);
    // [result]

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_COMPARE_NOT_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, invalid_seed);

    ADDOP_LOAD_NAME(seed);
    ADDOP_CALL("srand", 1);
    ADDOP(UNIT_OP_POP);
    ADDOP_JUMP(UNIT_OP_JUMP_TO, start_game);

    USE_LABEL(invalid_seed);
    ADDOP_STR("Not a valid seed");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_RETURN_VALUE);

    USE_LABEL(set_seed_from_time);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0); // NULL
    ADDOP_CALL("time", 1);
    ADDOP_CALL("srand", 1);
    ADDOP(UNIT_OP_POP);

    USE_LABEL(start_game);

    ADDOP_STR("Guessing game!\nThe number is between 1 and 100");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);

    ADDOP_CALL("rand", 0);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 100);
    ADDOP(UNIT_OP_MODULO);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_ADD);
    ADDOP_STORE_NAME(number);

    ADDOP_STR("Debug: number is %d\n");
    ADDOP_LOAD_NAME(number);
    ADDOP_CALL("printf", 2);
    ADDOP(UNIT_OP_POP);

    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    USE_LABEL(invalid);

    // We need to consume the bad characters from the buffer
    ADDOP_STR("%*[^\n]%*c");
    ADDOP_CALL("scanf", 1);
    ADDOP(UNIT_OP_POP);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP_STORE_NAME(guess);
    ADDOP_STR("Not a valid number");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);

    USE_LABEL(loop);

    ADDOP_STR("%d");
    ADDOP_INT(UNIT_OP_ADDRESS_OF, guess.id);
    ADDOP_CALL("scanf", 2);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_COMPARE_NOT_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, invalid);

    ADDOP_STR("Debug: guess was %d\n");
    ADDOP_LOAD_NAME(guess);
    ADDOP_CALL("printf", 2);
    ADDOP(UNIT_OP_POP);

    ADDOP_LOAD_NAME(guess)
    ADDOP_LOAD_NAME(number)
    ADDOP(UNIT_OP_COMPARE_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);

    ADDOP_LOAD_NAME(number)
    ADDOP_LOAD_NAME(guess)
    ADDOP(UNIT_OP_COMPARE_GREATER);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, greater);

    ADDOP_STR("Lower");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);

    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    USE_LABEL(greater);
    ADDOP_STR("Higher");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);
    ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

    USE_LABEL(correct);
    ADDOP_STR("You win!");
    ADDOP_CALL("puts", 1);
    ADDOP(UNIT_OP_POP);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

    //ADDOP_INT(UNIT_OP_EXIT, 0);

    UNIT_Platform platform;
    if (UNIT_FAILED(UNIT_GetCurrentPlatform(&context, &platform))) {
        goto error;
    }

    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, platform);
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
    UNIT_PrintError(&context, stderr);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    return 1;
}
