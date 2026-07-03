Compilation
===========

Compiling a procedure
---------------------

Now that we have a working ``UNIT_Procedure`` type, let's compile it to machine
code!

There is a single function to compile a procedure, called :c:func:`UNIT_Compile`.
It takes two arguments:

1. A pointer to the procedure we want to compile.
2. The target platform -- a combination of the architecture and ABI.

The current platform is accessible via the :c:macro:`UNIT_HOST_PLATFORM` macro.

Currently, UNIT only supports the AMD64 architecture, so let's pass
:c:macro:`UNIT_HOST_PLATFORM`.

.. note::

   AMD64 has many different names. You might be used to reading it as
   "x86-64", "x64", or "Intel 64".

:c:func:`UNIT_Compile` returns a heap-allocated :c:struct:`UNIT_CompiledProcedure`,
or ``NULL`` on failure. When we're done with it, we need to free it with
:c:func:`UNIT_CompiledProcedure_Free`.

Now, our code looks like this:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>
    #include <stdio.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

    #define ADDOP_INT(op, value)                                                \
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
            UNIT_PrintError(&context, stderr);                                  \
            UNIT_Procedure_Clear(&procedure);                                   \
            UNIT_Context_Clear(&context);                                       \
            return 1;                                                           \
        }

    #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

    #undef ADDOP_INT
    #undef ADDOP

        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            UNIT_PrintError(&context, stderr);
            UNIT_Procedure_Clear(&procedure);
            UNIT_Context_Clear(&context);
            return 1;
        }

        // We will use the compiled procedure in a moment.

        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    }


Writing an object file
----------------------

The most straightforward way to use the compiled procedure is to write it
to an object file. For this, we can use
:c:func:`UNIT_CompiledProcedure_WriteObjectFile`. We need to pass the format
that the object file will be stored in. For Linux, this is ELF, so we pass
:c:macro:`UNIT_FORMAT_ELF`.

.. note::

    Windows uses the Portable Executable (PE) format (:c:macro:`UNIT_FORMAT_PE`),
    and macOS uses the Mach Object (Mach-O) format (:c:macro:`UNIT_FORMAT_MACHO`).

    UNIT does not support either of these at the moment; trying to pass them to
    :c:func:`UNIT_CompiledProcedure_WriteObjectFile` will result in an error
    being set at runtime.


.. code-block:: c

    if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                           UNIT_FORMAT_ELF))) {
        UNIT_PrintError(&context, stderr);
        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }

Build and run the compiler, then link the output:

.. code-block:: bash

   gcc main.c -lunit -o guessing_game
   ./guessing_game
   gcc output.o -o output
   ./output
   echo $?

The last command prints ``0`` -- our procedure returned successfully!
