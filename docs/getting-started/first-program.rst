.. _first-program:

Your first program with UNIT
============================

This page walks through writing, compiling, and running a simple program
using UNIT. By the end, you'll have a compiled function that adds two
numbers -- first as an object file linked with ``gcc``, then as a
JIT-compiled function called directly from your program.

We'll use C for this tutorial. If you prefer C++ or Python, see the
:ref:`bindings <bindings>` page for how the same code looks in those
languages.


Setup
-----

Create a file called ``first.c``. Every UNIT program starts with a context
and a procedure:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c

   #include <unit/unit.h>
   #include <stdio.h>

   int main(void)
   {
       UNIT_Context ctx;
       if (UNIT_FAILED(UNIT_Context_Init(&ctx))) {
           fprintf(stderr, "failed to initialize context\n");
           return 1;
       }

       UNIT_Procedure proc;
       if (UNIT_FAILED(UNIT_Procedure_Init(&proc, &ctx, "add"))) {
           UNIT_PrintError(&ctx, stderr);
           UNIT_Context_Clear(&ctx);
           return 1;
       }

       // We'll add instructions here.

       UNIT_Procedure_Clear(&proc);
       UNIT_Context_Clear(&ctx);
       return 0;
   }

The :c:type:`UNIT_Context` owns all memory. The :c:type:`UNIT_Procedure`
holds the instructions for a single function. The name ``"add"`` becomes
the symbol name in the compiled output.

Build and run to make sure everything links:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   gcc first.c -lunit -o first
   ./first


Emitting instructions
---------------------

UNIT uses a stack-based instruction set. If you've worked with a bytecode
interpreter (Python, Java, WebAssembly), this will feel familiar. Values
are pushed onto a stack, and instructions consume values from the top.

Our ``add`` function takes two arguments and returns their sum. That's
three instructions:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c

   // int64_t add(int64_t a, int64_t b) { return a + b; }
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);  // push a
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 1);  // push b
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);            // pop both, push a+b
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);   // pop and return

.. attention::

   These functions usually need error handling, via :c:macro:`UNIT_FAILED`.
   For example's sake, it has been omitted for brevity.

After ``LOAD_ARGUMENT 0``, the stack is ``[a]``. After ``LOAD_ARGUMENT 1``,
it's ``[a, b]``. ``ADD`` pops both and pushes the sum: ``[a+b]``.
``RETURN_VALUE`` pops the result and returns it to the caller.

You can verify the instructions look correct by printing them:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c

   UNIT_Procedure_PrintInstructions(&proc, stdout, /*visualize_stack_effect=*/1);

This prints each instruction alongside the stack state, which is helpful
for debugging. If UNIT ever gives you an error during compilation complaining
about an instruction, try printing all the instructions to visualize the error.


Compiling to an object file
---------------------------

Now we compile the procedure and write it to an ELF object file:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c

   UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);
   if (compiled == NULL) {
       UNIT_PrintError(&ctx, stderr);
       UNIT_Procedure_Clear(&proc);
       UNIT_Context_Clear(&ctx);
       return 1;
   }

   UNIT_CompiledProcedure_WriteObjectFile(compiled, "add.o", UNIT_FORMAT_ELF);

:c:macro:`UNIT_HOST_PLATFORM` auto-detects your machine's architecture and
ABI. It's worth noting that UNIT will only work on x86-64 on ELF right now;
support for more architectures (notably AArch64) and other executable formats
(PE/COFF and Mach-O) will be added later.

In the above code, :c:func:`UNIT_Compile` translates the stack IR to register IR, runs
register allocation and optimization, and encodes the result as machine code.

.. note::

    UNIT has two phases of optimization. One of them is done on the stack IR
    (see :c:func:`UNIT_Procedure_Optimize`), and then second is done on the
    translated IR.

To use the compiled function, write a small driver and link them together:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` driver.c

   #include <stdio.h>
   #include <stdint.h>

   extern int64_t add(int64_t a, int64_t b);

   int main(void)
   {
       printf("%ld\n", add(3, 4));
       return 0;
   }

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ gcc first.c -lunit -o first
   $ ./first
   $ gcc driver.c add.o -o driver
   $ ./driver
   7


JIT compilation
---------------

Instead of writing an object file and linking separately, you can compile
and call the function directly in memory:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c

   UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);
   if (buf == NULL) {
       UNIT_PrintError(&ctx, stderr);
       UNIT_CompiledProcedure_Free(compiled);
       UNIT_Procedure_Clear(&proc);
       UNIT_Context_Clear(&ctx);
       return 1;
   }

   // Cast the raw pointer to a function pointer
   int64_t (*add)(int64_t, int64_t) = (int64_t (*)(int64_t, int64_t))UNIT_ExecutableBuffer_GetPointer(buf);

   printf("%ld\n", add(3, 4)); // prints 7

   UNIT_ExecutableBuffer_Free(buf);

The second argument to :c:func:`UNIT_CompiledProcedure_JIT` is a
:c:struct:`UNIT_SymbolMap` for custom symbol resolution. We pass ``NULL``
here because ``add`` doesn't call any external functions. See
:ref:`c-compilation` for details on symbol maps.


Optimization
------------

UNIT includes optimization passes that can improve the generated code.
Call :c:func:`UNIT_Procedure_Optimize` before compiling:

.. code-block:: c

   UNIT_Procedure_Optimize(&proc);
   UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);

For our simple ``add`` function, optimization won't change anything. But
for larger programs with constants, redundant loads, or inlineable function
calls, it makes a real difference. The optimizer runs constant folding,
dead code elimination, and function inlining.


Complete program
----------------

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` first.c
   :linenos:

   #include <unit/unit.h>
   #include <stdio.h>

   int main(void)
   {
       // Create a context
       UNIT_Context ctx;
       if (UNIT_FAILED(UNIT_Context_Init(&ctx))) {
           fprintf(stderr, "failed to initialize context\n");
           return 1;
       }

       // Create a procedure
       UNIT_Procedure proc;
       if (UNIT_FAILED(UNIT_Procedure_Init(&proc, &ctx, "add"))) {
           UNIT_PrintError(&ctx, stderr);
           UNIT_Context_Clear(&ctx);
           return 1;
       }

       // Emit instructions: int64_t add(int64_t a, int64_t b) { return a + b; }
       UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
       UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 1);
       UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);
       UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

       // Optimize
       UNIT_Procedure_Optimize(&proc);

       // Compile
       UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);
       if (compiled == NULL) {
           UNIT_PrintError(&ctx, stderr);
           UNIT_Procedure_Clear(&proc);
           UNIT_Context_Clear(&ctx);
           return 1;
       }

       // JIT and call
       UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);
       if (buf == NULL) {
           UNIT_PrintError(&ctx, stderr);
           UNIT_CompiledProcedure_Free(compiled);
           UNIT_Procedure_Clear(&proc);
           UNIT_Context_Clear(&ctx);
           return 1;
       }

       int64_t (*add)(int64_t, int64_t) =
           (int64_t (*)(int64_t, int64_t))UNIT_ExecutableBuffer_GetPointer(buf);

       printf("%ld\n", add(3, 4)); // 7
       printf("%ld\n", add(10, 20)); // 30

       // Clean up
       UNIT_ExecutableBuffer_Free(buf);
       UNIT_CompiledProcedure_Free(compiled);
       UNIT_Procedure_Clear(&proc);
       UNIT_Context_Clear(&ctx);
       return 0;
   }

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ gcc first.c -lunit -o first
   $ ./first
   7
   30


Next steps
----------

Now that you know the basics, try these:

- Add more instructions. Use :c:enumerator:`UNIT_OP_MULTIPLY` and
  :c:enumerator:`UNIT_OP_SUBTRACT` to build more complex expressions.
- Add control flow. Use :c:func:`UNIT_Procedure_CreateJumpLabel`,
  :c:func:`UNIT_Procedure_UseLabel`, and :c:func:`UNIT_Procedure_AddJump`
  to implement conditionals and loops.
- Call external functions. Use :c:func:`UNIT_Procedure_AddCallName` to
  call ``printf``, ``malloc``, or any C function.
- Use local variables. Use :c:func:`UNIT_Procedure_CreateLocal` and
  :c:enumerator:`UNIT_OP_STORE_LOCAL` and :c:enumerator:`UNIT_OP_LOAD_LOCAL`
  to store and read variables.
