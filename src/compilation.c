#include <string.h>

#include <unit/compilation.h>
#include <unit/errors.h>

#include <unit/internal/architectures.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/executable_formats.h>
#include <unit/internal/register_allocation.h>
#include <unit/internal/set.h>
#include <unit/internal/translation.h>

static UNIT_Status
build_constant_data(_UNIT_StringData *string_data, const _UNIT_Vector *strings)
{
    UNIT_Size count = _UNIT_Vector_SIZE(strings);

    for (UNIT_Size index = 0; index < count; ++index) {
        const char *string = _UNIT_Vector_GET(strings, index);
        UNIT_Size length = strlen(string) + 1;

        UNIT_Size buffer_index = _UNIT_CodeBuffer_CurrentIndex(&string_data->constant_buffer);
        if (UNIT_FAILED(_UNIT_SizeMap_Set(&string_data->string_offsets,
                                          index,
                                          buffer_index))) {
            return _UNIT_FAIL;
        }

        for (UNIT_Size byte = 0; byte < length; ++byte) {
            if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(&string_data->constant_buffer,
                                   string[byte]))) {
                return _UNIT_FAIL;
            }
        }
    }

    return _UNIT_OK;
}

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled)
{
    assert(compiled != NULL);
    _UNIT_CompileContext_Clear(&compiled->_compile_context);
    _UNIT_Translation_Clear(&compiled->_translation);
    _UNIT_Dealloc(compiled->context, compiled);
}

static UNIT_CompiledProcedure *
compile_procedure(const UNIT_Procedure *procedure, UNIT_Platform platform)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    UNIT_CompiledProcedure *compiled_procedure = _UNIT_Alloc(context,
                                                             sizeof(UNIT_CompiledProcedure));
    if (compiled_procedure == NULL) {
        return NULL;
    }
    compiled_procedure->name = procedure->name;
    compiled_procedure->context = context;
    compiled_procedure->platform = platform;

    if (UNIT_FAILED(_UNIT_Translate(&compiled_procedure->_translation, procedure))) {
        _UNIT_Dealloc(context, compiled_procedure);
        return NULL;
    }

    if (procedure->flags & UNIT_FLAG_PRINT_TRANSLATION_PREOP) {
        _UNIT_Translation_PrintInstructions(&compiled_procedure->_translation, procedure->name,
                                            stdout);
    }

    if (UNIT_FAILED(_UNIT_Translation_AllocateRegisters(&compiled_procedure->_translation,
                                                        &compiled_procedure->_compile_context, 8))) {
        goto error;
    }

  
    int8_t num_registers;
    switch (UNIT_Platform_GET_ARCH(platform)) {
        case UNIT_ARCH_AMD64:
            num_registers = 8;
            break;
        case UNIT_ARCH_AARCH64:
            num_registers = 17;
            break;
        default:
            _UNIT_SetError(context, UNIT_ERROR_UNSUPPORTED_PLATFORM, "Unsupported architecture");
            goto error;
    }
  
    if (!(procedure->flags & UNIT_FLAG_NO_OPTIMIZE_TRANSLATION)) {
        if (UNIT_FAILED(_UNIT_Translation_Optimize(&compiled_procedure->_translation, num_registers))) {
            _UNIT_Translation_Clear(&compiled_procedure->_translation);
            _UNIT_Dealloc(context, compiled_procedure);
            return NULL;
        }
    }

    if (UNIT_FAILED(_UNIT_CompileContext_Init(&compiled_procedure->_compile_context, context,
                                              procedure, &compiled_procedure->_translation))) {
        _UNIT_Translation_Clear(&compiled_procedure->_translation);
        _UNIT_Dealloc(context, compiled_procedure);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Translation_AllocateRegisters(&compiled_procedure->_translation,
                                                        &compiled_procedure->_compile_context,
                                                        num_registers))) {
        goto error;
    }

    if (procedure->flags & UNIT_FLAG_PRINT_TRANSLATION_POSTOP) {
        _UNIT_Translation_PrintInstructions(&compiled_procedure->_translation, procedure->name,
                                            stdout);
    }

    if (UNIT_FAILED(build_constant_data(&compiled_procedure->_compile_context.string_data,
                                        &procedure->_global_strings))) {
        goto error;
    }

    UNIT_Status result;
    switch (UNIT_Platform_GET_ARCH(platform)) {
        case UNIT_ARCH_AMD64:
            result = _UNIT_AMD64_Compile(&compiled_procedure->_translation,
                                         &compiled_procedure->_compile_context,
                                         UNIT_Platform_GET_ABI(platform));
            break;
        case UNIT_ARCH_AARCH64:
            result = _UNIT_AARCH64_Compile(&compiled_procedure->_translation,
                                           &compiled_procedure->_compile_context,
                                           UNIT_Platform_GET_ABI(platform));
            break;
        default:
            _UNIT_SetError(context, UNIT_ERROR_UNSUPPORTED_PLATFORM, "Unsupported architecture");
            goto error;
    }

    if (UNIT_FAILED(result)) {
        goto error;
    }

    return compiled_procedure;
error:
    UNIT_CompiledProcedure_Free(compiled_procedure);
    return NULL;
}

static UNIT_CompiledProcedure *
compile_and_store_procedure(const UNIT_Procedure *procedure, UNIT_Platform platform,
                            _UNIT_Vector *compiled, _UNIT_Set *visited)
{
    assert(procedure != NULL);
    assert(compiled != NULL);
    assert(visited != NULL);

    UNIT_CompiledProcedure *compiled_procedure = compile_procedure(procedure, platform);
    if (compiled_procedure == NULL) {
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(compiled, compiled_procedure))) {
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Set_Add(visited, procedure))) {
        return NULL;
    }

    return compiled_procedure;
}

static UNIT_Status
compile_procedure_recursive(const UNIT_Procedure *procedure, UNIT_Platform platform,
                            _UNIT_Vector *compiled, _UNIT_Set *visited)
{
    assert(procedure != NULL);
    assert(compiled != NULL);
    assert(visited != NULL);
    if (_UNIT_Set_Contains(visited, procedure)) {
        // Don't compile the same thing twice
        return _UNIT_OK;
    }

    UNIT_CompiledProcedure *compiled_parent = compile_and_store_procedure(procedure, platform,
                                                                          compiled, visited);
    if (compiled_parent == NULL) {
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(&procedure->_subprocedures);
    for (UNIT_Size index = 0; index < size; ++index) {
        UNIT_Procedure *subprocedure = _UNIT_Vector_GET(&procedure->_subprocedures, index);
        assert(subprocedure != NULL);
        assert(procedure != subprocedure);

        if (UNIT_FAILED(compile_procedure_recursive(subprocedure, platform,
                                                    compiled, visited))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

static UNIT_Status
merge_string_data(_UNIT_CompileContext *parent_ctx, _UNIT_CompileContext *sub_ctx,
                  UNIT_Size *rodata_offset)
{
    assert(parent_ctx != NULL);
    assert(sub_ctx != NULL);
    assert(rodata_offset != NULL);

    *rodata_offset = parent_ctx->string_data.constant_buffer.size;
    UNIT_Size size = sub_ctx->string_data.constant_buffer.size;
    for (UNIT_Size index = 0; index < size; ++index) {
        if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(&parent_ctx->string_data.constant_buffer,
                                               sub_ctx->string_data.constant_buffer.data[index]))) {
            return _UNIT_FAIL;
        }
    }
    return _UNIT_OK;
}

static UNIT_Size
find_or_add_symbol(UNIT_Context *context, _UNIT_SymbolTable *parent_symbols,
                   const char *name)
{
    assert(context != NULL);
    assert(parent_symbols != NULL);
    assert(name != NULL);

    UNIT_Size size = _UNIT_Vector_SIZE(&parent_symbols->symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Symbol *found_symbol = _UNIT_Vector_GET(&parent_symbols->symbols, index);
        assert(found_symbol != NULL);
        if (strcmp(found_symbol->name, name) == 0) {
            return index;
        }
    }

    _UNIT_Symbol *symbol = _UNIT_Symbol_New(context, name);
    if (symbol == NULL) {
        return -1;
    }

    if (UNIT_FAILED(_UNIT_Vector_Append(&parent_symbols->symbols, symbol))) {
        return -1;
    }

    return size;
}

static UNIT_Status
merge_relocations(UNIT_Context *context,
                  _UNIT_CompileContext *parent_ctx,
                  _UNIT_CompileContext *sub_ctx,
                  UNIT_Size code_offset,
                  UNIT_Size rodata_offset)
{
    assert(context != NULL);
    assert(parent_ctx != NULL);
    assert(sub_ctx != NULL);
    assert(code_offset >= 0);
    assert(rodata_offset >= 0);

    UNIT_Size size = _UNIT_Vector_SIZE(&sub_ctx->symbol_table.relocations);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Relocation *sub_relocation = _UNIT_Vector_GET(&sub_ctx->symbol_table.relocations,
                                                            index);
        assert(sub_relocation != NULL);

        _UNIT_Relocation *new_relocation = _UNIT_Alloc(context,
                                                       sizeof(_UNIT_Relocation));
        if (new_relocation == NULL) {
            return _UNIT_FAIL;
        }

        new_relocation->type = sub_relocation->type;
        new_relocation->offset = sub_relocation->offset + code_offset;

        if (sub_relocation->type == RELOCATION_DATA) {
            new_relocation->symbol_index = sub_relocation->symbol_index + rodata_offset;
        } else {
            _UNIT_Symbol *symbol = _UNIT_Vector_GET(&sub_ctx->symbol_table.symbols,
                                                    sub_relocation->symbol_index);
            assert(symbol != NULL);
            const char *name = symbol->name;
            UNIT_Size symbol_index = find_or_add_symbol(context, &parent_ctx->symbol_table, name);
            if (symbol_index == -1) {
                return _UNIT_FAIL;
            }
            new_relocation->symbol_index = symbol_index;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(&parent_ctx->symbol_table.relocations,
                                            new_relocation))) {
            return _UNIT_FAIL;
        }
    }
    return _UNIT_OK;
}

static void
register_defined_symbol(_UNIT_CompileContext *compile_context,
                        const char *name, UNIT_Size code_offset)
{
    assert(compile_context != NULL);
    assert(name != NULL);
    assert(code_offset >= 0);

    UNIT_Size size = _UNIT_Vector_SIZE(&compile_context->symbol_table.symbols);
    for (UNIT_Size index = 0; index < size; ++index) {
        _UNIT_Symbol *symbol = _UNIT_Vector_GET(&compile_context->symbol_table.symbols, index);
        assert(symbol != NULL);
        if (strcmp(symbol->name, name) == 0) {
            symbol->is_defined = 1;
            symbol->text_offset = code_offset;
            return ;
        }
    }

    _UNIT_Unreachable();
}

static UNIT_Status
merge_code(_UNIT_CompileContext *parent_ctx, _UNIT_CompileContext *sub_ctx,
           UNIT_Size *code_offset)
{
    *code_offset = _UNIT_CodeBuffer_CurrentIndex(&parent_ctx->buffer);
    UNIT_Size sub_size = _UNIT_CodeBuffer_CurrentIndex(&sub_ctx->buffer);
    for (UNIT_Size i = 0; i < sub_size; ++i) {
        if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(&parent_ctx->buffer,
                                                sub_ctx->buffer.data[i]))) {
            return _UNIT_FAIL;
        }
    }
    return _UNIT_OK;
}

static UNIT_Status
merge_subprocedure(UNIT_Context *context,
                   _UNIT_CompileContext *compile_context,
                   UNIT_CompiledProcedure *subprocedure)
{
    assert(context != NULL);
    assert(compile_context != NULL);
    assert(subprocedure != NULL);

    _UNIT_CompileContext *sub_compile_ctx = &subprocedure->_compile_context;

    UNIT_Size code_offset;
    if (UNIT_FAILED(merge_code(compile_context, sub_compile_ctx, &code_offset))) {
        return _UNIT_FAIL;
    }

    register_defined_symbol(compile_context, subprocedure->name, code_offset);

    UNIT_Size rodata_offset;
    if (UNIT_FAILED(merge_string_data(compile_context, sub_compile_ctx, &rodata_offset))) {
        return _UNIT_FAIL;
    }

    return merge_relocations(context, compile_context, sub_compile_ctx,
                             code_offset, rodata_offset);
}

static UNIT_Status
merge_compiled(UNIT_Context *context, _UNIT_Vector *compiled,
               UNIT_CompiledProcedure *parent)
{
    assert(context != NULL);
    assert(compiled != NULL);
    assert(parent != NULL);

    for (UNIT_Size index = 1; index < _UNIT_Vector_SIZE(compiled); ++index) {
        UNIT_CompiledProcedure *subprocedure = _UNIT_Vector_GET(compiled, index);
        assert(subprocedure != NULL);
        if (UNIT_FAILED(merge_subprocedure(context, &parent->_compile_context,
                                           subprocedure))) {
            return _UNIT_FAIL;
        }
    }

    return _UNIT_OK;
}

void
free_compiled_procedure_ctx(UNIT_Context *context, void *ptr)
{
    UNIT_CompiledProcedure *compiled = (UNIT_CompiledProcedure *)ptr;
    UNIT_CompiledProcedure_Free(compiled);
}

UNIT_CompiledProcedure *
UNIT_Compile(const UNIT_Procedure *procedure, UNIT_Platform platform)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    assert(context != NULL);

    _UNIT_Vector compiled;

    if (UNIT_FAILED(_UNIT_Vector_Init(&compiled, procedure->context,
                                      _UNIT_Vector_SIZE(&procedure->_subprocedures),
                                      free_compiled_procedure_ctx))) {
        return NULL;
    }

    _UNIT_Set visited;
    if (UNIT_FAILED(_UNIT_Set_Init(&visited, context,
                                   _UNIT_Vector_SIZE(&procedure->_subprocedures)))) {
        _UNIT_Vector_Clear(&compiled);
        return NULL;
    }

    if (UNIT_FAILED(compile_procedure_recursive(procedure, platform, &compiled,
                                                &visited))) {
        goto error;
    }
    assert(_UNIT_Vector_SIZE(&compiled) > 0);

    UNIT_CompiledProcedure *parent = _UNIT_Vector_STEAL(&compiled, 0);
    assert(parent != NULL);

    _UNIT_Symbol *root_symbol = _UNIT_Symbol_New(context, procedure->name);
    if (root_symbol == NULL) {
        goto error;
    }

    root_symbol->is_defined = 1;
    root_symbol->text_offset = 0;
    if (UNIT_FAILED(_UNIT_Vector_Append(&parent->_compile_context.symbol_table.symbols,
                                        root_symbol))) {
        goto error;
    }

    if (_UNIT_Vector_SIZE(&compiled) > 1) {
        if (UNIT_FAILED(merge_compiled(context, &compiled, parent))) {
            goto error;
        }
    }

    _UNIT_Vector_Clear(&compiled);
    _UNIT_Set_Clear(&visited);
    return parent;
error:
    _UNIT_Set_Clear(&visited);
    _UNIT_Vector_Clear(&compiled);
    return NULL;
}

UNIT_Status
UNIT_CompiledProcedure_WriteObjectFile(const UNIT_CompiledProcedure *compiled,
                                       const char *path,
                                       UNIT_ExecutableFormat format)
{
    assert(compiled != NULL);
    assert(path != NULL);

    switch (format) {
        case UNIT_FORMAT_ELF:
            return _UNIT_ELF_WriteObjectFile(&compiled->_compile_context, path);
        case UNIT_FORMAT_MACHO:
            return _UNIT_MachO_WriteObjectFile(&compiled->_compile_context, path);
        default:
            _UNIT_SetError(compiled->context, UNIT_ERROR_UNSUPPORTED_PLATFORM,
                           "unsupported executable format");
            return _UNIT_FAIL;
    }

    _UNIT_Unreachable();
}

UNIT_Status
UNIT_CompiledProcedure_PrintTranslatedIR(const UNIT_CompiledProcedure *compiled,
                                         FILE *stream)
{
    assert(compiled != NULL);
    assert(stream != NULL);
    return _UNIT_Translation_PrintInstructions(&compiled->_translation,
                                               compiled->name, stream);
}
