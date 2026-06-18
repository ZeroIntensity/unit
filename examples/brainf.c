#include <unit/unit.h>
#include <stdio.h>
#include <string.h>

#define ADDOP_INT(op, value)                                                \
    if (UNIT_FAILED(UNIT_Procedure_AddOperation(procedure, op, value))) {   \
        return -1;                                                          \
    }

#define ADDOP(op) ADDOP_INT(op, 0)

#define ADDOP_CALL(name, argc)                                          \
    if (UNIT_FAILED(UNIT_Procedure_AddCall(procedure, name, argc))) {   \
        return -1;                                                      \
    }

#define ADDOP_STORE_NAME(name)                                                  \
    UNIT_Local name;                                                            \
    if (UNIT_FAILED(UNIT_Procedure_AddStoreLocal(procedure, #name, &name))) {   \
        return -1;                                                              \
    }

#define ADDOP_LOAD_NAME(name)                                           \
    if (UNIT_FAILED(UNIT_Procedure_AddLoadLocal(procedure, name))) {    \
        return -1;                                                      \
    }

#define NEW_JUMP_LABEL(name)                                                    \
    UNIT_JumpLabel *name = UNIT_Procedure_CreateJumpLabel(procedure, #name);    \
    if (name == NULL) {                                                         \
        return -1;                                                              \
    }

#define USE_LABEL(name)                                                         \
    if (UNIT_FAILED(UNIT_Procedure_UseLabel(procedure, name))) {                \
        return -1;                                                              \
    }

#define ADDOP_JUMP(op, name)                                            \
    if (UNIT_FAILED(UNIT_Procedure_AddJump(procedure, op, name))) {     \
        return -1;                                                      \
    }

#define ADDOP_STR(str)                                                      \
    if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(procedure, str))) {        \
        return -1;                                                          \
    }

static int8_t
codegen_prelude(UNIT_Procedure *procedure)
{
    NEW_JUMP_LABEL(fail);
    NEW_JUMP_LABEL(body);

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 30000);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP_CALL("calloc", 2);
    // [ptr]

    ADDOP_INT(UNIT_OP_COPY, 0);
    // [ptr, ptr]

    ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);
    // [ptr]

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    // [ptr, 0]

    ADDOP(UNIT_OP_COMPARE_EQUAL);
    ADDOP_JUMP(UNIT_OP_JUMP_IF_FALSE, body);

    ADDOP_STR("brainfuck");
    ADDOP_CALL("perror", 1);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_EXIT);

    USE_LABEL(body);

    return 0;
}

static int8_t
codegen_right(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    // [ptr, 1]
    ADDOP(UNIT_OP_ADD);
    ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);
    return 0;
}

static int8_t
codegen_left(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    // [ptr, 1]
    ADDOP(UNIT_OP_SUBTRACT);
    ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);
    return 0;
}

static int8_t
codegen_add(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_COPY, 0);

    ADDOP_INT(UNIT_OP_READ_BYTES, 1);
    // [ptr, *ptr]
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_ADD);
    // [ptr, *ptr + 1]

    ADDOP_INT(UNIT_OP_WRITE_BYTES, 1);
    return 0;
}

static int8_t
codegen_sub(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_COPY, 0);

    ADDOP_INT(UNIT_OP_READ_BYTES, 1);
    // [ptr, *ptr]
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_SUBTRACT);
    // [ptr, *ptr - 1]

    ADDOP_INT(UNIT_OP_WRITE_BYTES, 1);
    return 0;
}

static int8_t
codegen_print(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_READ_BYTES, 1);
    // [*ptr]

    ADDOP_CALL("putchar", 1);
    ADDOP(UNIT_OP_POP);

    return 0;
}

static int8_t
codegen_input(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    // [ptr]

    ADDOP_CALL("getchar", 0);
    // [ptr, char]

    ADDOP_INT(UNIT_OP_WRITE_BYTES, 1);

    return 0;
}


static int8_t
codegen_final(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

    return 0;
}

static int8_t
codegen_loop_start(UNIT_Procedure *procedure)
{
    NEW_JUMP_LABEL(loop);
    NEW_JUMP_LABEL(end);

    USE_LABEL(loop);

    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_READ_BYTES, 1);
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    // [*ptr, 0]
    ADDOP(UNIT_OP_COMPARE_EQUAL);

    // [*ptr == 0]
    ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, end);

    return 0;
}

static int8_t
codegen_body(UNIT_Procedure *procedure, FILE *file, int8_t in_loop)
{
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        switch (ch) {
            #define CODEGEN(name)                               \
                {                                               \
                    if (codegen_ ##name (procedure) < 0) {      \
                        return -1;                              \
                    }                                           \
                    break;                                      \
                }

            case '>': CODEGEN(right);
            case '<': CODEGEN(left);
            case '+': CODEGEN(add);
            case '-': CODEGEN(sub);
            case '.': CODEGEN(print);
            case ',': CODEGEN(input);
            case '[': {
                NEW_JUMP_LABEL(loop);
                NEW_JUMP_LABEL(end);

                USE_LABEL(loop);

                ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
                ADDOP_INT(UNIT_OP_READ_BYTES, 1);
                ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
                // [*ptr, 0]
                ADDOP(UNIT_OP_COMPARE_EQUAL);

                // [*ptr == 0]
                ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, end);
                int8_t result = codegen_body(procedure, file, /*in_loop=*/1);
                if ((result != 1) && in_loop) {
                    puts("error: loop was never closed (missing ])");
                    return -1;
                }

                ADDOP_JUMP(UNIT_OP_JUMP_TO, loop);

                USE_LABEL(end);
                break;
            }
            case ']':
                return 1;  // return to caller's '[' handler
            default:
                // Comment character
                break;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    char *path;
    if (argc < 2) {
        path = NULL;
    } else {
        path = argv[1];
    }

    FILE *file;
    if (path == NULL || strcmp(path, "-") == 0) {
        file = stdin;
    } else {
        file = fopen(path, "r");
        if (file == NULL) {
            fprintf(stderr, "error: could not open '%s'\n", path);
            return 1;
        }
    }

    UNIT_Context context;
    if (UNIT_FAILED(UNIT_Context_Init(&context))) {
        goto error;
    }

    UNIT_Procedure procedure;
    if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
        goto error;
    }

    if (codegen_prelude(&procedure) < 0) {
        goto error;
    }

    int8_t result = codegen_body(&procedure, file, /*in_loop=*/0);
    if (result < 0) {
        goto error;
    }

    if (result == 1) {
        puts("error: ] outside loop");
        goto error;
    }

    if (codegen_final(&procedure) < 0) {
        goto error;
    }

#ifndef UNIT_HOST_PLATFORM
#error "Unsupported platform"
#endif

    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
    if (compiled == NULL) {
        goto error;
    }

    if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "test.o", UNIT_FORMAT_ELF))) {
        UNIT_CompiledProcedure_Free(compiled);
        goto error;
    }

    UNIT_CompiledProcedure_Free(compiled);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    if (file != stdin) {
        fclose(file);
    }
    return 0;

error:
    UNIT_PrintError(&context, stderr);
    UNIT_Procedure_Clear(&procedure);
    UNIT_Context_Clear(&context);
    if (file != stdin) {
        fclose(file);
    }
    return 1;
}
