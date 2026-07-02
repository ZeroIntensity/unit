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


JIT compilation
---------------

Writing an object file and linking it is useful for ahead-of-time compilation,
but sometimes you want to compile and run code immediately. This is called
JIT (Just-In-Time) compilation.

:c:func:`UNIT_CompiledProcedure_JIT` maps the compiled machine code into
executable memory and returns a pointer to it:

.. code-block:: c

    UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);
    if (buf == NULL) {
        UNIT_PrintError(&context, stderr);
        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }

    // Cast the raw pointer to a function pointer and call it
    int64_t (*my_main)(void) =
        (int64_t (*)(void))UNIT_ExecutableBuffer_GetPointer(buf);

    int64_t result = my_main();
    printf("returned: %ld\n", result);  // returned: 0

    UNIT_ExecutableBuffer_Free(buf);

The second argument to :c:func:`UNIT_CompiledProcedure_JIT` is a
:c:struct:`UNIT_SymbolMap` for resolving external function names. We pass
``NULL`` here because our procedure doesn't call any external functions yet.
We will need it later when we call ``printf`` and ``scanf``.

.. note::

   JIT compilation uses ``mmap`` (or ``VirtualAlloc`` on Windows) to allocate
   memory with execute permissions. The :c:func:`UNIT_ExecutableBuffer_Free`
   function unmaps this memory. Always free your buffers.


Optimization
------------

Before compiling, you can run optimization passes on the procedure with
:c:func:`UNIT_Procedure_Optimize`:

.. code-block:: c

    UNIT_Procedure_Optimize(&procedure);
    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);

Optimization is optional but recommended. It runs the following passes:

- **Inlining** -- small procedures called via :c:func:`UNIT_Procedure_AddCallProcedure`
  are spliced into the caller.
- **Constant folding** -- arithmetic on constants is computed at compile time.
  ``LOAD_INTEGER 3; LOAD_INTEGER 4; ADD`` becomes ``LOAD_INTEGER 7``.
- **Dead code elimination** -- unreachable instructions after unconditional
  jumps or returns are removed.
- **Dead store elimination** -- stores to local variables that are never
  loaded are removed.

For our simple "return 0" procedure, optimization has no effect. But as we
add more instructions, it will make a noticeable difference.


Debugging the output
--------------------

When things go wrong, it helps to see what UNIT is doing. Two functions
are particularly useful:

:c:func:`UNIT_Procedure_PrintInstructions` prints the stack IR with the
simulated stack state after each instruction:

.. code-block:: c

    UNIT_Procedure_PrintInstructions(&procedure, stdout, 1);

Output::

    procedure "main":
        0    LOAD_INTEGER  0      [0]
        1    RETURN_VALUE         []

:c:func:`UNIT_CompiledProcedure_PrintTranslatedIR` prints the register IR
after register allocation -- this is closer to what the CPU actually executes:

.. code-block:: c

    UNIT_CompiledProcedure_PrintTranslatedIR(compiled, stdout);

Output::

    translation for "main":
        block 0
            RETURN_VALUE(0)

These are invaluable when debugging. If the stack IR looks wrong, your
instructions are wrong. If the stack IR looks right but the register IR
looks wrong, you may have found a bug in UNIT -- please file an issue!


Putting it together
-------------------

Here is the complete program so far. We use the object file approach for
the guessing game since it needs to be the real ``main`` function:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>
    #include <stdio.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            fprintf(stderr, "failed to initialize context\n");
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
            goto cleanup;                                                       \
        }

    #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

        // Optimize
        UNIT_Procedure_Optimize(&procedure);

        // Compile
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            UNIT_PrintError(&context, stderr);
            goto cleanup;
        }

        // Write object file
        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                               UNIT_FORMAT_ELF))) {
            UNIT_PrintError(&context, stderr);
        }

        printf("Wrote output.o\n");

    #undef ADDOP_INT
    #undef ADDOP

        UNIT_CompiledProcedure_Free(compiled);
    cleanup:
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    }

.. code-block:: bash

   gcc main.c -lunit -o guessing_game
   ./guessing_game
   Wrote output.o
   gcc output.o -o output
   ./output
   echo $?
   0

We now have a working compiler pipeline. In the next section, we will start
building the actual guessing game by adding arithmetic, external function
calls, and control flow.
