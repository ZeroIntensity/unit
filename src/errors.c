#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unit/context.h>

const char *
UNIT_ErrorCode_ToString(UNIT_ErrorCode code)
{
    switch (code) {
        case UNIT_ERROR_NONE:
            return "NONE";
        case UNIT_ERROR_INVALID_USAGE:
            return "INVALID_USAGE";
        case UNIT_ERROR_NO_MEMORY:
            return "NO_MEMORY";
        case UNIT_ERROR_OS_FAILURE:
            return "OS_FAILURE";
    }
}

UNIT_ErrorCode
UNIT_Error_GetCode(UNIT_Context *context)
{
    assert(context != NULL);
    return context->_internal.error.code;
}

void
UNIT_Error_Clear(UNIT_Context *context)
{
    assert(context != NULL);
    context->_internal.error.code = UNIT_ERROR_NONE;
    memset(context->_internal.error.message, 0, 256);
}

const char *
UNIT_GetErrorMessage(UNIT_Context *context)
{
    assert(context != NULL);
    return context->_internal.error.message;
}

void
UNIT_Error_Print(UNIT_Context *context, FILE *stream)
{
    assert(context != NULL);
    assert(stream != NULL);
    UNIT_ErrorCode code = context->_internal.error.code;
    if (code == UNIT_ERROR_NONE) {
        fprintf(stream, "no error occurred\n");
    } else {
        fprintf(stream, "%s: %s\n", UNIT_ErrorCode_ToString(code),
                context->_internal.error.message);
    }
}

void
_UNIT_SetError(UNIT_Context *context, UNIT_ErrorCode code, const char *message)
{
    assert(context != NULL);
    assert(message != NULL);
    context->_internal.error.code = code;
    strncpy(context->_internal.error.message, message, 256);
}

void
_UNIT_SetErrorFormat(UNIT_Context *context, UNIT_ErrorCode code,
                     const char *format, ...)
{
    assert(context != NULL);
    assert(format != NULL);
    va_list vargs;
    va_start(vargs, format);
    vsnprintf(context->_internal.error.message, 256, format, vargs);
    va_end(vargs);
    context->_internal.error.code = code;
}

void
_UNIT_SetOSError(UNIT_Context *context, const char *what)
{
    _UNIT_SetErrorFormat(context, UNIT_ERROR_OS_FAILURE,
                         "OS error while %s (errno %d): %s", what,
                         errno, strerror(errno));
}
