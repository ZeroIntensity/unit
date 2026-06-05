#include <string.h>

#include <unit/compilation.h>

#include <unit/internal/architectures.h>
#include <unit/internal/compile_context.h>
#include <unit/internal/executable_formats.h>
#include <unit/internal/register_allocation.h>
#include <unit/internal/translation.h>

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
                       const _UNIT_Vector *names)
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
    if (size > 0 && size % 16 != 0) {
        size += 16 - (size % 16);
    }
    assert(size % 16 == 0);
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
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_StringData_Init(&compile_context->string_data,
                                          context))) {
        _UNIT_CodeBuffer_Clear(&compile_context->buffer);
        return UNIT_FAIL;
    }

    if (UNIT_FAILED(_UNIT_SymbolTable_Init(&compile_context->symbol_table,
                                           context, &procedure->_symbols))) {
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

    init_stack_frame(&compile_context->stack_frame, translation->num_memory_slots);
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

void
UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled)
{
    assert(compiled != NULL);
    _UNIT_CompileContext_Clear(&compiled->_compile_context);
    _UNIT_Translation_Clear(&compiled->_translation);
    _UNIT_Dealloc(compiled->context, compiled);
}


UNIT_CompiledProcedure *
UNIT_Compile(const UNIT_Procedure *procedure, UNIT_Architecture architecture)
{
    assert(procedure != NULL);
    UNIT_Context *context = procedure->context;
    UNIT_CompiledProcedure *compiled_procedure = _UNIT_Alloc(context,
                                                             sizeof(UNIT_CompiledProcedure));
    if (compiled_procedure == NULL) {
        return NULL;
    }
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
    switch (architecture) {
        case UNIT_ARCH_AMD64:
            result = _UNIT_AMD64_Compile(&compiled_procedure->_translation,
                                                 &compiled_procedure->_compile_context);
            break;
        // TODO: Add more architectures here as we add them
    }

    if (UNIT_FAILED(result)) {
        goto error;
    }

    return compiled_procedure;
error:
    UNIT_CompiledProcedure_Free(compiled_procedure);
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
