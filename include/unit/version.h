#ifndef UNIT_VERSION_H
#define UNIT_VERSION_H

#include <stdint.h>

#define _UNIT_STRINGIFY(x) #x
#define _UNIT_TOSTRING(x) _UNIT_STRINGIFY(x)

#define UNIT_VERSION_MAJOR 0
#define UNIT_VERSION_MINOR 1
#define UNIT_VERSION_PATCH 0
#define UNIT_VERSION_DEV 1  // 0 = release, 1+ = dev revision

#define UNIT_PACK_VERSION(major, minor, patch) \
        (((major) << 24) | ((minor) << 16) | ((patch) << 8) | 0xff)

#define UNIT_PACK_VERSION_FULL(major, minor, patch, dev)                  \
        (((major) << 24) | ((minor) << 16) | ((patch) << 8) | ((dev) ==   \
                                                               0 ? 0xff : \
                                                               (dev)))

#define UNIT_VERSION_HEX                           \
        UNIT_PACK_VERSION_FULL(UNIT_VERSION_MAJOR, \
                               UNIT_VERSION_MINOR, \
                               UNIT_VERSION_PATCH, \
                               UNIT_VERSION_DEV)

#if UNIT_VERSION_DEV == 0
    #define UNIT_VERSION_STRING                    \
            _UNIT_TOSTRING(UNIT_VERSION_MAJOR) "." \
            _UNIT_TOSTRING(UNIT_VERSION_MINOR) "." \
            _UNIT_TOSTRING(UNIT_VERSION_PATCH)
#else
    #define UNIT_VERSION_STRING                       \
            _UNIT_TOSTRING(UNIT_VERSION_MAJOR) "."    \
            _UNIT_TOSTRING(UNIT_VERSION_MINOR) "."    \
            _UNIT_TOSTRING(UNIT_VERSION_PATCH) ".dev" \
            _UNIT_TOSTRING(UNIT_VERSION_DEV)
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *
UNIT_GetVersion(void);

uint32_t
UNIT_GetVersionHex(void);

#ifdef __cplusplus
}
#endif

#endif
