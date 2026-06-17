#ifndef UNIT_COMPILATION_H
#define UNIT_COMPILATION_H

#include <unit/base.h>
#include <unit/context.h>
#include <unit/platform.h>
#include <unit/procedure.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>

typedef enum {
    UNIT_FORMAT_ELF,
    UNIT_FORMAT_MACHO,
    UNIT_FORMAT_PE,
} UNIT_ExecutableFormat;

typedef struct {
    UNIT_Context *context;
    UNIT_Platform platform;
    const char *name;
    _UNIT_Translation _translation;
    _UNIT_CompileContext _compile_context;
} UNIT_CompiledProcedure;

UNIT_CompiledProcedure *
UNIT_Compile(const UNIT_Procedure *procedure, UNIT_Platform platform);

UNIT_Status
UNIT_CompiledProcedure_WriteObjectFile(const UNIT_CompiledProcedure *compiled,
                                       const char *path,
                                       UNIT_ExecutableFormat format);

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled);

#endif
