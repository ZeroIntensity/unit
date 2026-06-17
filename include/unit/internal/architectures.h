#ifndef UNIT_ARCHITECTURES_H
#define UNIT_ARCHITECTURES_H

#include <unit/base.h>
#include <unit/platform.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>

UNIT_Status
_UNIT_AMD64_Compile(_UNIT_Translation *translation,
                    _UNIT_CompileContext *context,
                    UNIT_ABI abi);

#endif
