#include <Python.h>
#include <unit/unit.h>

typedef struct {
    PyObject *ErrorType;
    PyObject *ContextType;
    PyObject *CompiledProcedureType;
    PyObject *LocalType;
    PyObject *JumpLabelType;
    PyObject *ExecutableBufferType;
    PyObject *ProcedureType;
} _unit_state;

static _unit_state *
get_state_from_type(PyTypeObject *cls)
{
    assert(cls != NULL);
    assert(PyType_Check(cls));
    PyObject *module = PyType_GetModule(cls);
    assert(module != NULL);
    assert(PyModule_Check(module));

    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);
    return state;
}

static _unit_state *
get_state_from_object(PyObject *op)
{
    return get_state_from_type(Py_TYPE(op));
}

static void
set_py_error_from_context(_unit_state *state, UNIT_Context *context)
{
    assert(!PyErr_Occurred());
    assert(context != NULL);
    assert(state != NULL);
    assert(state->ErrorType != NULL);
    assert(UNIT_GetErrorCode(context) != UNIT_ERROR_NONE);
    assert(UNIT_GetErrorMessage(context) != NULL);

    PyErr_Format(state->ErrorType, "[%s] %s",
                 UNIT_ErrorCode_ToString(UNIT_GetErrorCode(context)),
                 UNIT_GetErrorMessage(context));
}

typedef struct {
    PyObject_HEAD
    UNIT_Context context;
} ContextObject;

#define ContextObject_CAST(op) ((ContextObject *)op)

static PyObject *
ContextObject_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    assert(cls != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        return NULL;
    }
    ContextObject *self = ContextObject_CAST(op);

    if (UNIT_FAILED(UNIT_Context_Init(&self->context))) {
        Py_DECREF(op);
        PyErr_SetString(PyExc_RuntimeError, "failed to create context");
        return NULL;
    }

    return op;
}

static int
ContextObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
ContextObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    ContextObject *context = ContextObject_CAST(op);
    UNIT_Context_Clear(&context->context);
    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyType_Slot ContextType_slots[] = {
    {Py_tp_new, ContextObject_new},
    {Py_tp_traverse, ContextObject_traverse},
    {Py_tp_dealloc, ContextObject_dealloc},
    {0, 0},  /* sentinel */
};

static PyType_Spec ContextType_spec = {
    .name = "Context",
    .basicsize = sizeof(ContextObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = ContextType_slots,
};

typedef struct {
    PyObject_HEAD
    UNIT_ExecutableBuffer *buffer;
} ExecutableBufferObject;

#define ExecutableBufferObject_CAST(op) ((ExecutableBufferObject *)op)

static PyObject *
ExecutableBufferObject_internal_create(PyTypeObject *cls, UNIT_ExecutableBuffer *buffer)
{
    assert(cls != NULL);
    assert(buffer != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        UNIT_ExecutableBuffer_Free(buffer);
        return NULL;
    }

    ExecutableBufferObject *self = ExecutableBufferObject_CAST(op);
    self->buffer = buffer;
    return op;
}

static int
ExecutableBufferObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
ExecutableBufferObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    ExecutableBufferObject *self = ExecutableBufferObject_CAST(op);
    UNIT_ExecutableBuffer_Free(self->buffer);

    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyObject *
ExecutableBufferObject_get_address(PyObject *op, void *closure)
{
    assert(op != NULL);
    ExecutableBufferObject *self = ExecutableBufferObject_CAST(op);
    assert(self->buffer != NULL);
    return PyLong_FromVoidPtr(UNIT_ExecutableBuffer_GetPointer(self->buffer));
}

static PyGetSetDef ExecutableBufferObject_getset[] = {
    {"address", ExecutableBufferObject_get_address, NULL, NULL},
    {NULL},
};

static PyType_Slot ExecutableBufferType_slots[] = {
    {Py_tp_traverse, ExecutableBufferObject_traverse},
    {Py_tp_dealloc, ExecutableBufferObject_dealloc},
    {Py_tp_getset, ExecutableBufferObject_getset},
    {0, 0},  /* sentinel */
};

static PyType_Spec ExecutableBufferType_spec = {
    .name = "ExecutableBuffer",
    .basicsize = sizeof(ExecutableBufferObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = ExecutableBufferType_slots,
};

static PyObject *
tmpfile_stream_to_text(FILE *stream)
{
    long size = ftell(stream);
    if (size < 0) {
        fclose(stream);
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    rewind(stream);

    char *buffer = PyMem_Malloc(size);
    if (buffer == NULL) {
        fclose(stream);
        return PyErr_NoMemory();
    }

    size_t read = fread(buffer, 1, size, stream);
    if (read != (size_t)size) {
        PyMem_Free(buffer);
        fclose(stream);
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    fclose(stream);

    PyObject *result = PyUnicode_FromStringAndSize(buffer, size);
    PyMem_Free(buffer);
    return result;
}

typedef struct {
    PyObject_HEAD
    UNIT_CompiledProcedure *compiled_procedure;
} CompiledProcedureObject;

#define CompiledProcedureObject_CAST(op) ((CompiledProcedureObject *)op)

static PyObject *
CompiledProcedureObject_internal_create(PyTypeObject *cls,
                                        UNIT_CompiledProcedure *compiled_procedure)
{
    assert(cls != NULL);
    assert(compiled_procedure != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        UNIT_CompiledProcedure_Free(compiled_procedure);
        return NULL;
    }

    CompiledProcedureObject *self = CompiledProcedureObject_CAST(op);
    self->compiled_procedure = compiled_procedure;
    return op;
}

static PyObject *
CompiledProcedureObject_write_object_file(PyObject *op, PyObject *args)
{
    assert(op != NULL);
    assert(args != NULL);

    const char *path;
    UNIT_ExecutableFormat format;

    if (!PyArg_ParseTuple(args, "si", &path, &format)) {
        return NULL;
    }

    CompiledProcedureObject *self = CompiledProcedureObject_CAST(op);

    if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(self->compiled_procedure,
                                                           path, format))) {
        set_py_error_from_context(get_state_from_object(op), self->compiled_procedure->context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
CompiledProcedureObject_jit(PyObject *op, PyObject *raw_symbols)
{
    assert(op != NULL);
    CompiledProcedureObject *self = CompiledProcedureObject_CAST(op);
    _unit_state *state = get_state_from_object(op);

    PyObject *symbols = PySequence_Fast(raw_symbols, "expected an iterable for symbols");
    if (symbols == NULL) {
        return NULL;
    }

    UNIT_SymbolMap symbol_map;
    if (UNIT_FAILED(UNIT_SymbolMap_Init(&symbol_map, self->compiled_procedure->context))) {
        Py_DECREF(symbols);
        set_py_error_from_context(state, self->compiled_procedure->context);
        return NULL;
    }

    Py_ssize_t size = PySequence_Fast_GET_SIZE(symbols);
    for (Py_ssize_t index = 0; index < size; ++index) {
        PyObject *item = PySequence_Fast_GET_ITEM(symbols, index);
        assert(item != NULL);
        if (!PyTuple_Check(item) || PyTuple_GET_SIZE(item) != 2) {
            Py_DECREF(symbols);
            PyErr_Format(PyExc_TypeError, "expected a tuple with 2 items, got %R",
                         item);
            UNIT_SymbolMap_Clear(&symbol_map);
            return NULL;
        }

        PyObject *name_obj = PyTuple_GET_ITEM(item, 0);
        const char *name = PyUnicode_AsUTF8(name_obj);
        if (name == NULL) {
            Py_DECREF(symbols);
            UNIT_SymbolMap_Clear(&symbol_map);
            return NULL;
        }

        PyObject *address_obj = PyTuple_GET_ITEM(item, 1);

        void *address = PyLong_AsVoidPtr(address_obj);
        if (address == NULL && PyErr_Occurred()) {
            Py_DECREF(symbols);
            UNIT_SymbolMap_Clear(&symbol_map);
            return NULL;
        }

        if (UNIT_FAILED(UNIT_SymbolMap_RegisterSymbol(&symbol_map, name, address))) {
            Py_DECREF(symbols);
            UNIT_SymbolMap_Clear(&symbol_map);
            set_py_error_from_context(state, self->compiled_procedure->context);
            return NULL;
        }
    }

    UNIT_ExecutableBuffer *buffer = UNIT_CompiledProcedure_JIT(self->compiled_procedure,
                                                               &symbol_map);
    UNIT_SymbolMap_Clear(&symbol_map);
    if (buffer == NULL) {
        set_py_error_from_context(state, self->compiled_procedure->context);
        return NULL;
    }

    return ExecutableBufferObject_internal_create((PyTypeObject *)state->ExecutableBufferType,
                                                  buffer);
}

static PyObject *
CompiledProcedureObject_print_translation(PyObject *op, PyObject *unused)
{
    assert(op != NULL);
    CompiledProcedureObject *self = CompiledProcedureObject_CAST(op);

    FILE *stream = tmpfile();
    if (stream == NULL) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    if (UNIT_FAILED(UNIT_CompiledProcedure_PrintTranslatedIR(self->compiled_procedure,
                                                             stream))) {
        set_py_error_from_context(get_state_from_object(op),
                                  self->compiled_procedure->context);
        return NULL;
    }

    return tmpfile_stream_to_text(stream);
}

static int
CompiledProcedureObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
CompiledProcedureObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    CompiledProcedureObject *compiled = CompiledProcedureObject_CAST(op);
    UNIT_CompiledProcedure_Free(compiled->compiled_procedure);

    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyMethodDef CompiledProcedureObject_methods[] = {
    {"write_object_file", CompiledProcedureObject_write_object_file, METH_VARARGS, NULL},
    {"jit", CompiledProcedureObject_jit, METH_O, NULL},
    {"print_translation", CompiledProcedureObject_print_translation, METH_NOARGS, NULL},
    {NULL},
};

static PyType_Slot CompiledProcedureType_slots[] = {
    {Py_tp_traverse, CompiledProcedureObject_traverse},
    {Py_tp_dealloc, CompiledProcedureObject_dealloc},
    {Py_tp_methods, CompiledProcedureObject_methods},
    {0, 0},  /* sentinel */
};

static PyType_Spec CompiledProcedureType_spec = {
    .name = "CompiledProcedure",
    .basicsize = sizeof(CompiledProcedureObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = CompiledProcedureType_slots,
};

typedef struct {
    PyObject_HEAD
    UNIT_Local local;
} LocalObject;

#define LocalObject_CAST(op) ((LocalObject *)op)

static PyObject *
LocalObject_internal_create(PyTypeObject *cls, UNIT_Local local)
{
    assert(cls != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        return NULL;
    }

    LocalObject *self = LocalObject_CAST(op);
    self->local = local;
    return op;
}

static int
LocalObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
LocalObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyType_Slot LocalType_slots[] = {
    {Py_tp_traverse, LocalObject_traverse},
    {Py_tp_dealloc, LocalObject_dealloc},
    {0, 0},  /* sentinel */
};

static PyType_Spec LocalType_spec = {
    .name = "Local",
    .basicsize = sizeof(LocalObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = LocalType_slots,
};

typedef struct {
    PyObject_HEAD
    UNIT_JumpLabel *label;
} JumpLabelObject;

#define JumpLabelObject_CAST(op) ((JumpLabelObject *)op)

static PyObject *
JumpLabelObject_internal_create(PyTypeObject *cls, UNIT_JumpLabel *label)
{
    assert(cls != NULL);
    assert(label != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        return NULL;
    }

    JumpLabelObject *self = JumpLabelObject_CAST(op);
    self->label = label;
    return op;
}

static int
JumpLabelObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
JumpLabelObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyType_Slot JumpLabelType_slots[] = {
    {Py_tp_traverse, JumpLabelObject_traverse},
    {Py_tp_dealloc, JumpLabelObject_dealloc},
    {0, 0},  /* sentinel */
};

static PyType_Spec JumpLabelType_spec = {
    .name = "JumpLabel",
    .basicsize = sizeof(JumpLabelObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = JumpLabelType_slots,
};

typedef struct {
    PyObject_HEAD
    PyObject *context;
    UNIT_Procedure procedure;
} ProcedureObject;

#define ProcedureObject_CAST(op) ((ProcedureObject *)op);

static PyObject *
ProcedureObject_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    _unit_state *state = get_state_from_type(cls);

    PyObject *context_op;
    const char *name;
    if (!PyArg_ParseTuple(args, "O!s", state->ContextType, &context_op, &name)) {
        return NULL;
    }

    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        return NULL;
    }

    ContextObject *context = ContextObject_CAST(context_op);
    ProcedureObject *procedure = ProcedureObject_CAST(op);
    if (UNIT_FAILED(UNIT_Procedure_Init(&procedure->procedure, &context->context, name))) {
        Py_DECREF(op);
        set_py_error_from_context(state, &context->context);
        return NULL;
    }

    procedure->context = Py_NewRef(context_op);
    return op;
}

static PyObject *
ProcedureObject_add_operation(PyObject *op, PyObject *args)
{
    assert(op != NULL);
    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_Instruction instruction;
    int64_t oparg;

    if (!PyArg_ParseTuple(args, "iL", &instruction, &oparg)) {
        return NULL;
    }

    if (UNIT_FAILED(UNIT_Procedure_AddOperation(&self->procedure, instruction, oparg))) {
        set_py_error_from_context(get_state_from_object(op), self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_load_string(PyObject *op, PyObject *string_obj)
{
    assert(op != NULL);
    assert(string_obj != NULL);
    ProcedureObject *self = ProcedureObject_CAST(op);

    if (!PyUnicode_Check(string_obj)) {
        PyErr_Format(PyExc_TypeError, "expected a string, got %R", string_obj);
        return NULL;
    }

    const char *string = PyUnicode_AsUTF8(string_obj);
    if (string == NULL) {
        return NULL;
    }

    if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&self->procedure, string))) {
        set_py_error_from_context(get_state_from_object(op), self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_create_jump_label(PyObject *op, PyObject *name_obj)
{
    if (!PyUnicode_Check(name_obj)) {
        PyErr_Format(PyExc_TypeError, "expected a string, got %R", name_obj);
        return NULL;
    }

    const char *name = PyUnicode_AsUTF8(name_obj);
    if (name == NULL) {
        return NULL;
    }

    _unit_state *state = get_state_from_object(op);
    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_JumpLabel *label = UNIT_Procedure_CreateJumpLabel(&self->procedure, name);
    if (label == NULL) {
        set_py_error_from_context(state, self->procedure.context);
        return NULL;
    }

    return JumpLabelObject_internal_create((PyTypeObject *)state->JumpLabelType, label);
}

static PyObject *
ProcedureObject_add_jump(PyObject *op, PyObject *args)
{
    UNIT_Instruction instruction;
    PyObject *jump_label_obj;
    _unit_state *state = get_state_from_object(op);

    if (!PyArg_ParseTuple(args, "iO!", &instruction, state->JumpLabelType, &jump_label_obj)) {
        return NULL;
    }

    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_JumpLabel *label = JumpLabelObject_CAST(jump_label_obj)->label;
    assert(label != NULL);

    if (UNIT_FAILED(UNIT_Procedure_AddJump(&self->procedure, instruction, label))) {
        set_py_error_from_context(state, self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_use_label(PyObject *op, PyObject *jump_label)
{
    _unit_state *state = get_state_from_object(op);
    if (!PyObject_TypeCheck(jump_label, (PyTypeObject *)state->JumpLabelType)) {
        PyErr_Format(PyExc_TypeError, "expected a jump label, got %R", jump_label);
        return NULL;
    }

    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_JumpLabel *label = JumpLabelObject_CAST(jump_label)->label;
    assert(label != NULL);

    if (UNIT_FAILED(UNIT_Procedure_UseLabel(&self->procedure, label))) {
        set_py_error_from_context(state, self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_add_call_name(PyObject *op, PyObject *args)
{
    const char *name;
    int nargs;

    if (!PyArg_ParseTuple(args, "si", &name, &nargs)) {
        return NULL;
    }

    ProcedureObject *self = ProcedureObject_CAST(op);
    if (UNIT_FAILED(UNIT_Procedure_AddCallName(&self->procedure, name, nargs))) {
        set_py_error_from_context(get_state_from_object(op), self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_compile(PyObject *op, PyObject *platform_obj)
{
    assert(op != NULL);
    assert(platform_obj != NULL);

    if (!PyLong_Check(platform_obj)) {
        PyErr_Format(PyExc_TypeError, "expected a number, got %R", platform_obj);
        return NULL;
    }

    UNIT_Platform platform;
    if (PyLong_AsUInt32(platform_obj, &platform) < 0) {
        return NULL;
    }

    _unit_state *state = get_state_from_object(op);
    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_CompiledProcedure *compiled_procedure = UNIT_Compile(&self->procedure,
                                                              platform);
    if (compiled_procedure == NULL) {
        set_py_error_from_context(state, self->procedure.context);
        return NULL;
    }

    return CompiledProcedureObject_internal_create((PyTypeObject *)state->CompiledProcedureType,
                                                    compiled_procedure);
}

static PyObject *
ProcedureObject_optimize(PyObject *op, PyObject *unused)
{
    assert(op != NULL);
    ProcedureObject *self = ProcedureObject_CAST(op);

    if (UNIT_FAILED(UNIT_Procedure_Optimize(&self->procedure))) {
        set_py_error_from_context(get_state_from_object(op), self->procedure.context);
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_set_flags(PyObject *op, PyObject *value)
{
    assert(op != NULL);
    ProcedureObject *self = ProcedureObject_CAST(op);
    uint32_t flags;
    if (PyLong_AsUInt32(value, &flags) < 0) {
        return NULL;
    }
    UNIT_Procedure_SetFlags(&self->procedure, flags);

    Py_RETURN_NONE;
}

static PyObject *
ProcedureObject_print_instructions(PyObject *op, PyObject *arg)
{
    assert(op != NULL);
    assert(arg != NULL);
    ProcedureObject *self = ProcedureObject_CAST(op);

    int8_t visualize_stack_effect = PyLong_AsInt(arg);
    if (visualize_stack_effect == -1) {
        return NULL;
    }

    FILE *stream = tmpfile();
    if (stream == NULL) {
        PyErr_SetFromErrno(PyExc_OSError);
        return NULL;
    }

    if (UNIT_FAILED(UNIT_Procedure_PrintInstructions(&self->procedure, stream, visualize_stack_effect))) {
        fclose(stream);
        set_py_error_from_context(get_state_from_object(op), self->procedure.context);
        return NULL;
    }

    return tmpfile_stream_to_text(stream);
}

static int
ProcedureObject_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    ProcedureObject *procedure = ProcedureObject_CAST(op);
    Py_VISIT(procedure->context);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static int
ProcedureObject_clear(PyObject *op)
{
    assert(op != NULL);
    ProcedureObject *procedure = ProcedureObject_CAST(op);
    Py_CLEAR(procedure->context);
    return 0;
}

static void
ProcedureObject_dealloc(PyObject *op)
{
    assert(op != NULL);
    ProcedureObject *procedure = ProcedureObject_CAST(op);
    UNIT_Procedure_Clear(&procedure->procedure);
    (void)ProcedureObject_clear(op);
    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyMethodDef ProcedureObject_methods[] = {
    {"create_jump_label", ProcedureObject_create_jump_label, METH_O, NULL},
    {"add_jump", ProcedureObject_add_jump, METH_VARARGS, NULL},
    {"use_label", ProcedureObject_use_label, METH_O, NULL},
    {"add_call_name", ProcedureObject_add_call_name, METH_VARARGS, NULL},
    {"add_operation", ProcedureObject_add_operation, METH_VARARGS, NULL},
    {"add_string_load", ProcedureObject_load_string, METH_O, NULL},
    {"compile", ProcedureObject_compile, METH_O, NULL},
    {"optimize", ProcedureObject_optimize, METH_NOARGS, NULL},
    {"set_flags", ProcedureObject_set_flags, METH_O, NULL},
    {"print_instructions", ProcedureObject_print_instructions, METH_O, NULL},
    {NULL},
};

static PyType_Slot ProcedureType_slots[] = {
    {Py_tp_new, ProcedureObject_new},
    {Py_tp_methods, ProcedureObject_methods},
    {Py_tp_traverse, ProcedureObject_traverse},
    {Py_tp_clear, ProcedureObject_clear},
    {Py_tp_dealloc, ProcedureObject_dealloc},
    {0, 0},  /* sentinel */
};

static PyType_Spec ProcedureType_spec = {
    .name = "Procedure",
    .basicsize = sizeof(ProcedureObject),
    .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE,
    .slots = ProcedureType_slots,
};

static int
_unit_modexec(PyObject *module)
{
    _unit_state *state = PyModule_GetState(module);

    state->ErrorType = PyErr_NewException("_unit.Error", NULL, NULL);
    if (state->ErrorType == NULL) {
        return -1;
    }
    if (PyModule_AddType(module, (PyTypeObject *)state->ErrorType) < 0) {
        return -1;
    }

#define ADD_TYPE(name)                                                                  \
    state->name## Type = PyType_FromModuleAndSpec(module, &name## Type_spec, NULL);     \
    if (state->name## Type == NULL) {                                                   \
        return -1;                                                                      \
    }                                                                                   \
    if (PyModule_AddType(module, (PyTypeObject *)state->name## Type) < 0) {             \
        return -1;                                                                      \
    }

    ADD_TYPE(Context);
    ADD_TYPE(CompiledProcedure);
    ADD_TYPE(Procedure);
    ADD_TYPE(JumpLabel);
    ADD_TYPE(Local);
    ADD_TYPE(ExecutableBuffer);

#undef ADD_TYPE

#define EXPORT_CONST(name)                                  \
    if (PyModule_AddIntConstant(module, #name, name)) {     \
        return -1;                                          \
    }

    EXPORT_CONST(UNIT_OP_LOAD_STRING);
    EXPORT_CONST(UNIT_OP_LOAD_INTEGER);

    EXPORT_CONST(UNIT_OP_LOAD_LOCAL);
    EXPORT_CONST(UNIT_OP_STORE_LOCAL);

    EXPORT_CONST(UNIT_OP_ADD);
    EXPORT_CONST(UNIT_OP_SUBTRACT);
    EXPORT_CONST(UNIT_OP_MULTIPLY);
    EXPORT_CONST(UNIT_OP_DIVIDE);
    EXPORT_CONST(UNIT_OP_MODULO);

    EXPORT_CONST(UNIT_OP_JUMP_TO);
    EXPORT_CONST(UNIT_OP_JUMP_IF_FALSE);
    EXPORT_CONST(UNIT_OP_JUMP_IF_TRUE);

    EXPORT_CONST(UNIT_OP_EXIT);
    EXPORT_CONST(UNIT_OP_RETURN_VALUE);
    EXPORT_CONST(UNIT_OP_LOAD_ARGUMENT);

    EXPORT_CONST(UNIT_OP_PREPARE_CALL);
    EXPORT_CONST(UNIT_OP_CALL_NAME);
    EXPORT_CONST(UNIT_OP_CALL_PROCEDURE);

    EXPORT_CONST(UNIT_OP_COMPARE_EQUAL);
    EXPORT_CONST(UNIT_OP_COMPARE_NOT_EQUAL);
    EXPORT_CONST(UNIT_OP_COMPARE_GREATER);
    EXPORT_CONST(UNIT_OP_COMPARE_GREATER_EQUAL);
    EXPORT_CONST(UNIT_OP_COMPARE_LESS);
    EXPORT_CONST(UNIT_OP_COMPARE_LESS_EQUAL);

    EXPORT_CONST(UNIT_OP_COPY);
    EXPORT_CONST(UNIT_OP_SWAP);
    EXPORT_CONST(UNIT_OP_POP);

    EXPORT_CONST(UNIT_OP_ADDRESS_OF);
    EXPORT_CONST(UNIT_OP_READ_BYTES);
    EXPORT_CONST(UNIT_OP_WRITE_BYTES);

    EXPORT_CONST(UNIT_OP_CONVERT);

    EXPORT_CONST(UNIT_TYPE_INT8);
    EXPORT_CONST(UNIT_TYPE_INT16);
    EXPORT_CONST(UNIT_TYPE_INT32);
    EXPORT_CONST(UNIT_TYPE_INT64);
    EXPORT_CONST(UNIT_TYPE_UINT8);
    EXPORT_CONST(UNIT_TYPE_UINT16);
    EXPORT_CONST(UNIT_TYPE_UINT32);
    EXPORT_CONST(UNIT_TYPE_UINT64);

    EXPORT_CONST(UNIT_FORMAT_ELF);
    EXPORT_CONST(UNIT_FORMAT_MACHO);
    EXPORT_CONST(UNIT_FORMAT_PE);

    EXPORT_CONST(UNIT_HOST_PLATFORM);

    EXPORT_CONST(_UNIT_ABI_MASK);
    EXPORT_CONST(UNIT_ABI_SYSTEMV);
    EXPORT_CONST(UNIT_ABI_APPLE);
    EXPORT_CONST(UNIT_ABI_SYSTEMV);

    EXPORT_CONST(_UNIT_ARCH_MASK);
    EXPORT_CONST(UNIT_ARCH_AMD64);
    EXPORT_CONST(UNIT_ARCH_AARCH64);

    EXPORT_CONST(UNIT_FLAG_NONE);
    EXPORT_CONST(UNIT_FLAG_FORCE_INLINE);
    EXPORT_CONST(UNIT_FLAG_FORCE_NO_INLINE);
    EXPORT_CONST(UNIT_FLAG_NO_OPTIMIZE_TRANSLATION);
    EXPORT_CONST(UNIT_FLAG_PRINT_TRANSLATION_PREOP);
    EXPORT_CONST(UNIT_FLAG_PRINT_TRANSLATION_POSTOP);

#undef EXPORT_CONST

    if (PyModule_AddStringConstant(module, "UNIT_VERSION_STRING", UNIT_VERSION_STRING)) {
        return -1;
    }

    return 0;
}

static PyModuleDef_Slot _unit_slots[] = {
    {Py_mod_exec, _unit_modexec},

#if PY_VERSION_HEX >= 0x30c0000
    {Py_mod_multiple_interpreters, Py_MOD_PER_INTERPRETER_GIL_SUPPORTED},
#endif

#if PY_VERSION_HEX >= 0x30d0000
    {Py_mod_gil, Py_MOD_GIL_NOT_USED},
#endif

    {0, NULL}
};

static int
_unit_traverse(PyObject *module, visitproc visit, void *arg)
{
    assert(module != NULL);
    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);
    Py_VISIT(state->ContextType);
    Py_VISIT(state->ErrorType);
    Py_VISIT(state->ProcedureType);
    Py_VISIT(state->CompiledProcedureType);
    Py_VISIT(state->JumpLabelType);
    Py_VISIT(state->LocalType);
    return 0;
}

static int
_unit_clear(PyObject *module)
{
    assert(module != NULL);
    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);
    Py_CLEAR(state->ContextType);
    Py_CLEAR(state->ErrorType);
    Py_CLEAR(state->ProcedureType);
    Py_CLEAR(state->CompiledProcedureType);
    Py_CLEAR(state->JumpLabelType);
    Py_CLEAR(state->LocalType);
    return 0;
}

static void
_unit_free(void *module)
{
    assert(module != NULL);
    (void)_unit_clear((PyObject *)module);
}

static struct PyModuleDef _unit_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_core",
    .m_size = sizeof(_unit_state),
    .m_slots = _unit_slots,
    .m_traverse = _unit_traverse,
    .m_clear = _unit_clear,
    .m_free = _unit_free,
};

PyMODINIT_FUNC
PyInit__core(void)
{
    return PyModuleDef_Init(&_unit_module);
}
