#ifndef UNIT_REGISTER_ALLOCATION_H
#define UNIT_REGISTER_ALLOCATION_H

#include <unit/base.h>

#include <unit/internal/compile_context.h>
#include <unit/internal/translation.h>

UNIT_Status
_UNIT_Translation_AllocateRegisters(_UNIT_Translation *translation,
                                    _UNIT_CompileContext *compile_context,
                                    int8_t num_registers);

#endif
