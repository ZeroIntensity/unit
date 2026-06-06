Instructions
============


.. c:enum:: UNIT_Instruction

   Enumerated type containing the ID for all UNIT stack-based instructions.

.. list-table:: Instructions
    :header-rows: 1
    :widths: 25 35 40

    * - Operation code
      - Effect
      - Description
    * - .. c:enumerator:: UNIT_OP_LOAD_STRING
      - ``stack.push(strings[oparg])``
      - Push a constant string onto the stack.
    * - .. c:enumerator:: UNIT_OP_LOAD_INTEGER
      - ``stack.push(oparg)``
      - Push a constant 32-bit integer onto the stack.
    * - .. c:enumerator:: UNIT_OP_ADD
      - ``stack.push(stack.pop(top - 1) + stack.pop(top))``
      - Add two numbers together.
    * - .. c:enumerator:: UNIT_OP_SUBTRACT
      - ``stack.push(stack.pop(top - 1) - stack.pop(top))``
      - Subtract two numbers.
    * - .. c:enumerator:: UNIT_OP_MULTIPLY
      - ``stack.push(stack.pop(top - 1) * stack.pop(top))``
      - Multiply two numbers together.
    * - .. c:enumerator:: UNIT_OP_DIVIDE
      - ``stack.push(stack.pop(top - 1) / stack.pop(top))``
      - Divide two numbers.
    * - .. c:enumerator:: UNIT_OP_MODULO
      - ``stack.push(stack.pop(top - 1) % stack.pop(top))``
      - Take the remainder two numbers.
    * - .. c:enumerator:: UNIT_OP_RETURN_VALUE
      - ``return stack.pop(top)``
      - Return a value and end execution of the current procedure.
    * - .. c:enumerator:: UNIT_OP_EXIT
      - ``exit(stack.pop(top))``
      - Exit the entire program with a given status code.
    * - .. c:enumerator:: UNIT_OP_POP_TOP
      - ``stack.pop(top)``
      - Throw away the value on the top of the stack.
    * - .. c:enumerator:: UNIT_OP_PREPARE_CALL
      - ``for i in range(oparg, 0, -1): stack.push(stack.pop(top - i))``
      - Push function arguments to the stack in left-to-right order.
    * - .. c:enumerator:: UNIT_OP_CALL_NAME
      - ``names[oparg](stack.pop())``
      - Call a function.
    * - .. c:enumerator:: UNIT_OP_STORE_LOCAL
      - ``locals[oparg] = stack.pop(top)``
      - Store a local variable.
    * - .. c:enumerator:: UNIT_OP_LOAD_LOCAL
      - ``stack.push(locals[oparg])``
      - Read the value of a local variable.
    * - .. c:enumerator:: UNIT_OP_ADDRESS_OF
      - ``stack.push(&locals[oparg])``
      - Take the memory address of a local variable.
    * - .. c:enumerator:: UNIT_OP_COMPARE_EQUAL
      - ``stack.push(stack.pop(top - 1) == stack.pop(top))``
      - Compare equal between two values.
    * - .. c:enumerator:: UNIT_OP_COMPARE_NOT_EQUAL
      - ``stack.push(stack.pop(top - 1) != stack.pop(top))``
      - Compare inequality between two values.
    * - .. c:enumerator:: UNIT_OP_COMPARE_GREATER
      - ``stack.push(stack.pop(top - 1) > stack.pop(top))``
      - Compare whether a value is greater than another.
    * - .. c:enumerator:: UNIT_OP_COMPARE_GREATER_EQUAL
      - ``stack.push(stack.pop(top - 1) >= stack.pop(top))``
      - Compare whether a value is greater than or equal to another.
    * - .. c:enumerator:: UNIT_OP_COMPARE_LESS
      - ``stack.push(stack.pop(top - 1) < stack.pop(top))``
      - Compare whether a value is less than another.
    * - .. c:enumerator:: UNIT_OP_COMPARE_LESS_EQUAL
      - ``stack.push(stack.pop(top - 1) <= stack.pop(top))``
      - Compare whether a value is less than or equal to another.
    * - .. c:enumerator:: UNIT_OP_JUMP_TO
      - ``goto jump_labels[oparg]``
      - Jump to a label.
    * - .. c:enumerator:: UNIT_OP_JUMP_IF_TRUE
      - ``if stack.pop(top): goto jump_labels[oparg]``
      - Jump to a label if the given value is true.
    * - .. c:enumerator:: UNIT_OP_JUMP_IF_FALSE
      - ``if not stack.pop(top): goto jump_labels[oparg]``
      - Jump to a label if the given value is false.
