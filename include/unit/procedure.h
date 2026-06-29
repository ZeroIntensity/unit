#ifndef UNIT_PROCEDURE_H
#define UNIT_PROCEDURE_H

#include <stdio.h>

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/map.h>
#include <unit/internal/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    // Constants
    UNIT_OP_LOAD_STRING,
    UNIT_OP_LOAD_INTEGER,

    // Variables
    UNIT_OP_LOAD_LOCAL,
    UNIT_OP_STORE_LOCAL,
    _UNIT_OP_LOAD_LOCAL_NAME,
    _UNIT_OP_STORE_LOCAL_NAME,

    // Arithmetic
    UNIT_OP_ADD,
    UNIT_OP_SUBTRACT,
    UNIT_OP_MULTIPLY,
    UNIT_OP_DIVIDE,
    UNIT_OP_MODULO,

    // Jumps
    _UNIT_OP_JUMP_MARKER,
    UNIT_OP_JUMP_TO,
    UNIT_OP_JUMP_IF_FALSE,
    UNIT_OP_JUMP_IF_TRUE,

    // Functions
    UNIT_OP_EXIT,
    UNIT_OP_RETURN_VALUE,
    UNIT_OP_LOAD_ARGUMENT,

    // Function calls
    UNIT_OP_PREPARE_CALL,
    UNIT_OP_CALL_NAME,
    UNIT_OP_CALL_PROCEDURE,

    // Comparisons
    UNIT_OP_COMPARE_EQUAL,
    UNIT_OP_COMPARE_NOT_EQUAL,
    UNIT_OP_COMPARE_GREATER,
    UNIT_OP_COMPARE_GREATER_EQUAL,
    UNIT_OP_COMPARE_LESS,
    UNIT_OP_COMPARE_LESS_EQUAL,

    // Stack manipulation
    UNIT_OP_COPY,
    UNIT_OP_SWAP,
    UNIT_OP_POP,

    // Pointers
    UNIT_OP_ADDRESS_OF,
    UNIT_OP_READ_BYTES,
    UNIT_OP_WRITE_BYTES,

    // Casting
    UNIT_OP_CONVERT
} UNIT_Instruction;

typedef struct {
    UNIT_Instruction instruction;
    int64_t argument;
} _UNIT_Operation;

typedef struct {
    char *name; // heap-allocated
    int32_t id;
    // This is a _UNIT_BasicBlock * -- we hide it to avoid exposing that type
    // publicly.
    void *_block;
} UNIT_JumpLabel;

void
_UNIT_JumpLabel_Free(UNIT_Context *context, void *ptr);

typedef struct {
    int32_t id;
} UNIT_Local;

typedef enum {
    UNIT_TYPE_INT8,
    UNIT_TYPE_INT16,
    UNIT_TYPE_INT32,
    UNIT_TYPE_INT64,
    UNIT_TYPE_UINT8,
    UNIT_TYPE_UINT16,
    UNIT_TYPE_UINT32,
    UNIT_TYPE_UINT64,
} UNIT_IntegerType;

typedef uint32_t UNIT_Flags;

typedef struct {
    UNIT_Context *context;
    char *name;
    UNIT_Flags flags;
    _UNIT_Vector _instructions; // Contains _UNIT_Operation * (heap-allocated)
    _UNIT_Vector _global_strings; // Contains char * (heap-allocated)
    _UNIT_Vector _symbols; // Contains char * (heap-allocated)
    _UNIT_Vector _jump_labels; // Contains UNIT_JumpLabel * (heap-allocated)
    _UNIT_Vector _local_variables; // Contains char * (heap-allocated)
    _UNIT_Vector _subprocedures; // Contains UNIT_Procedure * (borrowed)
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
                            int64_t argument);

UNIT_JumpLabel *
UNIT_Procedure_CreateJumpLabel(UNIT_Procedure *procedure, const char *name);


UNIT_Status
UNIT_Procedure_UseLabel(UNIT_Procedure *procedure,
                        UNIT_JumpLabel *jump_label);

UNIT_Status
UNIT_Procedure_AddJump(UNIT_Procedure *procedure, UNIT_Instruction instruction,
                       UNIT_JumpLabel *jump_label);

UNIT_Status
UNIT_Procedure_AddCallName(UNIT_Procedure *procedure,
                           const char *name,
                           UNIT_Size num_arguments);

UNIT_Status
UNIT_Procedure_AddStringLoad(UNIT_Procedure *procedure, const char *str);

UNIT_Status
UNIT_Procedure_CreateLocal(UNIT_Procedure *procedure, const char *name,
                        UNIT_Local *local_ptr);

UNIT_Status
UNIT_Procedure_AddStoreName(UNIT_Procedure *procedure, UNIT_Local local);

UNIT_Status
UNIT_Procedure_AddLoadName(UNIT_Procedure *procedure, UNIT_Local local);

UNIT_Status
UNIT_Procedure_AddCallProcedure(UNIT_Procedure *self,
                                UNIT_Procedure *target,
                                uint8_t nargs);

const char *
UNIT_Instruction_GetName(UNIT_Instruction instruction);

UNIT_Status
UNIT_Procedure_PrintInstructions(const UNIT_Procedure *procedure,
                                 FILE *stream, int8_t visualize_stack_effect);

#define UNIT_FLAG_NONE 0
#define UNIT_FLAG_NO_OPTIMIZE_TRANSLATION (1 << 0)
#define UNIT_FLAG_FORCE_NO_INLINE (1 << 1)
#define UNIT_FLAG_FORCE_INLINE (1 << 2)
#define UNIT_FLAG_PRINT_TRANSLATION_PREOP (1 << 3)
#define UNIT_FLAG_PRINT_TRANSLATION_POSTOP (1 << 4)

void
UNIT_Procedure_SetFlags(UNIT_Procedure *procedure, UNIT_Flags flags);

UNIT_Flags
UNIT_Procedure_GetFlags(const UNIT_Procedure *procedure);

#ifdef __cplusplus
}
#endif

#endif
