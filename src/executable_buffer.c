#include <string.h>

#include <unit/errors.h>
#include <unit/executable_buffer.h>

UNIT_Status
UNIT_SymbolMap_Init(UNIT_SymbolMap *symbol_map, UNIT_Context *context)
{
    assert(symbol_map != NULL);
    assert(context != NULL);
    symbol_map->context = context;
    if (UNIT_FAILED(_UNIT_Map_Init(&symbol_map->_symbols, context, 8, _UNIT_Map_CompareString,
                                   _UNIT_Map_HashString, _UNIT_Dealloc, NULL))) {
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

void
UNIT_SymbolMap_Clear(UNIT_SymbolMap *symbol_map)
{
    assert(symbol_map != NULL);
    _UNIT_Map_Clear(&symbol_map->_symbols);
}

UNIT_SymbolMap *
UNIT_SymbolMap_New(UNIT_Context *context)
{
    _UNIT_Structure_NEW_IMPL(UNIT_SymbolMap, context);
}

_UNIT_Structure_DEFINE_PUBLIC_FREE(UNIT_SymbolMap);

UNIT_Status
UNIT_SymbolMap_RegisterSymbol(UNIT_SymbolMap *symbol_map, const char *name, void *address)
{
    assert(symbol_map != NULL);
    assert(name != NULL);
    assert(address != NULL);

    char *name_copy = _UNIT_StrDup(symbol_map->context, name);
    if (name_copy == NULL) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Map_Set(&symbol_map->_symbols, name_copy, address))) {
        _UNIT_Dealloc(symbol_map->context, name_copy);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

struct _UNIT_ExecutableBuffer {
    UNIT_Context *context;
    void *code;
    void *rodata;
    UNIT_Size code_size;
    UNIT_Size rodata_size;
};

#ifdef _WIN32
    #include <windows.h>
    #define JIT_ALLOC(size) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
    #define JIT_PROTECT_EXEC(ptr, size) \
        do { DWORD old; VirtualProtect(ptr, size, PAGE_EXECUTE_READ, &old); } while(0)
    #define JIT_PROTECT_READ(ptr, size) \
        do { DWORD old; VirtualProtect(ptr, size, PAGE_READONLY, &old); } while(0)
    #define JIT_FREE(ptr, size) VirtualFree(ptr, 0, MEM_RELEASE)
    #define JIT_FAILED(ptr) ((ptr) == NULL)
    #define JIT_RESOLVE_SYMBOL(name) GetProcAddress(GetModuleHandle(NULL), name)
#else
    #define _GNU_SOURCE
    #include <sys/mman.h>
    #include <dlfcn.h>
    #define JIT_ALLOC(size) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)
    #define JIT_PROTECT_EXEC(ptr, size) mprotect(ptr, size, PROT_READ | PROT_EXEC)
    #define JIT_PROTECT_READ(ptr, size) mprotect(ptr, size, PROT_READ)
    #define JIT_FREE(ptr, size) munmap(ptr, size)
    #define JIT_FAILED(ptr) ((ptr) == MAP_FAILED)
    #define JIT_RESOLVE_SYMBOL(name) dlsym(RTLD_DEFAULT, name)
#endif

void *
resolve_symbol(const UNIT_SymbolMap *symbol_map, const char *name)
{
    if (symbol_map != NULL) {
        void *result = _UNIT_Map_Get(&symbol_map->_symbols, name);
        if (result != NULL) {
            return result;
        }
    }

    return JIT_RESOLVE_SYMBOL(name);
}

static UNIT_Status
init_executable_buffer(const UNIT_CompiledProcedure *compiled,
                       UNIT_ExecutableBuffer *buffer,
                       const UNIT_SymbolMap *symbol_map)
{
    assert(compiled != NULL);
    assert(buffer != NULL);
    // symbol_map may be NULL

    const _UNIT_CompileContext *compile_context = &compiled->_compile_context;
    UNIT_Size code_size = _UNIT_CodeBuffer_CurrentIndex(&compile_context->buffer);
    UNIT_Size rodata_size = compile_context->string_data.constant_buffer.size;
    UNIT_Size total_size = code_size + rodata_size;

    void *code = JIT_ALLOC(total_size);
    if (JIT_FAILED(code)) {
        _UNIT_SetError(compiled->context, UNIT_ERROR_OS_FAILURE,
                       "failed to allocate JIT buffer");
        return _UNIT_FAIL;
    }

    void *rodata = (char *)code + code_size;

    memcpy(code, compile_context->buffer.data, code_size);
    if (rodata_size > 0) {
        memcpy(rodata, compile_context->string_data.constant_buffer.data, rodata_size);
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&compile_context->symbol_table.relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Relocation *relocation = _UNIT_Vector_GET(&compile_context->symbol_table.relocations,
                                                        index);
        assert(relocation != NULL);

        if (relocation->type == RELOCATION_CALL) {
            _UNIT_Symbol *symbol = _UNIT_Vector_GET(&compile_context->symbol_table.symbols,
                                                    relocation->symbol_index);
            assert(symbol != NULL);

            void *target;
            if (symbol->is_defined) {
                target = (char *)code + symbol->text_offset;
            } else {
                target = resolve_symbol(symbol_map, symbol->name);
                if (target == NULL) {
                    JIT_FREE(code, total_size);
                    _UNIT_SetErrorFormat(compiled->context,
                                         UNIT_ERROR_INVALID_USAGE,
                                         "could not resolve symbol: %s",
                                         symbol->name);
                    return _UNIT_FAIL;
                }
            }

            char *patch_address = (char *)code + relocation->offset;
            int32_t displacement = (int32_t)((char *)target - (patch_address + 4));
            memcpy(patch_address, &displacement, 4);

        } else if (relocation->type == RELOCATION_DATA) {
            char *patch_address = (char *)code + relocation->offset;
            char *data_address = (char *)rodata + relocation->symbol_index;
            int32_t displacement = (int32_t)(data_address - (patch_address + 4));
            memcpy(patch_address, &displacement, 4);
        }
    }

    JIT_PROTECT_EXEC(code, code_size);
    if (rodata_size > 0) {
        JIT_PROTECT_READ(rodata, rodata_size);
    }

    buffer->code = code;
    buffer->rodata = rodata;
    buffer->code_size = code_size;
    buffer->rodata_size = rodata_size;
    return _UNIT_OK;
}

UNIT_ExecutableBuffer *
UNIT_CompiledProcedure_JIT(const UNIT_CompiledProcedure *compiled_procedure,
                           const UNIT_SymbolMap *symbol_map)
{
    assert(compiled_procedure != NULL);
    UNIT_ExecutableBuffer *buffer = _UNIT_Alloc(compiled_procedure->context,
                                                sizeof(UNIT_ExecutableBuffer));
    buffer->context = compiled_procedure->context;
    if (UNIT_FAILED(init_executable_buffer(compiled_procedure, buffer, symbol_map))) {
        _UNIT_Dealloc(compiled_procedure->context, buffer);
        return NULL;
    }

    return buffer;
}

void *
UNIT_ExecutableBuffer_GetPointer(const UNIT_ExecutableBuffer *buffer)
{
    assert(buffer != NULL);
    assert(buffer->code != NULL);
    return buffer->code;
}

void
UNIT_ExecutableBuffer_Free(UNIT_ExecutableBuffer *buffer)
{
    assert(buffer != NULL);
    if (buffer->code != NULL) {
        UNIT_Size total = buffer->code_size + buffer->rodata_size;
        JIT_FREE(buffer->code, total);
    }

    _UNIT_Dealloc(buffer->context, buffer);
}
