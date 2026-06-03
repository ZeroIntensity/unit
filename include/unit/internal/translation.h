#ifndef UNIT_TRANSLATION_H
#define UNIT_TRANSLATION_H

#include <stdint.h>

#include <unit/procedure.h>

#include <unit/internal/map.h>
#include <unit/internal/size_map.h>
#include <unit/internal/size_set.h>
#include <unit/internal/vector.h>

typedef enum {
    CONSTANT,
    LOCATION,
    REGISTER,
    CALL_ARGS,
    COMPARISON,
    MEMORY
} _UNIT_MachineItem_Type;

typedef struct _UNIT_MachineItem {
    _UNIT_MachineItem_Type type;
    union {
        int32_t value;
        _UNIT_Vector *call_args;
        struct {
            struct _UNIT_MachineItem *left;
            struct _UNIT_MachineItem *right;
            UNIT_ComparisonType type;
        } comparison;
    };
    const char *hint;
    // Next node in the machine item linked list
    struct _UNIT_MachineItem *next;
} _UNIT_MachineItem;

typedef enum {
    _UNIT_I_MOVE,
    _UNIT_I_ADD,
    _UNIT_I_JUMP_LABEL,
    _UNIT_I_JUMP,
    _UNIT_I_CALL_SYMBOL,
    _UNIT_I_EXIT,
    _UNIT_I_RETURN_VALUE,
    _UNIT_I_COMPARE_EQUAL,
    _UNIT_I_JUMP_IF_EQUAL,
    _UNIT_I_JUMP_IF_NOT_EQUAL,
    _UNIT_I_JUMP_IF_GREATER,
    _UNIT_I_JUMP_IF_LESS,
    _UNIT_I_JUMP_IF_GREATER_EQUAL,
    _UNIT_I_JUMP_IF_LESS_EQUAL,
    _UNIT_I_LOAD_STRING,
    _UNIT_I_ADDRESS_OF,
} _UNIT_MachineInstruction;

typedef struct {
    _UNIT_MachineInstruction instruction;
    _UNIT_MachineItem *destination;
    _UNIT_MachineItem *argument_1;
    _UNIT_MachineItem *argument_2;
} _UNIT_MachineOperation;

static const UNIT_Size _UNIT_BasicBlock_NO_LABEL = -1;

typedef struct {
    _UNIT_SizeSet created_locations;
    _UNIT_SizeSet used_locations;
    _UNIT_SizeSet alive_at_start;
    _UNIT_SizeSet alive_at_end;
} _UNIT_LivenessInfo;

typedef struct {
    UNIT_Context *context;
    UNIT_Size id;
    UNIT_Size label_id; // or _UNIT_BasicBlock_NO_LABEL
    _UNIT_Vector instructions; // Holds _UNIT_MachineOperation*
    _UNIT_Vector successors; // Holds (unowned) _UNIT_BasicBlock*
    _UNIT_LivenessInfo liveness;
} _UNIT_BasicBlock;

typedef struct {
    UNIT_Context *context;
    _UNIT_Vector blocks; // Holds _UNIT_BasicBlock
    _UNIT_SizeMap symbols;
    _UNIT_Map strings; // UNIT_Size -> char*
    _UNIT_MachineItem *item_list_head; // Head of machine item linked list
    UNIT_Size num_memory_slots; // Not a huge fan of this but it'll do
} _UNIT_Translation;

UNIT_Status
_UNIT_Translation_InitFromProcedure(_UNIT_Translation *translation,
                                    UNIT_Procedure *procedure);

void
_UNIT_Translation_Clear(_UNIT_Translation *translation);

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation, int8_t num_registers);

void
_UNIT_Translation_PrintInstructions(_UNIT_Translation *translation);

#endif
