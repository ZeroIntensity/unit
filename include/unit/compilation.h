#ifndef UNIT_COMPILATION_H
#define UNIT_COMPILATION_H

#include <unit/base.h>
#include <unit/context.h>
#include <unit/platform.h>
#include <unit/procedure.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UNIT_FORMAT_ELF,
    UNIT_FORMAT_MACHO,
    UNIT_FORMAT_PE,
} UNIT_ExecutableFormat;

/* Auto-detect the native object file format for the host platform */
#if defined(__APPLE__)
    #define UNIT_HOST_FORMAT UNIT_FORMAT_MACHO
#elif defined(_WIN32)
    #define UNIT_HOST_FORMAT UNIT_FORMAT_PE
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
UNIT_CompiledProcedure_PrintTranslatedIR(const UNIT_CompiledProcedure *compiled, FILE *stream);

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled);

#ifdef __cplusplus
}
#endif

#endif
