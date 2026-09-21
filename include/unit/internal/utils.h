#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

#include <stdio.h>

#include <unit/internal/base.h>
#include <unit/internal/context.h>

UNIT_Status
_UNIT_File_WriteU8(UNIT_Context *context, FILE *file, uint8_t value);

UNIT_Status
_UNIT_File_WriteU16(UNIT_Context *context, FILE *file, uint16_t value);

UNIT_Status
_UNIT_File_WriteU32(UNIT_Context *context, FILE *file, uint32_t value);

UNIT_Status
_UNIT_File_WriteU64(UNIT_Context *context, FILE *file, uint64_t value);

UNIT_Status
_UNIT_File_WriteI16(UNIT_Context *context, FILE *file, int16_t value);

UNIT_Status
_UNIT_File_WriteI64(UNIT_Context *context, FILE *file, int64_t value);

UNIT_Status
_UNIT_File_WriteBytes(UNIT_Context *context, FILE *file, const void *data, size_t num_bytes);

#endif
