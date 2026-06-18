#include <Python.h>
#include <unit/unit.h>

typedef struct {
    PyObject *ErrorType;
    PyObject *ContextType;
    PyObject *CompiledProcedureType;
    PyObject *ProcedureType;
} _unit_state;

static void
set_py_error_from_context(PyObject *module, UNIT_Context *context)
{
    assert(!PyErr_Occurred());
    assert(module != NULL);
    assert(context != NULL);
    assert(PyModule_Check(module));
    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);
    assert(state->ErrorType != NULL);

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
    UNIT_CompiledProcedure *compiled_procedure;
} CompiledProcedureObject;

#define CompiledProcedureObject_CAST(op) ((CompiledProcedureObject *)op)

static PyObject *
CompiledProcedure_internal_create(PyTypeObject *cls,
                                  UNIT_CompiledProcedure *compiled_procedure)
{
    assert(cls != NULL);
    assert(compiled_procedure != NULL);
    PyObject *op = cls->tp_alloc(cls, 0);
    if (op == NULL) {
        UNIT_CompiledProcedure_Free(compiled_procedure);
        return NULL;
    }

    CompiledProcedureObject *compiled = CompiledProcedureObject_CAST(op);
    compiled->compiled_procedure = compiled_procedure;
    return op;
}

static int
CompiledProcedure_traverse(PyObject *op, visitproc visit, void *arg)
{
    assert(op != NULL);
    Py_VISIT(Py_TYPE(op));
    return 0;
}

static void
CompiledProcedure_dealloc(PyObject *op)
{
    assert(op != NULL);
    CompiledProcedureObject *compiled = CompiledProcedureObject_CAST(op);
    UNIT_CompiledProcedure_Free(compiled->compiled_procedure);
    PyTypeObject *cls = Py_TYPE(op);
    cls->tp_free(op);
    Py_DECREF(cls);
}

static PyType_Slot CompiledProcedureType_slots[] = {
    {Py_tp_traverse, CompiledProcedure_traverse},
    {Py_tp_dealloc, CompiledProcedure_dealloc},
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
    PyObject *context;
    UNIT_Procedure procedure;
} ProcedureObject;

#define ProcedureObject_CAST(op) ((ProcedureObject *)op);

static PyObject *
ProcedureObject_new(PyTypeObject *cls, PyObject *args, PyObject *kwds)
{
    assert(cls != NULL);
    PyObject *module = PyType_GetModule(cls);
    if (module == NULL) {
        return NULL;
    }

    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);

    PyObject *context_op;
    const char *name;
    if (PyArg_ParseTuple(args, "O!s", state->ContextType, &context_op, &name) < 0) {
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
        set_py_error_from_context(module, &context->context);
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

    if (PyArg_ParseTuple(args, "iL", &instruction, &oparg) < 0) {
        return NULL;
    }

    if (UNIT_FAILED(UNIT_Procedure_AddOperation(&self->procedure, instruction, oparg))) {
        PyObject *module = PyType_GetModule(Py_TYPE(op));
        if (module == NULL) {
            return NULL;
        }
        set_py_error_from_context(module, self->procedure.context);
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

    PyObject *module = PyType_GetModule(Py_TYPE(op));
    if (module == NULL) {
        return NULL;
    }

    ProcedureObject *self = ProcedureObject_CAST(op);
    UNIT_CompiledProcedure *compiled_procedure = UNIT_Compile(&self->procedure,
                                                              platform);
    if (compiled_procedure == NULL) {
        set_py_error_from_context(module, self->procedure.context);
        return NULL;
    }

    _unit_state *state = PyModule_GetState(module);
    assert(state != NULL);

    return CompiledProcedure_internal_create((PyTypeObject *)state->CompiledProcedureType,
                                             compiled_procedure);
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
    {"add_operation", ProcedureObject_add_operation, METH_VARARGS, NULL},
    {"compile", ProcedureObject_compile, METH_O, NULL},
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
