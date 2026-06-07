Compilation
===========

Compiling a procedure
---------------------

Now that we have a working ``UNIT_Procedure`` type, let's compile it to machine
code!

There is a single function to compile a procedure, called :c:func:`UNIT_Compile`.
It takes two arguments:

1. A pointer to the procedure we want to compile.
2. The target architecture, as a :c:enum:`UNIT_Architecture`.

Currently, UNIT only supports the AMD64 architecture, so let's pass
:c:enum:`UNIT_ARCH_AMD64`.

.. note::

   AMD64 has many different names. You might be used to reading it as
   "x86-64", "x64", or "Intel 64".

Now, our code looks like this:

.. code-block:: c
   :caption: main.c

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&procedure);
            return 1;
        }

    #define ADDOP_INT(op, value)                                                \
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
            UNIT_PrintError(&context, stderr);                                  \
            UNIT_Procedure_Clear(&context);                                     \
            UNIT_Context_Clear(&procedure);                                     \
            return 1;                                                           \
        }

    #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

    #undef ADDOP_INT
    #undef ADDOP

        UNIT_CompiledProcedure *compiled = UNIT_Compile(procedure, UNIT_ARCH_AMD64);

        UNIT_Procedure_Clear(&procedure)
        UNIT_Context_Clear(&context);
        return 0;
    }
