Your first program with UNIT
============================

This section provides instructions on how to build a "hello world" program
with UNIT.

Example
-------

Start with some C code using UNIT:

.. code-block:: c

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
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_POP_TOP, 0 /* This value doesn't matter for POP_TOP */);

        // Now, we want to return 0.
        // First, push 0 onto the stack, which will be consumed by the UNIT_OP_RETURN_VALUE opcode.
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 0);
        UNIT_Procedure_AddOperation(&procedure, UNIT_OP_RETURN_VALUE, 0 /* doesn't matter */);

        // Finally, we can compile our procedure into an object file
        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_ARCH_AMD64);
        UNIT_CompiledProcedure_WriteObjectFile(compiled, "test.o", UNIT_FORMAT_ELF);
        UNIT_CompiledProcedure_Free(compiled);

        // Clean up the procedure and context
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    }

.. note::

   The above code does not have proper error handling. In real applications,
   each call needs to be inside of a ``UNIT_FAILED`` check.


Compiling and running the example
*********************************
