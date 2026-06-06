#ifndef UNIT_PROCEDURE_H
#define UNIT_PROCEDURE_H

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/map.h>
#include <unit/internal/vector.h>

typedef enum {
    UNIT_OP_LOAD_LOCAL,
    UNIT_OP_STORE_LOCAL,
    _UNIT_OP_LOAD_LOCAL_NAME,
    _UNIT_OP_STORE_LOCAL_NAME,
    UNIT_OP_LOAD_STRING,
    UNIT_OP_LOAD_INTEGER,
    UNIT_OP_ADD,
    UNIT_OP_SUBTRACT,
    UNIT_OP_MULTIPLY,
    UNIT_OP_DIVIDE,
    UNIT_OP_MODULO,
    _UNIT_OP_JUMP_MARKER,
    UNIT_OP_JUMP_TO,
    UNIT_OP_CALL_NAME,
    UNIT_OP_EXIT,
    UNIT_OP_POP_TOP,
    UNIT_OP_PREPARE_CALL,
    UNIT_OP_RETURN_VALUE,
    UNIT_OP_JUMP_IF_FALSE,
    UNIT_OP_JUMP_IF_TRUE,
    UNIT_OP_COMPARE,
    UNIT_OP_ADDRESS_OF,
} UNIT_Instruction;

typedef enum {
    UNIT_COMPARE_EQUAL,
    UNIT_COMPARE_NOT_EQUAL,
    UNIT_COMPARE_GREATER_THAN,
    UNIT_COMPARE_LESS_THAN,
    UNIT_COMPARE_LESS_EQUAL,
    UNIT_COMPARE_GREATER_EQUAL
} UNIT_ComparisonType;

typedef struct {
    UNIT_Instruction instruction;
    UNIT_Size argument;
} _UNIT_Operation;

typedef struct {
    const char *name;
    UNIT_Size id;
    // This is a _UNIT_BasicBlock * -- we hide it to avoid exposing that type
    // publicly.
    void *_block;
} UNIT_JumpLabel;

typedef struct {
    int32_t id;
} UNIT_Local;

typedef struct {
    UNIT_Context *context;
    const char *name;
    _UNIT_Vector _instructions;
    _UNIT_Vector _global_strings;
    _UNIT_Vector _symbols;
    _UNIT_Vector _jump_labels;
    _UNIT_Vector _local_variables;
} UNIT_Procedure;

UNIT_Status
UNIT_Procedure_Init(UNIT_Procedure *procedure,
                    UNIT_Context *context, const char *name);

UNIT_Procedure *
UNIT_Procedure_New(UNIT_Context *context,
                   UNIT_Procedure *procedure, const char *name);

void
UNIT_Procedure_Clear(UNIT_Procedure *procedure);

void
UNIT_Procedure_Free(UNIT_Procedure *procedure);

UNIT_Status
UNIT_Procedure_AddOperation(UNIT_Procedure *procedure,
                            UNIT_Instruction instruction,
                            int32_t argument);

UNIT_JumpLabel *
UNIT_Procedure_CreateJumpLabel(UNIT_Procedure *procedure, const char *name);


UNIT_Status
UNIT_Procedure_UseLabel(UNIT_Procedure *procedure,
                        UNIT_JumpLabel *jump_label);

UNIT_Status
UNIT_Procedure_AddJump(UNIT_Procedure *procedure, UNIT_Instruction instruction,
                       UNIT_JumpLabel *jump_label);

UNIT_Status
UNIT_Procedure_AddCall(UNIT_Procedure *procedure,
                       const char *name,
                       UNIT_Size num_arguments);

UNIT_Status
UNIT_Procedure_AddStringLoad(UNIT_Procedure *procedure, const char *str);

UNIT_Status
UNIT_Procedure_AddStoreLocal(UNIT_Procedure *procedure, const char *name,
                             UNIT_Local *local_ptr);

UNIT_Status
UNIT_Procedure_AddLoadLocal(UNIT_Procedure *procedure, UNIT_Local local);

#endif
