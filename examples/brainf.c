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

    ADDOP(UNIT_OP_DEREFERENCE);
    // [ptr, *ptr]
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_ADD);
    // [ptr, *ptr + 1]

    ADDOP(UNIT_OP_WRITE_THROUGH);
    return 0;
}

static int8_t
codegen_sub(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP_INT(UNIT_OP_COPY, 0);

    ADDOP(UNIT_OP_DEREFERENCE);
    // [ptr, *ptr]
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
    ADDOP(UNIT_OP_SUBTRACT);
    // [ptr, *ptr - 1]

    ADDOP(UNIT_OP_WRITE_THROUGH);
    return 0;
}

static int8_t
codegen_print(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_LOCAL, 0);
    ADDOP(UNIT_OP_DEREFERENCE);
    // [*ptr]

    ADDOP_CALL("putchar", 1);
    ADDOP(UNIT_OP_POP_TOP);

    return 0;
}

static int8_t
codegen_final(UNIT_Procedure *procedure)
{
    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

    return 0;
}

int main(int argc, char **argv)
{
    char *path;
    if (argc < 2) {
        path = NULL;
    } else {
        path = argv[2];
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

    char character;
    while (true) {
        character = fgetc(file);
        if (character == EOF) {
            break;
        }

        switch (character) {
            case '>': {
                if (codegen_right(&procedure) < 0) {
                    goto error;
                }
                break;
            }
            case '<': {
                if (codegen_left(&procedure) < 0) {
                    goto error;
                }
                break;
            }
            case '+': {
                if (codegen_add(&procedure) < 0) {
                    goto error;
                }
                break;
            }
            case '-': {
                if (codegen_sub(&procedure) < 0) {
                    goto error;
                }
                break;
            }
            case '.': {
                if (codegen_print(&procedure) < 0) {
                    goto error;
                }
                break;
            }
            case ',': {
                break;
            }
            case '[': {
                break;
            }
            case ']': {
                break;
            }
            default:
                // Comment character
                break;
        }
    }

    if (codegen_final(&procedure) < 0) {
        goto error;
    }

    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_ARCH_AMD64);
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
