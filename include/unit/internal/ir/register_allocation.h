#ifndef UNIT_REGISTER_ALLOCATION_H
#define UNIT_REGISTER_ALLOCATION_H

#include <unit/internal/base.h>
#include <unit/internal/compilation/compile_context.h>
#include <unit/internal/ir/translation.h>

#ifdef __cplusplus
extern "C" {
#endif

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    _UNIT_CompileContext *compile_context,
                                    int8_t num_registers);

#ifdef __cplusplus
}
#endif

#endif
