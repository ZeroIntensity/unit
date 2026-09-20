#ifndef UNIT_COMPILATION_H
#define UNIT_COMPILATION_H

#include <unit/internal/base.h>
#include <unit/internal/context.h>
#include <unit/internal/platform.h>
#include <unit/internal/ir/procedure.h>

#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/ir/translation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UNIT_FORMAT_ELF,
    UNIT_FORMAT_MACHO,
    UNIT_FORMAT_COFF,
} UNIT_ExecutableFormat;

#if defined(__APPLE__)
    #define UNIT_HOST_FORMAT UNIT_FORMAT_MACHO
#elif defined(_WIN32)
    #define UNIT_HOST_FORMAT UNIT_FORMAT_COFF
#else
    #define UNIT_HOST_FORMAT UNIT_FORMAT_ELF
#endif

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

UNIT_Status
UNIT_CompiledProcedure_PrintTranslatedIR(
    const UNIT_CompiledProcedure *compiled,
    FILE *stream);

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled);

#ifdef __cplusplus
}
#endif

#endif
