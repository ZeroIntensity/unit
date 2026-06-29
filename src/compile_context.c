#include <unit/internal/compile_context.h>

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

_UNIT_Symbol *
_UNIT_Symbol_New(UNIT_Context *context, const char *name)
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

        _UNIT_Symbol *symbol = _UNIT_Symbol_New(context, name);
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
    _UNIT_Vector_Clear(&symbol_table->symbols);
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
    frame->slot_offset = 0;
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
