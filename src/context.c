#include <string.h> // memset
#include <stdlib.h> // malloc, free

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

UNIT_Status
UNIT_Context_Init(UNIT_Context *context)
{
    assert(context != NULL);
    memset(context, 0, sizeof(UNIT_Context));
    return _UNIT_OK;
}

UNIT_Context *
UNIT_Context_New(void)
{
    UNIT_Context *context = malloc(sizeof(UNIT_Context));
    if (context == NULL) {
        return NULL;
    }

    if (UNIT_FAILED(UNIT_Context_Init(context))) {
        return NULL;
    }

    return context;
}

void
UNIT_Context_Clear(UNIT_Context *context)
{
    assert(context != NULL);
    _UNIT_ClearFreelists(context);
}

void
UNIT_Context_Free(UNIT_Context *context)
{
    UNIT_Context_Clear(context);
    free(context);
}
