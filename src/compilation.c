#include <unit/internal/architectures.h>
#include <unit/internal/translation.h>
#include <unit/internal/formats/elf.h>
#include <unit/internal/compile_context.h>

#include <string.h>

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


UNIT_Status
_UNIT_SymbolTable_Init(_UNIT_SymbolTable *symbol_table, UNIT_Context *context,
                       _UNIT_Vector *names)
{
    assert(symbol_table != NULL);
    if (UNIT_FAILED(_UNIT_Vector_Init(&symbol_table->relocations, context, 16, _UNIT_Dealloc))) {
        return UNIT_FAIL;
    }
    symbol_table->names = names;
    return UNIT_OK;
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
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SizeMap_Init(&jump_table->label_offsets, context, 4))) {
        _UNIT_Vector_Clear(&jump_table->pending_jumps);
        return UNIT_FAIL;
    }

    return UNIT_OK;
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
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_CodeBuffer_Init(&string_data->constant_buffer, context))) {
        _UNIT_SizeMap_Clear(&string_data->string_offsets);
        return UNIT_FAIL;
    }

    return UNIT_OK;
}

void
_UNIT_StringData_Clear(_UNIT_StringData *string_data)
{
    assert(string_data != NULL);
    _UNIT_SizeMap_Clear(&string_data->string_offsets);
    _UNIT_CodeBuffer_Clear(&string_data->constant_buffer);
}

UNIT_Status
_UNIT_CompileContext_Init(_UNIT_CompileContext *compile_context,
                          UNIT_Context *context,
                          _UNIT_Vector *symbol_names)
{
    assert(context != NULL);
    compile_context->context = context;
    if (UNIT_FAILED(_UNIT_CodeBuffer_Init(&compile_context->buffer,
                                          context))) {
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_StringData_Init(&compile_context->string_data,
                                          context))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SymbolTable_Init(&compile_context->symbol_table,
                                           context, symbol_names))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        _UNIT_StringData_Clear(&compile_context->string_data);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_JumpTable_Init(&compile_context->jump_table, context))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        _UNIT_StringData_Clear(&compile_context->string_data);
        _UNIT_SymbolTable_Clear(&compile_context->symbol_table);
        return UNIT_FAIL;
    }

    compile_context->frame_size = 0;
    return UNIT_OK;
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
build_constant_data(_UNIT_StringData *string_data, _UNIT_Vector *strings)
{
    UNIT_Size count = _UNIT_Vector_SIZE(strings);

    for (UNIT_Size index = 0; index < count; ++index) {
        const char *string = _UNIT_Vector_GET(strings, index);
        UNIT_Size length = strlen(string) + 1;

        UNIT_Size buffer_index = _UNIT_CodeBuffer_CurrentIndex(&string_data->constant_buffer);
        if (UNIT_FAILED(_UNIT_SizeMap_Set(&string_data->string_offsets,
                                          index,
                                          buffer_index))) {
            return UNIT_FAIL;
        }

        for (UNIT_Size byte = 0; byte < length; ++byte) {
            if (UNIT_FAILED(_UNIT_CodeBuffer_Emit8(&string_data->constant_buffer,
                                   string[byte]))) {
                return UNIT_FAIL;
            }
        }
    }

    return UNIT_OK;
}

UNIT_Size
_UNIT_CompileContext_AllocateStackSlot(_UNIT_CompileContext *context)
{
    assert(context != NULL);
    UNIT_Size offset = context->frame_size;
    context->frame_size += 8;
    return offset;
}

UNIT_Status
UNIT_CompileProcedure(UNIT_Procedure *procedure)
{
    _UNIT_Translation translation;
    if (UNIT_FAILED(_UNIT_Translation_InitFromProcedure(&translation, procedure))) {
        return UNIT_FAIL;
    }

    _UNIT_CompileContext context;
    if (UNIT_FAILED(_UNIT_CompileContext_Init(&context, procedure->context,
                                              &procedure->_symbols))) {
        _UNIT_Translation_Clear(&translation);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_Translation_AllocateRegisters(&translation, 8))) {
        goto error;
    }

    _UNIT_Translation_PrintInstructions(&translation);

    if (UNIT_FAILED(build_constant_data(&context.string_data,
                                        &procedure->_global_strings))) {
        goto error;
    }

    if (UNIT_FAILED(_UNIT_X86_64_FromTranslation(&translation, &context))) {
        goto error;
    }

    UNIT_Status result = _UNIT_ELF_WriteObjectFile(&context, "test.o");
    _UNIT_CompileContext_Clear(&context);
    _UNIT_Translation_Clear(&translation);
    return result;
error:
    _UNIT_CompileContext_Clear(&context);
    _UNIT_Translation_Clear(&translation);
    return UNIT_FAIL;
}
