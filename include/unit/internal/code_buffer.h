#ifndef UNIT_CODE_BUFFER_H
#define UNIT_CODE_BUFFER_H

#include <unit/base.h>
#include <unit/context.h>

#include <unit/internal/structure.h>

typedef struct {
    UNIT_Context *context;
    uint8_t *data;
    UNIT_Size capacity;
    UNIT_Size size;
} _UNIT_CodeBuffer;

UNIT_Status
_UNIT_CodeBuffer_Init(_UNIT_CodeBuffer *buffer, UNIT_Context *context);

static inline _UNIT_CodeBuffer *
_UNIT_CodeBuffer_New(UNIT_Context *context)
{
    _UNIT_Structure_NEW_IMPL(_UNIT_CodeBuffer, context);
}

_UNIT_Structure_CLEAR_AND_FREE(_UNIT_CodeBuffer)

UNIT_Status
_UNIT_CodeBuffer_Emit8(_UNIT_CodeBuffer *buffer, uint8_t value);

UNIT_Status
_UNIT_CodeBuffer_Emit32(_UNIT_CodeBuffer *buffer, uint32_t value);

UNIT_Status
_UNIT_CodeBuffer_Emit64(_UNIT_CodeBuffer *buffer, uint64_t value);

static inline UNIT_Size
_UNIT_CodeBuffer_CurrentIndex(const _UNIT_CodeBuffer *buffer)
{
    assert(buffer != NULL);
    assert(buffer->size >= 0);
    return buffer->size;
}

void
_UNIT_CodeBuffer_Patch32(_UNIT_CodeBuffer *buffer,
                         UNIT_Size offset,
                         int32_t value);

// Writes N zero bytes and returns the offset where they start.
UNIT_Size
_UNIT_CodeBuffer_Reserve(_UNIT_CodeBuffer *buffer, UNIT_Size count);

// Write a full instruction into reserved space
void
_UNIT_CodeBuffer_PatchBytes(_UNIT_CodeBuffer *buffer, UNIT_Size offset,
                            const uint8_t *bytes, UNIT_Size count);

#endif
