Your First Program With UNIT
============================

Choosing an interface
---------------------

UNIT is accessible as a library in three languages:

1. C. This is the most native variation, as UNIT is itself written in C.
2. C++. This is a thin wrapper over the C bindings.
3. Python. This is very easy to use, but requires installing an additional PyPI package.

Each option comes with its own benefits and tradeoffs.
Choose the interface that best suits your needs.

The Example
-----------


C Example
---------

Start with some C code using UNIT:

.. code-block:: c
    :linenos:
    :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        UNIT_Context_Init(&context);
        UNIT_Procedure procedure;
        UNIT_Procedure_Init(&procedure, &context, "main");

        // Pushes the string "Hello, world!" onto the virtual stack
        UNIT_Procedure_AddStringLoad(&procedure, "Hello, world!");

        // Calls "puts" and consume 1 argument off the virtual stack.
        // (In this case, that means our string "Hello, world!" will be consumed.)
        UNIT_Procedure_AddCall(&procedure, "puts", 1);

        // The top of the stack is now the result of calling puts().
        // We don't actually care about it, so we pop it off.
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_POP, 0 /* This value doesn't matter for POP */);

        // Now, we want to return 0.
        // First, push 0 onto the stack, which will be consumed by the UNIT_OP_RETURN_VALUE opcode.
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 0);
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_RETURN_VALUE, 0 /* doesn't matter */);

        // Finally, we can compile our procedure into an object file
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        UNIT_CompiledProcedure_WriteObjectFile(compiled, "test.o", UNIT_FORMAT_ELF);
        UNIT_CompiledProcedure_Free(compiled);

        // Clean up the procedure and context
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    }

.. note::

   The above code does not have proper error handling. In real applications,
   most calls need to be inside of a :c:macro:`UNIT_FAILED` check.




Compiling and running the example
*********************************
