#ifndef UNIT_UTIL_H
#define UNIT_UTIL_H

#include <stdio.h>

#include <unit/internal/base.h>
#include <unit/internal/context.h>

// Foreground resets preserve an instruction's background color.
#define _UNIT_COLOR_INTEGER "\x1b[92m"
#define _UNIT_COLOR_STRING "\x1b[32m"
#define _UNIT_COLOR_TYPE "\x1b[1;33m"
#define _UNIT_COLOR_RESULT "\x1b[1;31m"
#define _UNIT_COLOR_INSTRUCTION "\x1b[34m"
#define _UNIT_COLOR_LABEL "\x1b[38;5;208m"
#define _UNIT_COLOR_BACKGROUND "\x1b[48;5;236m"
#define _UNIT_COLOR_FOREGROUND "\x1b[22;39m"
#define _UNIT_COLOR_RESET "\x1b[0m"
#define _UNIT_COLOR(enabled, name) ((enabled) ? _UNIT_COLOR_ ## name : "")

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

/* Print a string with newlines represented as a "\n" */
UNIT_Status
_UNIT_PrintString(UNIT_Context *context,
                  const char *string,
                  FILE *stream);

#endif
