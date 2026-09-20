#ifndef UNIT_ARCHITECTURES_H
#define UNIT_ARCHITECTURES_H

#include <unit/internal/base.h>
#include <unit/internal/platform.h>

#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/ir/translation.h>

#ifdef __cplusplus
extern "C" {
#endif

UNIT_Status
_UNIT_AMD64_Compile(_UNIT_Translation *translation,
                    _UNIT_CompileContext *context);

#ifdef __cplusplus
}
#endif

#endif
