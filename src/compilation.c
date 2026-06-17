#include <string.h>

#include <unit/compilation.h>
#include <unit/errors.h>

#include <unit/internal/architectures.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/executable_formats.h>
#include <unit/internal/register_allocation.h>
#include <unit/internal/set.h>
#include <unit/internal/translation.h>

UNIT_Status
UNIT_GetCurrentPlatform(UNIT_Context *context, UNIT_Platform *out)
{
    assert(context != NULL);
    assert(out != NULL);

    UNIT_Platform platform = 0;

#if defined(__x86_64__) || defined(_M_X64)
    platform |= UNIT_ARCH_AMD64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    platform |= UNIT_ARCH_AARCH64;
#else
    _UNIT_SetError(context,
                   UNIT_ERROR_UNSUPPORTED_PLATFORM,
                   "unknown CPU architecture");
    return _UNIT_FAIL;
#endif

#if defined(_WIN32)
    platform |= UNIT_ABI_WIN64;
#elif defined(__APPLE__)
    platform |= UNIT_ABI_APPLE;
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__) \
      || defined(__NetBSD__) || defined(__DragonFly__) || defined(__sun)
    platform |= UNIT_ABI_SYSTEMV;
#else
    _UNIT_SetError(context,
                   UNIT_ERROR_UNSUPPORTED_PLATFORM,
                   "unknown operating system");
    return _UNIT_FAIL;
#endif

    *out = platform;
    return _UNIT_OK;
}

_UNIT_Relocation *
new_relocation(UNIT_Context *context, UNIT_Size offset, UNIT_Size symbol_index,
               _UNIT_RelocationType type)
{
    _UNIT_Relocation *relocation = _UNIT_Alloc(context, sizeof(_UNIT_Relocation));
    if (relocation == NULL) {
        return NULL;
    }

    relocation->offset = offset;
    relocation->symbol_index = symbol_index;
    relocation->type = type;
    return relocation;
}

_UNIT_Relocation *
_UNIT_Relocation_NewCall(UNIT_Context *context, UNIT_Size offset,
                         UNIT_Size symbol_index)
{
    return new_relocation(context, offset, symbol_index, RELOCATION_CALL);
}

_UNIT_Relocation *
_UNIT_Relocation_NewData(UNIT_Context *context, UNIT_Size offset,
                         UNIT_Size symbol_index)
{
    return new_relocation(context, offset, symbol_index, RELOCATION_DATA);
}

void
_UNIT_Relocation_Free(UNIT_Context *context, _UNIT_Relocation *relocation)
{
    assert(relocation != NULL);
    _UNIT_Dealloc(context, relocation);
}

static _UNIT_Symbol *
new_symbol(UNIT_Context *context, const char *name)
{
    assert(context != NULL);
    assert(name != NULL);

    _UNIT_Symbol *symbol = _UNIT_Alloc(context, sizeof(_UNIT_Symbol));
    if (symbol == NULL) {
        return NULL;
    }

    symbol->name = _UNIT_StrDup(context, name);
    if (symbol->name == NULL) {
        _UNIT_Dealloc(context, symbol);
        return NULL;
    }

    symbol->is_defined = 0;
    symbol->text_offset = 0;
    return symbol;
}

static void
free_symbol(UNIT_Context *context, void *symbol_ptr)
{
    assert(symbol_ptr != NULL);
    _UNIT_Symbol *symbol = (_UNIT_Symbol *)symbol_ptr;
    _UNIT_Dealloc(context, symbol->name);
    _UNIT_Dealloc(context, symbol);
}

UNIT_Status
_UNIT_SymbolTable_Init(_UNIT_SymbolTable *symbol_table, UNIT_Context *context,
                       const _UNIT_Vector *names)
{
    assert(symbol_table != NULL);
    if (UNIT_FAILED(_UNIT_Vector_Init(&symbol_table->relocations, context, 16, _UNIT_Dealloc))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Vector_Init(&symbol_table->symbols, context, 8, free_symbol))) {
        _UNIT_Vector_Clear(&symbol_table->relocations);
        return _UNIT_FAIL;
    }

    UNIT_Size size = _UNIT_Vector_SIZE(names);
    for (UNIT_Size index = 0; index < size; ++index) {
        const char *name = _UNIT_Vector_GET(names, index);

        _UNIT_Symbol *symbol = new_symbol(context, name);
        if (symbol == NULL) {
            return _UNIT_FAIL;
        }

        if (UNIT_FAILED(_UNIT_Vector_Append(&symbol_table->symbols, symbol))) {
            return _UNIT_FAIL;
        }
    }
    return _UNIT_OK;
}

void
_UNIT_SymbolTable_Clear(_UNIT_SymbolTable *symbol_table)
{
    assert(symbol_table != NULL);
    _UNIT_Vector_Clear(&symbol_table->relocations);
}

_UNIT_PendingJump *
_UNIT_PendingJump_New(UNIT_Context *context,
                      UNIT_Size patch_offset, UNIT_Size label_index)
{
    assert(patch_offset > 0);
    assert(label_index >= 0);
    _UNIT_PendingJump *pending_jump = _UNIT_Alloc(context, sizeof(_UNIT_PendingJump));
    if (pending_jump == NULL) {
        return NULL;
    }

    pending_jump->patch_offset = patch_offset;
    pending_jump->label_index = label_index;
    return pending_jump;
}

void
_UNIT_PendingJump_Free(UNIT_Context *context, _UNIT_PendingJump *pending_jump)
{
    assert(pending_jump != NULL);
    _UNIT_Dealloc(context, pending_jump);
}

UNIT_Status
_UNIT_JumpTable_Init(_UNIT_JumpTable *jump_table, UNIT_Context *context)
{
    assert(jump_table != NULL);
    if (UNIT_FAILED(_UNIT_Vector_Init(&jump_table->pending_jumps,
                                      context, 4,
                                      (UNIT_Destructor)_UNIT_PendingJump_Free))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&jump_table->label_offsets, context, 4))) {
        _UNIT_Vector_Clear(&jump_table->pending_jumps);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

void
_UNIT_JumpTable_Clear(_UNIT_JumpTable *jump_table)
{
    assert(jump_table != NULL);
    _UNIT_Vector_Clear(&jump_table->pending_jumps);
    _UNIT_SizeMap_Clear(&jump_table->label_offsets);
}

UNIT_Status
_UNIT_StringData_Init(_UNIT_StringData *string_data, UNIT_Context *context)
{
    assert(string_data != NULL);
    if (UNIT_FAILED(_UNIT_SizeMap_Init(&string_data->string_offsets, context, 8))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_CodeBuffer_Init(&string_data->constant_buffer, context))) {
        _UNIT_SizeMap_Clear(&string_data->string_offsets);
        return _UNIT_FAIL;
    }

    return _UNIT_OK;
}

void
_UNIT_StringData_Clear(_UNIT_StringData *string_data)
{
    assert(string_data != NULL);
    _UNIT_SizeMap_Clear(&string_data->string_offsets);
    _UNIT_CodeBuffer_Clear(&string_data->constant_buffer);
}

void
init_stack_frame(_UNIT_StackFrame *frame, UNIT_Size reserved_slots)
{
    assert(frame != NULL);
    frame->next_slot = reserved_slots;
    frame->free_slot_count = 0;
    frame->reserved_slots = reserved_slots;
}

UNIT_Size
_UNIT_StackFrame_AllocateSlotID(_UNIT_StackFrame *frame)
{
    assert(frame != NULL);
    assert(frame->free_slot_count < _UNIT_StackFrame_MAX_FREE_SLOTS);
    if (frame->free_slot_count > 0) {
        return frame->free_slots[--frame->free_slot_count];
    }
    return (frame->next_slot++);
}

UNIT_Size
_UNIT_StackFrame_AllocateSlot(_UNIT_StackFrame *frame)
{
    assert(frame != NULL);
    return _UNIT_StackFrame_AllocateSlotID(frame) * 8;
}

void
_UNIT_StackFrame_FreeSlotID(_UNIT_StackFrame *frame, UNIT_Size slot_id)
{
    assert(frame != NULL);
    assert(slot_id >= frame->reserved_slots); // can't free memory variable slots
    assert(frame->free_slot_count < _UNIT_StackFrame_MAX_FREE_SLOTS);
    frame->free_slots[frame->free_slot_count++] = slot_id;
}

void
_UNIT_StackFrame_FreeSlot(_UNIT_StackFrame *frame, UNIT_Size slot_id)
{
    assert(frame != NULL);
    assert(slot_id >= 0);
    assert(slot_id % 8 == 0);
    _UNIT_StackFrame_FreeSlotID(frame, slot_id / 8);
}

UNIT_Size
_UNIT_StackFrame_ComputeSize(_UNIT_StackFrame *frame)
{
    assert(frame != NULL);
    UNIT_Size size = frame->next_slot * 8;
    assert(size >= 0);
    if (size == 0) {
        return 0;
    }
    // Round up so that size % 16 == 8
    // This ensures RSP is 16-byte aligned after sub
    // because RSP at entry is 8 mod 16 (return address)
    if (size % 16 == 0) {
        size += 8;
    } else if (size % 16 != 8) {
        size = ((size + 15) & ~15) + 8;
    }
    assert(size % 16 == 8);
    return size;
}

UNIT_Status
_UNIT_CompileContext_Init(_UNIT_CompileContext *compile_context,
                          UNIT_Context *context,
                          const UNIT_Procedure *procedure,
                          const _UNIT_Translation *translation)
{
    assert(compile_context != NULL);
    assert(context != NULL);
    assert(procedure != NULL);
    assert(translation != NULL);

    compile_context->context = context;
    if (UNIT_FAILED(_UNIT_CodeBuffer_Init(&compile_context->buffer,
                                          context))) {
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_StringData_Init(&compile_context->string_data,
                                          context))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SymbolTable_Init(&compile_context->symbol_table,
                                           context, &procedure->_symbols))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        _UNIT_StringData_Clear(&compile_context->string_data);
        return _UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_JumpTable_Init(&compile_context->jump_table, context))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        _UNIT_StringData_Clear(&compile_context->string_data);
        _UNIT_SymbolTable_Clear(&compile_context->symbol_table);
        return _UNIT_FAIL;
    }

    init_stack_frame(&compile_context->stack_frame, translation->num_memory_slots);
    return _UNIT_OK;
}

void
_UNIT_CompileContext_Clear(_UNIT_CompileContext *context)
{
    assert(context != NULL);
    _UNIT_CodeBuffer_Clear(&context->buffer);
    _UNIT_StringData_Clear(&context->string_data);
    _UNIT_SymbolTable_Clear(&context->symbol_table);
    _UNIT_JumpTable_Clear(&context->jump_table);
}

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

    if (UNIT_FAILED(_UNIT_Translate(&compiled_procedure->_translation, procedure))) {
        _UNIT_Dealloc(context, compiled_procedure);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_CompileContext_Init(&compiled_procedure->_compile_context, context,
                                              procedure, &compiled_procedure->_translation))) {
        _UNIT_Translation_Clear(&compiled_procedure->_translation);
        _UNIT_Dealloc(context, compiled_procedure);
        return NULL;
    }

    if (UNIT_FAILED(_UNIT_Translation_AllocateRegisters(&compiled_procedure->_translation,
                                                        &compiled_procedure->_compile_context, 8))) {
        goto error;
    }

    _UNIT_Translation_PrintInstructions(&compiled_procedure->_translation);

    if (UNIT_FAILED(build_constant_data(&compiled_procedure->_compile_context.string_data,
                                        &procedure->_global_strings))) {
        goto error;
    }

    UNIT_Status result;
    switch (UNIT_Platform_GET_ARCH(platform)) {
        case UNIT_ARCH_AMD64:
            result = _UNIT_AMD64_Compile(&compiled_procedure->_translation,
                                         &compiled_procedure->_compile_context);
            break;
        default:
            assert(UNIT_Platform_GET_ARCH(platform) == UNIT_ARCH_AARCH64);
            _UNIT_SetError(context, UNIT_ERROR_UNSUPPORTED_PLATFORM, "AArch64 is not supported yet");
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

    _UNIT_Symbol *symbol = new_symbol(context, name);
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

UNIT_CompiledProcedure *
UNIT_Compile(const UNIT_Procedure *procedure, UNIT_Platform platform)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    assert(context != NULL);

    _UNIT_Vector compiled;

    if (UNIT_FAILED(_UNIT_Vector_Init(&compiled, procedure->context,
                                      _UNIT_Vector_SIZE(&procedure->_subprocedures),
                                      (UNIT_Destructor)UNIT_CompiledProcedure_Free))) {
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

    _UNIT_Symbol *root_symbol = new_symbol(context, procedure->name);
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
    }
}
