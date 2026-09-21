#include <stdio.h>

#include <unit/internal/context.h>
#include <unit/internal/errors.h>

#define WRITE_INT_IMPL(size_bytes)                        \
        assert(context != NULL);                          \
        assert(file != NULL);                             \
        if (fwrite(&value, size_bytes, 1, file) != 1) {   \
            _UNIT_SetOSError(context, "writing integer"); \
            return _UNIT_FAIL;                            \
        }                                                 \
        return _UNIT_OK;

UNIT_Status
_UNIT_File_WriteU8(UNIT_Context *context, FILE *file, uint8_t value)
{
    WRITE_INT_IMPL(1);
}

UNIT_Status
_UNIT_File_WriteU16(UNIT_Context *context, FILE *file, uint16_t value)
{
    WRITE_INT_IMPL(2);
}

UNIT_Status
_UNIT_File_WriteU32(UNIT_Context *context, FILE *file, uint32_t value)
{
    WRITE_INT_IMPL(4);
}

UNIT_Status
_UNIT_File_WriteI16(UNIT_Context *context, FILE *file, int16_t value)
{
    WRITE_INT_IMPL(2);
}

UNIT_Status
_UNIT_File_WriteBytes(UNIT_Context *context, FILE *file, const void *data, size_t num_bytes)
{
    assert(context != NULL);
    assert(file != NULL);
    assert(data != NULL);
    if (num_bytes == 0) {
        return _UNIT_OK;
    }

    if (fwrite(data, 1, num_bytes, file) != num_bytes) {
        _UNIT_SetOSError(context, "writing bytes");
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}
