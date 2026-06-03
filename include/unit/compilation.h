#ifndef UNIT_COMPILATION_H
#define UNIT_COMPILATION_H

#include <unit/base.h>
#include <unit/context.h>
#include <unit/procedure.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>

typedef enum {
    UNIT_ARCH_AMD64,
    // TODO: UNIT_ARCH_ARM64
} UNIT_Architecture;

typedef enum {
    UNIT_FORMAT_ELF
    // TODO: UNIT_FORMAT_MACHO
    // TODO: UNIT_FORMAT_PE
} UNIT_ExecutableFormat;

typedef struct {
    UNIT_Context *context;
    UNIT_Architecture architecture;
    _UNIT_Translation _translation;
    _UNIT_CompileContext _compile_context;
} UNIT_CompiledProcedure;

UNIT_CompiledProcedure *
UNIT_Compile(UNIT_Procedure *procedure, UNIT_Architecture arch);

UNIT_Status
UNIT_CompiledProcedure_WriteObjectFile(UNIT_CompiledProcedure *compiled,
                                       const char *path,
                                       UNIT_ExecutableFormat format);

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled);

#endif
