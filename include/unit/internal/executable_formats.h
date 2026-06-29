#ifndef UNIT_EXECUTABLE_FORMATS_H
#define UNIT_EXECUTABLE_FORMATS_H

#include <unit/internal/compile_context.h>

#ifdef __cplusplus
extern "C" {
#endif

UNIT_Status
_UNIT_ELF_WriteObjectFile(const _UNIT_CompileContext *context, const char *path);

UNIT_Status
_UNIT_MachO_WriteObjectFile(const _UNIT_CompileContext *context, const char *path);

#ifdef __cplusplus
}
#endif

#endif
