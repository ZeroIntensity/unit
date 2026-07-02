.. _c-opcodes:

Operation codes
===============

.. c:enum:: UNIT_OperationCode

   Enumerated type containing the ID for all UNIT stack-based instructions.


Constants
---------

.. c:enumerator:: UNIT_OP_LOAD_INTEGER

   Push a constant integer onto the stack.

   **Stack effect:** ``-- value``

   The operand is the integer value to push.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // Push 42 onto the stack
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);

.. c:enumerator:: UNIT_OP_LOAD_STRING

   Push a constant string onto the stack. Emitted by
   :c:func:`UNIT_Procedure_AddStringLoad` rather than
   :c:func:`UNIT_Procedure_AddOperation`.

   **Stack effect:** ``-- pointer``

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddStringLoad(&proc, "hello world");


Arithmetic
----------

All arithmetic instructions pop two values and push the result.
The first value pushed is the left operand.

.. c:enumerator:: UNIT_OP_ADD

   **Stack effect:** ``a b -- a+b``

   .. code-block:: c

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 10);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 20);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);
      // stack: [30]

.. c:enumerator:: UNIT_OP_SUBTRACT

   **Stack effect:** ``a b -- a-b``

.. c:enumerator:: UNIT_OP_MULTIPLY

   **Stack effect:** ``a b -- a*b``

.. c:enumerator:: UNIT_OP_DIVIDE

   Integer division, truncated toward zero.

   **Stack effect:** ``a b -- a/b``

.. c:enumerator:: UNIT_OP_MODULO

   Integer remainder.

   **Stack effect:** ``a b -- a%b``


Local Variables
---------------

Locals are created with :c:func:`UNIT_Procedure_CreateLocal` and accessed
by index. Named access is also available via :c:func:`UNIT_Procedure_AddStoreName`
and :c:func:`UNIT_Procedure_AddLoadName`.

.. c:enumerator:: UNIT_OP_STORE_LOCAL

   Pop the top of the stack into a local variable.

   **Stack effect:** ``value --``

   The operand is the local variable index.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_STORE_LOCAL, 0);

.. c:enumerator:: UNIT_OP_LOAD_LOCAL

   Push the value of a local variable onto the stack.

   **Stack effect:** ``-- value``

   The operand is the local variable index.

   .. code-block:: c

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 0);

.. c:enumerator:: UNIT_OP_ADDRESS_OF

   Push the memory address of a local variable onto the stack.
   This is used for pointer operations with :c:enumerator:`UNIT_OP_READ_BYTES`
   and :c:enumerator:`UNIT_OP_WRITE_BYTES`.

   **Stack effect:** ``-- address``

   The operand is the local variable index.


Memory Access
-------------

These instructions read and write raw memory at arbitrary addresses.
The address is typically obtained from :c:enumerator:`UNIT_OP_ADDRESS_OF`
or from a call to an external function like ``malloc``.

.. c:enumerator:: UNIT_OP_READ_BYTES

   Pop an address, read the specified number of bytes from it, and push
   the value. Valid sizes are 1, 2, 4, and 8.

   **Stack effect:** ``address -- value``

   The operand is the number of bytes to read.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // Read a single byte from the pointer on the stack
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_READ_BYTES, 1);

.. c:enumerator:: UNIT_OP_WRITE_BYTES

   Pop an address and a value, write the specified number of bytes to the
   address. Valid sizes are 1, 2, 4, and 8.

   **Stack effect:** ``address value --``

   The operand is the number of bytes to write.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // Write a single byte to the pointer on the stack
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_WRITE_BYTES, 1);


Comparisons
-----------

All comparison instructions pop two values and push a comparison result.
The result is consumed by :c:enumerator:`UNIT_OP_JUMP_IF_TRUE` or
:c:enumerator:`UNIT_OP_JUMP_IF_FALSE`. The first value pushed is the left
operand.

.. c:enumerator:: UNIT_OP_COMPARE_EQUAL

   **Stack effect:** ``a b -- result``

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 5);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 5);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_COMPARE_EQUAL, 0);
      // result: true (5 == 5)

.. c:enumerator:: UNIT_OP_COMPARE_NOT_EQUAL

   **Stack effect:** ``a b -- result``

.. c:enumerator:: UNIT_OP_COMPARE_LESS

   **Stack effect:** ``a b -- result``

.. c:enumerator:: UNIT_OP_COMPARE_LESS_EQUAL

   **Stack effect:** ``a b -- result``

.. c:enumerator:: UNIT_OP_COMPARE_GREATER

   **Stack effect:** ``a b -- result``

.. c:enumerator:: UNIT_OP_COMPARE_GREATER_EQUAL

   **Stack effect:** ``a b -- result``


Control Flow
------------

.. c:enumerator:: UNIT_OP_JUMP

   Unconditionally jump to a label. Emitted by :c:func:`UNIT_Procedure_AddJump`.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_JumpLabel *end = UNIT_Procedure_CreateJumpLabel(&proc, "end");
      UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP, end);
      // ...
      UNIT_Procedure_UseLabel(&proc, end);

.. c:enumerator:: UNIT_OP_JUMP_IF_TRUE

   Pop a comparison result. Jump to the label if the result is true.
   Emitted by :c:func:`UNIT_Procedure_AddJump`.

   **Stack effect:** ``comparison --``

.. c:enumerator:: UNIT_OP_JUMP_IF_FALSE

   Pop a comparison result. Jump to the label if the result is false.
   Emitted by :c:func:`UNIT_Procedure_AddJump`.

   **Stack effect:** ``comparison --``

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_JumpLabel *else_branch = UNIT_Procedure_CreateJumpLabel(&proc, "else");

      // if (a == b) ...
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 0);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_LOCAL, 1);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_COMPARE_EQUAL, 0);
      UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP_IF_FALSE, else_branch);

      // then branch ...

      UNIT_Procedure_UseLabel(&proc, else_branch);
      // else branch ...


Function Calls
--------------

.. c:enumerator:: UNIT_OP_PREPARE_CALL

   Collect arguments from the stack for a function call. This instruction
   is emitted automatically by :c:func:`UNIT_Procedure_AddCallName` and
   :c:func:`UNIT_Procedure_AddCallProcedure`. You should not emit it
   manually.

   The operand is the number of arguments.

.. c:enumerator:: UNIT_OP_CALL_NAME

   Call a named external function and push the return value. Emitted by
   :c:func:`UNIT_Procedure_AddCallName`.

   **Stack effect:** ``arg1 arg2 ... argN -- result``

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // printf("%d\n", 42)
      UNIT_Procedure_AddStringLoad(&proc, "%d\n");
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
      UNIT_Procedure_AddCallName(&proc, "printf", 2);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_POP, 0); // discard return

.. c:enumerator:: UNIT_OP_CALL_PROCEDURE

   Call another UNIT procedure. Emitted by
   :c:func:`UNIT_Procedure_AddCallProcedure`. This enables inlining
   during optimization.

   **Stack effect:** ``arg1 arg2 ... argN -- result``

.. c:enumerator:: UNIT_OP_LOAD_ARGUMENT

   Push a function argument onto the stack. Argument 0 is the first
   parameter passed to the procedure.

   **Stack effect:** ``-- value``

   The operand is the argument index.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // Load the first two arguments and add them
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 1);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);

.. c:enumerator:: UNIT_OP_RETURN_VALUE

   Pop the top of the stack and return it to the caller. Ends execution
   of the current procedure.

   **Stack effect:** ``value --``

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

.. c:enumerator:: UNIT_OP_EXIT

   Terminate the entire process with the value on top of the stack as
   the exit code.

   **Stack effect:** ``code --``


Stack Manipulation
------------------

.. c:enumerator:: UNIT_OP_POP

   Discard the top of the stack.

   **Stack effect:** ``value --``

.. c:enumerator:: UNIT_OP_COPY

   Duplicate a stack item at the given depth. ``COPY 0`` duplicates
   the top of the stack.

   **Stack effect:** ``-- value``

   The operand is the depth (0 = top, 1 = second item, etc).

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 10);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 20);
      // stack: [10, 20]
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_COPY, 1);
      // stack: [10, 20, 20]

.. c:enumerator:: UNIT_OP_SWAP

   Swap the top of the stack with the item at the given depth.

   **Stack effect:** unchanged (items rearranged)

   The operand is the depth (1 = second item, 2 = third item, etc).

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 10);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 20);
      // stack: [10, 20]
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_SWAP, 1);
      // stack: [20, 10]


Type conversion
---------------

.. c:enumerator:: UNIT_OP_CONVERT

   Convert the top of the stack to a different integer width. The operand
   is a :c:enum:`UNIT_IntegerType` value.

   **Stack effect:** ``value -- converted``

.. c:enum:: UNIT_IntegerType

   Integer types for :c:enumerator:`UNIT_OP_CONVERT`.

   .. c:enumerator:: UNIT_TYPE_INT8
   .. c:enumerator:: UNIT_TYPE_INT16
   .. c:enumerator:: UNIT_TYPE_INT32
   .. c:enumerator:: UNIT_TYPE_INT64
   .. c:enumerator:: UNIT_TYPE_UINT8
   .. c:enumerator:: UNIT_TYPE_UINT16
   .. c:enumerator:: UNIT_TYPE_UINT32
   .. c:enumerator:: UNIT_TYPE_UINT64
