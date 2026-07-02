.. _bindings:

Language bindings
=================

UNIT is written in C, with bindings for C++ and Python. The bindings are
merely thin wrappers over the C library, so the same concepts and instruction
set apply in all three languages. This page shows the same program in each
language so you can see the mapping.

.. attention::

   None of the C examples on this page come with proper error handling.


Hello World
-----------

A function that returns 42.

C
^

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` C

   UNIT_Context ctx;
   UNIT_Context_Init(&ctx);

   UNIT_Procedure proc;
   UNIT_Procedure_Init(&proc, &ctx, "main");
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

   UNIT_Procedure_Optimize(&proc);
   UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);
   UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);

   int64_t (*fn)(void) = (int64_t (*)(void))UNIT_ExecutableBuffer_GetPointer(buf);
   printf("%ld\n", fn()); // 42

   UNIT_ExecutableBuffer_Free(buf);
   UNIT_CompiledProcedure_Free(compiled);
   UNIT_Procedure_Clear(&proc);
   UNIT_Context_Clear(&ctx);


C++
^^^

.. code-block:: cpp
   :caption: :iconify:`devicon-plain:cplusplus` Example

   unit::Context ctx;
   unit::Procedure proc(ctx, "main");
   proc.load_integer(42);
   proc.return_value();

   proc.optimize();
   auto compiled = proc.compile(unit::Platform::host());
   auto fn = compiled.jit<int64_t(*)()>();

   printf("%ld\n", fn()); // 42


Python
^^^^^^

.. code-block:: python
   :caption: :iconify:`akar-icons:python-fill` Example

   import unit

   proc = unit.Procedure("main")
   proc.load_integer(42)
   proc.return_value()

   proc.optimize()
   compiled = proc.compile()
   fn = compiled.jit()

   print(fn()) # 42


Add
---

A function that takes two arguments and returns their sum.


C
^

.. code-block:: c

   UNIT_Procedure proc;
   UNIT_Procedure_Init(&proc, &ctx, "add");

   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

   UNIT_Procedure_Optimize(&proc);
   UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);
   UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);

   int64_t (*add)(int64_t, int64_t) =
       (int64_t (*)(int64_t, int64_t))UNIT_ExecutableBuffer_GetPointer(buf);
   printf("%ld\n", add(3, 4)); // 7


C++
^^^

.. code-block:: cpp

   unit::Procedure proc(ctx, "add");
   proc.load_argument(0);
   proc.load_argument(1);
   proc.add();
   proc.return_value();

   proc.optimize();
   auto compiled = proc.compile(unit::Platform::host());
   auto add = compiled.jit<int64_t(*)(int64_t, int64_t)>();

   printf("%ld\n", add(3, 4)); // 7


Python
^^^^^^

.. code-block:: python

   proc = unit.Procedure("add")
   proc.load_argument(0)
   proc.load_argument(1)
   proc.add()
   proc.return_value()

   proc.optimize()
   compiled = proc.compile()
   add = compiled.jit()

   print(add(3, 4))  # 7


Conditional
-----------

Return 1 if the argument is positive, 0 otherwise.


C
^

.. code-block:: c

   UNIT_Procedure proc;
   UNIT_Procedure_Init(&proc, &ctx, "is_positive");

   UNIT_JumpLabel *nope = UNIT_Procedure_CreateJumpLabel(&proc, "nope");

   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_COMPARE_LESS_EQUAL, 0);
   UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP_IF_TRUE, nope);

   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

   UNIT_Procedure_UseLabel(&proc, nope);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);


C++
^^^

.. code-block:: cpp

   unit::Procedure proc(ctx, "is_positive");
   auto nope = proc.create_jump_label("nope");

   proc.load_argument(0);
   proc.load_integer(0);
   proc.compare_less_equal();
   proc.jump_if_true(nope);

   proc.load_integer(1);
   proc.return_value();

   proc.use_label(nope);
   proc.load_integer(0);
   proc.return_value();


Python
^^^^^^

.. code-block:: python

   proc = unit.Procedure("is_positive")
   nope = proc.create_jump_label("nope")

   proc.load_argument(0)
   proc.load_integer(0)
   proc.compare_less_equal()
   proc.jump_if_true(nope)

   proc.load_integer(1)
   proc.return_value()

   proc.use_label(nope)
   proc.load_integer(0)
   proc.return_value()


Loop
----

Sum the numbers from 1 to N.


C
^

.. code-block:: c

   UNIT_Procedure proc;
   UNIT_Procedure_Init(&proc, &ctx, "sum_to_n");
   UNIT_Local sum, i;
   UNIT_Procedure_CreateLocal(&proc, "sum", &sum);
   UNIT_Procedure_CreateLocal(&proc, "i", &i);

   UNIT_JumpLabel *loop = UNIT_Procedure_CreateJumpLabel(&proc, "loop");
   UNIT_JumpLabel *end = UNIT_Procedure_CreateJumpLabel(&proc, "end");

   // sum = 0; i = n;
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_STORE_LOCAL, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_STORE_LOCAL, 1);

   // loop:
   UNIT_Procedure_UseLabel(&proc, loop);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_COMPARE_EQUAL, 0);
   UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP_IF_TRUE, end);

   // sum += i; i -= 1;
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_STORE_LOCAL, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 1);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_SUBTRACT, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_STORE_LOCAL, 1);
   UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP, loop);

   // end: return sum;
   UNIT_Procedure_UseLabel(&proc, end);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 0);
   UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);


C++
^^^

.. code-block:: cpp

   unit::Procedure proc(ctx, "sum_to_n");
   proc.create_local("sum");
   proc.create_local("i");
   auto loop = proc.create_jump_label("loop");
   auto end = proc.create_jump_label("end");

   proc.load_integer(0);
   proc.store_local(0);
   proc.load_argument(0);
   proc.store_local(1);

   proc.use_label(loop);
   proc.load_local(1);
   proc.load_integer(0);
   proc.compare_equal();
   proc.jump_if_true(end);

   proc.load_local(0);
   proc.load_local(1);
   proc.add();
   proc.store_local(0);
   proc.load_local(1);
   proc.load_integer(1);
   proc.subtract();
   proc.store_local(1);
   proc.jump_to(loop);

   proc.use_label(end);
   proc.load_local(0);
   proc.return_value();


Python
^^^^^^

.. code-block:: python

   proc = unit.Procedure("sum_to_n")
   proc.create_local("sum")
   proc.create_local("i")
   loop = proc.create_jump_label("loop")
   end = proc.create_jump_label("end")

   proc.load_integer(0)
   proc.store_local(0)
   proc.load_argument(0)
   proc.store_local(1)

   proc.use_label(loop)
   proc.load_local(1)
   proc.load_integer(0)
   proc.compare_equal()
   proc.jump_if_true(end)

   proc.load_local(0)
   proc.load_local(1)
   proc.add()
   proc.store_local(0)
   proc.load_local(1)
   proc.load_integer(1)
   proc.subtract()
   proc.store_local(1)
   proc.jump(loop)

   proc.use_label(end)
   proc.load_local(0)
   proc.return_value()


Key Differences
---------------

The instruction set and compilation pipeline are identical across all
three languages. The differences are purely syntactic:

1. C uses :c:func:`UNIT_Procedure_AddOperation` with opcode constants.
   Cleanup is manual (``Clear``/``Free``). Error handling uses
   :c:macro:`UNIT_FAILED`.
2. C++ has a method per instruction (``proc.add()`` instead of
   ``UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0)``). Cleanup is
   automatic via RAII. Errors throw :cpp:class:`unit::error`.
3. Python mirrors the C++ API. Cleanup is automatic via garbage collection.
   Errors raise :py:class:`unit.Error` subclasses. The JIT return type is
   auto-detected from the arguments you pass, or you can use the
   :py:attr:`~unit.ExecutableBuffer.address` attribute with ``ctypes`` (or
   any FFI library of your preference) for full control.

For all API details, see the language-specific reference pages:
:ref:`C <c-reference>`, :ref:`C++ <cpp-reference>`, :ref:`Python <python-reference>`.
