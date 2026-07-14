#ifndef UNIT_OBJECT_H
#define UNIT_OBJECT_H

#include <unit/internal/allocation.h>

#ifdef __cplusplus
extern "C" {
#endif

#define _UNIT_Structure_NEW_IMPL(type, ...)                     \
        /* This cast has to be here for C++ compatibility */    \
        type *ptr = (type *)_UNIT_Alloc(context, sizeof(type)); \
        if (ptr == NULL) {                                      \
            return NULL;                                        \
        }                                                       \
        if (UNIT_FAILED(type ## _Init(ptr, __VA_ARGS__))) {     \
            return NULL;                                        \
        }                                                       \
        return ptr;

#define _UNIT_Structure_DEFINE_PUBLIC_FREE(type) \
        void type ## _Free(type * ptr) {         \
            assert(ptr != NULL);                 \
            type ## _Clear(ptr);                 \
            _UNIT_Dealloc(ptr->context, ptr);    \
        }

#define _UNIT_Structure_DEFINE_FREE(type)              \
        static inline void type ## _Free(type * ptr) { \
            assert(ptr != NULL);                       \
            type ## _Clear(ptr);                       \
            _UNIT_Dealloc(ptr->context, ptr);          \
        }

// Note: Do not use this macro for Free() functions that are intended
// to be used as part of the public API, because they won't
// be accessible to an FFI.
#define _UNIT_Structure_CLEAR_AND_FREE(type) \
        void type ## _Clear(type * ptr);     \
        _UNIT_Structure_DEFINE_FREE(type)    \

#ifdef __cplusplus
}
#endif

#endif
