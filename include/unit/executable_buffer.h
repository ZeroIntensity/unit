#ifndef UNIT_EXECUTABLE_BUFFER_H
#define UNIT_EXECUTABLE_BUFFER_H

#include <unit/base.h>
#include <unit/compilation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    UNIT_Context *context;
    _UNIT_Map _symbols;
} UNIT_SymbolMap;

UNIT_Status
UNIT_SymbolMap_Init(UNIT_SymbolMap *symbol_map, UNIT_Context *context);

void
UNIT_SymbolMap_Clear(UNIT_SymbolMap *symbol_map);

UNIT_SymbolMap *
UNIT_SymbolMap_New(UNIT_Context *context);

void
UNIT_SymbolMap_Free(UNIT_SymbolMap *symbol_map);

UNIT_Status
UNIT_SymbolMap_RegisterSymbol(UNIT_SymbolMap *symbol_map, const char *name, void *address);

typedef struct _UNIT_ExecutableBuffer UNIT_ExecutableBuffer;

UNIT_ExecutableBuffer *
UNIT_CompiledProcedure_JIT(const UNIT_CompiledProcedure *compiled,
                           const UNIT_SymbolMap *symbol_map);

void *
UNIT_ExecutableBuffer_GetPointer(const UNIT_ExecutableBuffer *buffer);

void
UNIT_ExecutableBuffer_Free(UNIT_ExecutableBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
