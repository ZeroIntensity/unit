#include <unit/internal/version.h>

const char *
UNIT_GetVersion(void)
{
    return UNIT_VERSION_STRING;
}

uint32_t
UNIT_GetVersionHex(void)
{
    return UNIT_VERSION_HEX;
}
