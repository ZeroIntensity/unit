#ifndef UNIT_OPTIMIZATION_H
#define UNIT_OPTIMIZATION_H

#include <unit/procedure.h>

#ifdef __cplusplus
extern "C" {
#endif

UNIT_Status
UNIT_Procedure_OptimizeFold(UNIT_Procedure *procedure);

UNIT_Status
UNIT_Procedure_OptimizeInline(UNIT_Procedure *procedure);

UNIT_Status
UNIT_Procedure_Optimize(UNIT_Procedure *procedure);

#ifdef __cplusplus
}
#endif

#endif
