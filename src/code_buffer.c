#include <string.h> // memcpy

#include <unit/internal/allocation.h>
#include <unit/internal/code_buffer.h>

#define CODE_BUFFER_INITIAL_SIZE 256

UNIT_Status
_UNIT_CodeBuffer_Init(_UNIT_CodeBuffer *buffer, UNIT_Context *context)
{
    assert(buffer != NULL);
    assert(context != NULL);
    buffer->data = _UNIT_Alloc(context, CODE_BUFFER_INITIAL_SIZE);
    if (buffer->data == NULL) {
        return _UNIT_FAIL;
    }
    buffer->capacity = CODE_BUFFER_INITIAL_SIZE;
    buffer->size = 0;
    buffer->context = context;
    return _UNIT_OK;
}

void
_UNIT_CodeBuffer_Clear(_UNIT_CodeBuffer *buffer)
{
    assert(buffer != NULL);
    assert(buffer->size >= 0);
    assert(buffer->capacity >= CODE_BUFFER_INITIAL_SIZE);
    assert(buffer->size < buffer->capacity);
    _UNIT_Dealloc(buffer->context, buffer->data);
}

static inline UNIT_Status
ensure_buffer_capacity(_UNIT_CodeBuffer *buffer, uint8_t amount_to_add)
{
    assert(buffer != NULL);
    assert(buffer->size >= 0);
    assert(buffer->capacity >= CODE_BUFFER_INITIAL_SIZE);
    assert(buffer->size < buffer->capacity);
    if ((amount_to_add + buffer->size) < buffer->capacity) {
        return _UNIT_OK;
    }

    uint8_t *new_buffer = _UNIT_Realloc(buffer->context, buffer->data,
                                        buffer->capacity * 2);
    if (new_buffer == NULL) {
        return _UNIT_FAIL;
    }
    buffer->data = new_buffer;
    buffer->capacity *= 2;
    return _UNIT_OK;
}

UNIT_Status
_UNIT_CodeBuffer_Emit8(_UNIT_CodeBuffer *buffer, uint8_t value)
{
    assert(buffer != NULL);
    if (UNIT_FAILED(ensure_buffer_capacity(buffer, 1))) {
        return _UNIT_FAIL;
    }
    buffer->data[buffer->size++] = value;
    return _UNIT_OK;
}

UNIT_Status
_UNIT_CodeBuffer_Emit32(_UNIT_CodeBuffer *buffer, uint32_t value)
{
    assert(buffer != NULL);
    if (UNIT_FAILED(ensure_buffer_capacity(buffer, 4))) {
        return _UNIT_FAIL;
    }
    memcpy(buffer->data + buffer->size, &value, 4);
    buffer->size += 4;
    return _UNIT_OK;
}

UNIT_Status
_UNIT_CodeBuffer_Emit64(_UNIT_CodeBuffer *buffer, uint64_t value)
{
    assert(buffer != NULL);
    if (UNIT_FAILED(ensure_buffer_capacity(buffer, 8))) {
        return _UNIT_FAIL;
    }
    memcpy(buffer->data + buffer->size, &value, 8);
    buffer->size += 8;
    return _UNIT_OK;
}

void
_UNIT_CodeBuffer_Patch32(_UNIT_CodeBuffer *buffer,
                         UNIT_Size offset,
                         int32_t value)
{
    assert(buffer != NULL);
    assert((offset + 4) < buffer->size);
    buffer->data[offset + 0] = (value >> 0) & 0xFF;
    buffer->data[offset + 1] = (value >> 8) & 0xFF;
    buffer->data[offset + 2] = (value >> 16) & 0xFF;
    buffer->data[offset + 3] = (value >> 24) & 0xFF;
}

UNIT_Size
_UNIT_CodeBuffer_Reserve(_UNIT_CodeBuffer *buffer, UNIT_Size count)
{
    UNIT_Size offset = buffer->size;
    for (UNIT_Size i = 0; i < count; ++i) {
        _UNIT_CodeBuffer_Emit8(buffer, 0x00);
    }
    return offset;
}

void
_UNIT_CodeBuffer_PatchBytes(_UNIT_CodeBuffer *buffer, UNIT_Size offset,
                            const uint8_t *bytes, UNIT_Size count)
{
    for (UNIT_Size i = 0; i < count; ++i) {
        buffer->data[offset + i] = bytes[i];
    }
}
