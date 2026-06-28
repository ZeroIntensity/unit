#ifndef UNIT_TRANSLATION_H
#define UNIT_TRANSLATION_H

#include <stdint.h>

#include <unit/procedure.h>

#include <unit/internal/map.h>
#include <unit/internal/size_map.h>
#include <unit/internal/size_set.h>
#include <unit/internal/vector.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    _UNIT_TYPE_CONSTANT,
    _UNIT_TYPE_LOCATION,
    _UNIT_TYPE_REGISTER,
    _UNIT_TYPE_CALL_ARGS,
    _UNIT_TYPE_COMPARISON,
    _UNIT_TYPE_MEMORY
} _UNIT_MachineItem_Type;

typedef struct _UNIT_MachineItem {
    _UNIT_MachineItem_Type type;
    union {
        int64_t value;
        _UNIT_Vector *call_args;
        struct {
            struct _UNIT_MachineItem *left;
            struct _UNIT_MachineItem *right;
            UNIT_Instruction type; // One of the comparison instructions
        } comparison;
    };
    const char *hint;
    // Next node in the machine item linked list
    struct _UNIT_MachineItem *next;
    struct {
        const char *name;
        UNIT_Size index;
    } created_by;
} _UNIT_MachineItem;

typedef struct {
    uintptr_t _tagged;
} _UNIT_MachineDestination;

#define _UNIT_MachineDestination_NULL ((_UNIT_MachineDestination) {._tagged = 0})

static inline _UNIT_MachineDestination
_UNIT_MachineDestination_FromDestination(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    assert(((uintptr_t)item & 1) == 0); // Must be aligned
    return (_UNIT_MachineDestination){ (uintptr_t)item };
}

static inline _UNIT_MachineDestination
_UNIT_MachineDestination_FromInput(_UNIT_MachineItem *item)
{
    assert(item != NULL);
    assert(((uintptr_t)item & 1) == 0);
    return (_UNIT_MachineDestination){ (uintptr_t)item | 1 };
}

static inline int8_t
_UNIT_MachineDestination_IsInput(_UNIT_MachineDestination dest)
{
    return (int8_t)(dest._tagged & 1);
}

static inline int8_t
_UNIT_MachineDestination_IsNull(_UNIT_MachineDestination dest)
{
    return dest._tagged == 0;
}

static inline _UNIT_MachineItem *
_UNIT_MachineDestination_GetPointer(_UNIT_MachineDestination dest)
{
    assert(dest._tagged != 0);
    return (_UNIT_MachineItem *)(dest._tagged & ~(uintptr_t)1);
}

static inline _UNIT_MachineItem *
_UNIT_MachineDestination_GetPointerNullable(_UNIT_MachineDestination dest)
{
    if (_UNIT_MachineDestination_IsNull(dest)) {
        return NULL;
    }
    return _UNIT_MachineDestination_GetPointer(dest);
}

typedef enum {
    // General
    _UNIT_I_LOAD,
    _UNIT_I_CALL_SYMBOL,
    _UNIT_I_LOAD_STRING,

    // Functions
    _UNIT_I_EXIT,
    _UNIT_I_RETURN_VALUE,
    _UNIT_I_LOAD_ARGUMENT,

    // Jumps
    _UNIT_I_JUMP_LABEL,
    _UNIT_I_JUMP,
    _UNIT_I_JUMP_IF_EQUAL,
    _UNIT_I_JUMP_IF_NOT_EQUAL,
    _UNIT_I_JUMP_IF_GREATER,
    _UNIT_I_JUMP_IF_LESS,
    _UNIT_I_JUMP_IF_GREATER_EQUAL,
    _UNIT_I_JUMP_IF_LESS_EQUAL,

    // Comparisons
    _UNIT_I_COMPARE_EQUAL,

    // Arithmetic
    _UNIT_I_ADD,
    _UNIT_I_SUB,
    _UNIT_I_MUL,
    _UNIT_I_DIV,
    _UNIT_I_MOD,

    // Casting
    _UNIT_I_CONVERT,

    // Pointers
    _UNIT_I_READ_BYTES,
    _UNIT_I_WRITE_BYTES,
    _UNIT_I_ADDRESS_OF,
} _UNIT_MachineInstruction;

typedef struct {
    _UNIT_MachineInstruction instruction;
    _UNIT_MachineDestination destination;
    _UNIT_MachineItem *argument_1;
    _UNIT_MachineItem *argument_2;
} _UNIT_MachineOperation;

typedef struct {
    UNIT_Context *context;
    _UNIT_Vector blocks; // Holds _UNIT_BasicBlock
    _UNIT_SizeMap symbols;
    _UNIT_Map strings; // UNIT_Size -> char*
    _UNIT_MachineItem *item_list_head; // Head of machine item linked list
    UNIT_Size num_memory_slots; // Not a huge fan of this but it'll do
} _UNIT_Translation;

UNIT_Status
_UNIT_Translate(_UNIT_Translation *translation,
                const UNIT_Procedure *procedure);

void
_UNIT_Translation_Clear(_UNIT_Translation *translation);

UNIT_Status
_UNIT_Translation_PrintInstructions(const _UNIT_Translation *translation, const char *name,
                                    FILE *stream);

UNIT_Status
_UNIT_Translation_Optimize(_UNIT_Translation *translation, int8_t num_registers);

#ifdef __cplusplus
}
#endif

#endif
