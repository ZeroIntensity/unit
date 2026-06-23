Instructions
============

.. c:enum:: UNIT_Instruction

   Enumerated type containing the ID for all UNIT stack-based instructions.

   .. c:enumerator:: UNIT_OP_LOAD_STRING
   .. c:enumerator:: UNIT_OP_LOAD_INTEGER
   .. c:enumerator:: UNIT_OP_ADD
   .. c:enumerator:: UNIT_OP_SUBTRACT
   .. c:enumerator:: UNIT_OP_MULTIPLY
   .. c:enumerator:: UNIT_OP_DIVIDE
   .. c:enumerator:: UNIT_OP_MODULO
   .. c:enumerator:: UNIT_OP_RETURN_VALUE
   .. c:enumerator:: UNIT_OP_EXIT
   .. c:enumerator:: UNIT_OP_POP
   .. c:enumerator:: UNIT_OP_PREPARE_CALL
   .. c:enumerator:: UNIT_OP_CALL_NAME
   .. c:enumerator:: UNIT_OP_STORE_LOCAL
   .. c:enumerator:: UNIT_OP_LOAD_LOCAL
   .. c:enumerator:: UNIT_OP_ADDRESS_OF
   .. c:enumerator:: UNIT_OP_COMPARE_EQUAL
   .. c:enumerator:: UNIT_OP_COMPARE_NOT_EQUAL
   .. c:enumerator:: UNIT_OP_COMPARE_GREATER
   .. c:enumerator:: UNIT_OP_COMPARE_GREATER_EQUAL
   .. c:enumerator:: UNIT_OP_COMPARE_LESS
   .. c:enumerator:: UNIT_OP_COMPARE_LESS_EQUAL
   .. c:enumerator:: UNIT_OP_JUMP_TO
   .. c:enumerator:: UNIT_OP_JUMP_IF_TRUE
   .. c:enumerator:: UNIT_OP_JUMP_IF_FALSE
   .. c:enumerator:: UNIT_OP_COPY
   .. c:enumerator:: UNIT_OP_SWAP
   .. c:enumerator:: UNIT_OP_READ_BYTES
   .. c:enumerator:: UNIT_OP_WRITE_BYTES
   .. c:enumerator:: UNIT_OP_CAST
   .. c:enumerator:: UNIT_OP_PUSH_VIRTUAL


Constants
---------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_LOAD_INTEGER`
     - ``stack.push(oparg)``
     - Push a constant 32-bit integer onto the stack.
   * - :c:enumerator:`UNIT_OP_LOAD_STRING`
     - ``stack.push(strings[oparg])``
     - Push a constant string onto the stack.


Arithmetic
----------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_ADD`
     - ``stack.push(stack.pop(top - 1) + stack.pop(top))``
     - Add two numbers together.
   * - :c:enumerator:`UNIT_OP_SUBTRACT`
     - ``stack.push(stack.pop(top - 1) - stack.pop(top))``
     - Subtract two numbers.
   * - :c:enumerator:`UNIT_OP_MULTIPLY`
     - ``stack.push(stack.pop(top - 1) * stack.pop(top))``
     - Multiply two numbers together.
   * - :c:enumerator:`UNIT_OP_DIVIDE`
     - ``stack.push(stack.pop(top - 1) / stack.pop(top))``
     - Divide two numbers.
   * - :c:enumerator:`UNIT_OP_MODULO`
     - ``stack.push(stack.pop(top - 1) % stack.pop(top))``
     - Take the remainder of two numbers.


Local Variables
---------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_STORE_LOCAL`
     - ``locals[oparg] = stack.pop(top)``
     - Store a value into a local variable.
   * - :c:enumerator:`UNIT_OP_LOAD_LOCAL`
     - ``stack.push(locals[oparg])``
     - Read the value of a local variable.
   * - :c:enumerator:`UNIT_OP_ADDRESS_OF`
     - ``stack.push(&locals[oparg])``
     - Push the memory address of a local variable.


Memory Access
-------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_READ_BYTES`
     - ``stack.push(read(stack.pop(top), oparg))``
     - Read ``oparg`` bytes from a memory address.
   * - :c:enumerator:`UNIT_OP_WRITE_BYTES`
     - ``write(stack.pop(top - 1), stack.pop(top), oparg)``
     - Write ``oparg`` bytes to a memory address.


Comparisons
-----------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_COMPARE_EQUAL`
     - ``stack.push(stack.pop(top - 1) == stack.pop(top))``
     - Compare two values for equality.
   * - :c:enumerator:`UNIT_OP_COMPARE_NOT_EQUAL`
     - ``stack.push(stack.pop(top - 1) != stack.pop(top))``
     - Compare two values for inequality.
   * - :c:enumerator:`UNIT_OP_COMPARE_GREATER`
     - ``stack.push(stack.pop(top - 1) > stack.pop(top))``
     - Check whether a value is greater than another.
   * - :c:enumerator:`UNIT_OP_COMPARE_GREATER_EQUAL`
     - ``stack.push(stack.pop(top - 1) >= stack.pop(top))``
     - Check whether a value is greater than or equal to another.
   * - :c:enumerator:`UNIT_OP_COMPARE_LESS`
     - ``stack.push(stack.pop(top - 1) < stack.pop(top))``
     - Check whether a value is less than another.
   * - :c:enumerator:`UNIT_OP_COMPARE_LESS_EQUAL`
     - ``stack.push(stack.pop(top - 1) <= stack.pop(top))``
     - Check whether a value is less than or equal to another.


Control Flow
------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_JUMP_TO`
     - ``goto jump_labels[oparg]``
     - Unconditionally jump to a label.
   * - :c:enumerator:`UNIT_OP_JUMP_IF_TRUE`
     - ``if stack.pop(top): goto jump_labels[oparg]``
     - Jump to a label if the top of the stack is nonzero.
   * - :c:enumerator:`UNIT_OP_JUMP_IF_FALSE`
     - ``if not stack.pop(top): goto jump_labels[oparg]``
     - Jump to a label if the top of the stack is zero.


Function Calls
--------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_PREPARE_CALL`
     - Collects ``oparg`` items from the stack as call arguments.
     - Prepare arguments for a function call. Used internally by :c:func:`UNIT_Procedure_AddCall`.
   * - :c:enumerator:`UNIT_OP_CALL_NAME`
     - ``stack.push(names[oparg](args))``
     - Call a named function and push its return value.
   * - :c:enumerator:`UNIT_OP_PUSH_VIRTUAL`
     - ``stack.push(<parameter>)``
     - Declare a procedure parameter. Used at the start of a procedure body.


Stack Manipulation
------------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_POP`
     - ``stack.pop(top)``
     - Discard the value on top of the stack.
   * - :c:enumerator:`UNIT_OP_COPY`
     - ``stack.push(stack[top - oparg])``
     - Duplicate a stack item. With no argument, copies the top.
   * - :c:enumerator:`UNIT_OP_SWAP`
     - ``swap(stack[top], stack[top - oparg])``
     - Swap two stack items. With no argument, swaps the top two.


Type Conversion
---------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_CAST`
     - ``stack.push((type)stack.pop(top))``
     - Cast the top of the stack to the integer type specified by ``oparg``.


Program Control
---------------

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Opcode
     - Effect
     - Description
   * - :c:enumerator:`UNIT_OP_RETURN_VALUE`
     - ``return stack.pop(top)``
     - Return a value and end execution of the current procedure.
   * - :c:enumerator:`UNIT_OP_EXIT`
     - ``exit(stack.pop(top))``
     - Exit the entire program with the given status code.
