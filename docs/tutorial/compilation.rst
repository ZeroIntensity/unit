Compiling a simple program
==========================

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
that the object file will be stored in. For simplicity, we use :c:macro:`UNIT_HOST_FORMAT`
to auto-detect this based on the current system.

.. note::

   On macOS, ``UNIT_HOST_FORMAT`` resolves to :c:macro:`UNIT_FORMAT_MACHO`. UNIT does
   not currently support this format.


.. code-block:: c

    if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                           UNIT_HOST_FORMAT))) {
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

Now, before we start building things, let's go over some other important
information about compilation.

JIT compilation
---------------

Writing an object file and linking it is useful for ahead-of-time compilation,
but sometimes you want to compile and run code immediately in the same process.
This is called JIT (Just-In-Time) compilation.

:c:func:`UNIT_CompiledProcedure_JIT` maps the compiled machine code into
executable memory in a type known as :c:type:`UNIT_ExecutableBuffer`.
We can get a pointer to the executable memory through
:c:func:`UNIT_ExecutableBuffer_GetPointer`.

Like with :c:type:`UNIT_CompiledProcedure`, a ``UNIT_ExecutableBuffer`` is
heap-allocated memory and must be freed later (via :c:func:`UNIT_ExecutableBuffer_Free`).

.. code-block:: c

    UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL /* more on this in a moment */);
    if (buf == NULL) {
        UNIT_PrintError(&context, stderr);
        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }

    // Cast the raw pointer to a function pointer and call it
    int64_t (*my_main)(void) = (int64_t (*)(void))UNIT_ExecutableBuffer_GetPointer(buf);

    int64_t result = my_main();
    printf("returned: %ld\n", result); // returned: 0

    UNIT_ExecutableBuffer_Free(buf);

The second argument to :c:func:`UNIT_CompiledProcedure_JIT` is a
:c:struct:`UNIT_SymbolMap` for resolving external function names. We pass
``NULL`` here because our procedure doesn't call any external functions yet.
We will need it later when we call ``printf`` and ``scanf``.


Optimization
------------

Before compiling, you can also run optimization passes on the procedure with
:c:func:`UNIT_Procedure_Optimize`:

.. code-block:: c

    if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
        /* ... */
    }
    UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);

This will modify the procedure's instructions to generally make it more efficient.
Optimization is a one-way street; an optimized procedure cannot be "unoptimized".

Optimization is optional but recommended. Note that UNIT runs two optimization passes.
This one is only on the stack IR, but during compilation, there's a second, more simple
pass on the translated register IR. This second optimization pass is enabled by default;
it can be disabled by setting :c:macro:`UNIT_FLAG_NO_OPTIMIZE_TRANSLATION` on the procedure.

For our simple "return 0" procedure, optimization has no effect, but as we
add more instructions, it will make a noticeable difference.


Debugging stack errors
----------------------

When using UNIT, you'll likely run into an error like this at some point:

.. code-block::

    [INVALID USAGE] stack underflow at SOME_INSTRUCTION

Or this:

.. code-block::

    [INVALID USAGE] procedure did not consume entire stack

This means that there is a stack-effect error somewhere in your IR.
To debug this, we can use a function called :c:func:`UNIT_Procedure_PrintInstructions`,
which prints all the instructions in a procedure alongside a simulated stack
state after each instruction. This is very helpful for visualizing what your
IR is doing at translation time, and often makes it very easy to determine
what is wrong with your IR.

It can be used like this:

.. code-block:: c

    UNIT_Procedure_PrintInstructions(&procedure, stdout, /*visualize_stack_effect=*/1);

Output:

.. code-block::

    procedure "main":
        0    LOAD_INTEGER  0
        [0]
        1    RETURN_VALUE
        []


Debugging logical errors
------------------------

For debugging logical errors in your IR, another helpful function is
:c:func:`UNIT_CompiledProcedure_PrintTranslatedIR`, which prints the translated
register machine IR. For many people, this can be easier to read than the stack
machine IR, because it resembles an actual programming language and also because
it's closer to what your CPU actually executes.

It can be used like this:

.. code-block:: c

    UNIT_CompiledProcedure_PrintTranslatedIR(compiled, stdout);

Sample output:

.. code-block::

    translation for "main":
        block 0
            RETURN_VALUE(0)
        block 1

.. hint::

    During translation, UNIT splits up your code into blocks of linear control flow
    (also known as a `basic block <https://en.wikipedia.org/wiki/Basic_block>`_)
    for the sake of optimization and register allocation. Blocks will be
    split at jumps and at returns.

If the stack IR looks wrong, your instructions are wrong.
If the stack IR looks right but the register IR looks wrong,
you may have found a bug in UNIT -- please file an issue!


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
                goto error;                                                         \
            }

        #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

        if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
            goto error;
        }

        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            goto error;
        }

        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                               UNIT_HOST_FORMAT))) {
            UNIT_CompiledProcedure_Free(compiled);
            goto error;
        }

        printf("Wrote output.o\n");

        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    error:
        UNIT_PrintError(&context, stderr);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
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
