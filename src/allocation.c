#include <stdlib.h>
#include <string.h>

#include <unit/base.h>
#include <unit/context.h>
#include <unit/errors.h>

static void *
freelist_pop(_UNIT_Freelist **freelist)
{
    assert(freelist != NULL);
    _UNIT_Freelist *top = *freelist;
    if (top == NULL) {
        return NULL;
    }

    *freelist = top->next;
    return top;
}

static void
freelist_push(void *ptr, _UNIT_Freelist **freelist)
{
    _UNIT_Freelist *node = ptr;
    node->next = *freelist;
    *freelist = node;
}

typedef struct {
    UNIT_Size size_class;
} UNIT_FreelistHeader;

static inline void *
malloc_with_header(UNIT_Context *context, UNIT_Size size)
{
    UNIT_Size actual_size = size + sizeof(UNIT_FreelistHeader);
    UNIT_FreelistHeader *header = malloc(actual_size);
    if (header == NULL) {
        _UNIT_SetErrorFormat(context, UNIT_ERROR_NO_MEMORY, "failed to allocate %ld bytes",
                             actual_size);
        return NULL;
    }

    header->size_class = size;
    return header + 1;
}

static inline void *
freelist_pop_or_malloc(UNIT_Context *context, _UNIT_Freelist **freelist,
                       UNIT_Size size)
{
    assert(freelist != NULL);
    void *ptr = freelist_pop(freelist);
    if (ptr == NULL) {
        return malloc_with_header(context, size);
    }
    // Restore the size class since freelist_push overwrote it
    UNIT_FreelistHeader *header = ptr;
    header->size_class = size;
    return header + 1;
}

static inline UNIT_Size
round_size(UNIT_Size size_bytes)
{
#define SIZE_CLASS(size)        \
    if (size_bytes <= size) {   \
        return size;            \
    }

    SIZE_CLASS(8);
    SIZE_CLASS(16);
    SIZE_CLASS(32);
    SIZE_CLASS(64);
    SIZE_CLASS(128);
    SIZE_CLASS(256);
    return size_bytes;
#undef SIZE_CLASS
}

void *
_UNIT_Alloc(UNIT_Context *context, UNIT_Size raw_size)
{
    assert(context != NULL);
    assert(raw_size > 0);
    UNIT_Size size = round_size(raw_size);
    assert(size >= sizeof(_UNIT_Freelist));

    switch (size) {
#define SIZE_CLASS(size)                                                                        \
    case size: {                                                                                \
        return freelist_pop_or_malloc(context,                                                  \
                                      &context->_internal.allocator.freelist_ ##size, size);    \
    }

    SIZE_CLASS(8);
    SIZE_CLASS(16);
    SIZE_CLASS(32);
    SIZE_CLASS(64);
    SIZE_CLASS(128);
    SIZE_CLASS(256);

#undef SIZE_CLASS
    default:
        return malloc_with_header(context, size);
    }
}

void
_UNIT_Dealloc(UNIT_Context *context, void *ptr)
{
    assert(context != NULL);
    assert(ptr != NULL);
    UNIT_FreelistHeader *header = ((UNIT_FreelistHeader *)ptr) - 1;
    assert(header->size_class > 0);

    switch (header->size_class) {
#define SIZE_CLASS(size)                                                        \
    case size: {                                                                \
        freelist_push(header, &context->_internal.allocator.freelist_ ##size);  \
        return;                                                                 \
    }

    SIZE_CLASS(8);
    SIZE_CLASS(16);
    SIZE_CLASS(32);
    SIZE_CLASS(64);
    SIZE_CLASS(128);
    SIZE_CLASS(256);

#undef SIZE_CLASS
    default:
        free(header);
        return;
    }
}

void *
_UNIT_Calloc(UNIT_Context *context, UNIT_Size count, UNIT_Size size)
{
    if (count == 0 || size == 0) {
        return NULL;
    }

    if (UNIT_SIZE_MAX / count < size) {
        return NULL;
    }

    UNIT_Size total = count * size;

    void *ptr = _UNIT_Alloc(context, total);
    if (ptr == NULL) {
        return NULL;
    }

    memset(ptr, 0, total);
    return ptr;
}

void *
_UNIT_Realloc(UNIT_Context *context, void *ptr, UNIT_Size new_size)
{
    assert(ptr != NULL);
    assert(new_size > 0);

    UNIT_FreelistHeader *header = ((UNIT_FreelistHeader *)ptr) - 1;

    UNIT_Size old_class = header->size_class;
    UNIT_Size new_class = round_size(new_size);

    // Existing allocation already fits
    if (new_class == old_class) {
        return ptr;
    }

    void *new_ptr = _UNIT_Alloc(context, new_size);
    if (new_ptr == NULL) {
        return NULL;
    }

    memcpy(new_ptr, ptr, old_class);
    _UNIT_Dealloc(context, ptr);

    return new_ptr;
}

char *
_UNIT_StrDup(UNIT_Context *context, const char *src)
{
    assert(context != NULL);
    assert(src != NULL);
    UNIT_Size length = strlen(src);
    char *result = _UNIT_Alloc(context, length + 1);
    for (UNIT_Size index = 0; index < length; ++index) {
        result[index] = src[index];
    }
    result[length] = '\0';
    return result;
}

static void
clear_freelist(_UNIT_Freelist **freelist)
{
    assert(freelist != NULL);
    _UNIT_Freelist *current = *freelist;
    while (current != NULL) {
        _UNIT_Freelist *next = current->next;
        free(current);
        current = next;
    }
    *freelist = NULL;
}

void
_UNIT_ClearFreelists(UNIT_Context *context)
{
    assert(context != NULL);
#define SIZE_CLASS(size) \
    clear_freelist(&context->_internal.allocator.freelist_ ##size);

    SIZE_CLASS(8);
    SIZE_CLASS(16);
    SIZE_CLASS(32);
    SIZE_CLASS(64);
    SIZE_CLASS(128);
    SIZE_CLASS(256);

#undef SIZE_CLASS
}
