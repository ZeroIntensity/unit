#ifndef UNIT_COMPILE_CONTEXT_H
#define UNIT_COMPILE_CONTEXT_H

#include <unit/base.h>
#include <unit/procedure.h>

#include <unit/internal/code_buffer.h>
#include <unit/internal/size_map.h>
#include <unit/internal/translation.h>
#include <unit/internal/vector.h>

typedef enum {
    RELOCATION_CALL,
    RELOCATION_DATA,
} _UNIT_RelocationType;

typedef struct {
    UNIT_Size offset;
    UNIT_Size symbol_index;
    _UNIT_RelocationType type;
} _UNIT_Relocation;

_UNIT_Relocation *
_UNIT_Relocation_NewCall(UNIT_Context *context, UNIT_Size offset,
                         UNIT_Size symbol_index);

_UNIT_Relocation *
_UNIT_Relocation_NewData(UNIT_Context *context, UNIT_Size offset,
                         UNIT_Size symbol_index);

void
_UNIT_Relocation_Free(UNIT_Context *context, _UNIT_Relocation *relocation);

typedef struct {
    /* Contains index -> `char *` (heap allocated). */
    const _UNIT_Vector *names;
    /* Contains `_UNIT_PendingJump *` */
    _UNIT_Vector relocations;
} _UNIT_SymbolTable;

UNIT_Status
_UNIT_SymbolTable_Init(_UNIT_SymbolTable *symbol_table, UNIT_Context *context,
                       const _UNIT_Vector *names);

void
_UNIT_SymbolTable_Clear(_UNIT_SymbolTable *symbol_table);

typedef struct {
    UNIT_Size patch_offset; // where the 4-byte displacement starts
    UNIT_Size label_index; // which label to jump to
} _UNIT_PendingJump;

_UNIT_PendingJump *
_UNIT_PendingJump_New(UNIT_Context *context, UNIT_Size patch_offset,
                      UNIT_Size label_index);

void
_UNIT_PendingJump_Free(UNIT_Context *context, _UNIT_PendingJump *pending_jump);

typedef struct {
    /*
     * Vector of jumps that need patching.
     * Contains `_UNIT_PendingJump *`
     */
    _UNIT_Vector pending_jumps;
    /* Records label index -> byte offset */
    _UNIT_SizeMap label_offsets;
} _UNIT_JumpTable;

UNIT_Status
_UNIT_JumpTable_Init(_UNIT_JumpTable *jump_table, UNIT_Context *context);

void
_UNIT_JumpTable_Clear(_UNIT_JumpTable *jump_table);

typedef struct {
    _UNIT_CodeBuffer constant_buffer;
    /* String index -> byte offset */
    _UNIT_SizeMap string_offsets;
} _UNIT_StringData;

UNIT_Status
_UNIT_StringData_Init(_UNIT_StringData *string_data, UNIT_Context *context);

void
_UNIT_StringData_Clear(_UNIT_StringData *string_data);

#define _UNIT_StackFrame_MAX_FREE_SLOTS 32

typedef struct {
    UNIT_Size next_slot;
    UNIT_Size free_slots[_UNIT_StackFrame_MAX_FREE_SLOTS];
    UNIT_Size free_slot_count;
    UNIT_Size reserved_slots;
} _UNIT_StackFrame;

UNIT_Size
_UNIT_StackFrame_AllocateSlot(_UNIT_StackFrame *frame);

void
_UNIT_StackFrame_FreeSlot(_UNIT_StackFrame *frame, UNIT_Size slot);

UNIT_Size
_UNIT_StackFrame_AllocateSlotID(_UNIT_StackFrame *frame);

void
_UNIT_StackFrame_FreeSlotID(_UNIT_StackFrame *frame, UNIT_Size slot_id);

UNIT_Size
_UNIT_StackFrame_ComputeSize(_UNIT_StackFrame *frame);

typedef struct {
    UNIT_Context *context;
    _UNIT_CodeBuffer buffer;
    _UNIT_SymbolTable symbol_table;
    _UNIT_JumpTable jump_table;
    _UNIT_StringData string_data;
    _UNIT_StackFrame stack_frame;
} _UNIT_CompileContext;

UNIT_Status
_UNIT_CompileContext_Init(_UNIT_CompileContext *compile_context,
                          UNIT_Context *context,
                          const UNIT_Procedure *procedure,
                          const _UNIT_Translation *translation);

void
_UNIT_CompileContext_Clear(_UNIT_CompileContext *compile_context);

#endif
