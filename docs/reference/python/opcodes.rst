.. _python-opcodes:

Operation codes
===============

.. py:class:: unit.OpCode

   Enum of stack machine instructions. These correspond to the
   C API's ``UNIT_OP_*`` constants.

   Most users will not need this enum directly -- the methods on
   :py:class:`~unit.Procedure` are the preferred way to emit instructions.
   This enum is exposed for advanced use cases like building generic
   instruction emitters.

   .. code-block:: python
      :caption: :iconify:`akar-icons:python-fill` Example

      # Preferred: use Procedure methods
      proc.load_int64(42)
      proc.add()

      # Advanced: use OpCode enum directly
      proc._add_op_int(OpCode.LOAD_INT64, 42)

   **Constants**

   .. py:attribute:: LOAD_INT8

      Push a signed 8-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_INT8`.

   .. py:attribute:: LOAD_INT16

      Push a signed 16-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_INT16`.

   .. py:attribute:: LOAD_INT32

      Push a signed 32-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_INT32`.

   .. py:attribute:: LOAD_INT64

      Push a signed 64-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_INT64`.

   .. py:attribute:: LOAD_UINT8

      Push an unsigned 8-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_UINT8`.

   .. py:attribute:: LOAD_UINT16

      Push an unsigned 16-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_UINT16`.

   .. py:attribute:: LOAD_UINT32

      Push an unsigned 32-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_UINT32`.

   .. py:attribute:: LOAD_UINT64

      Push an unsigned 64-bit integer constant.
      See :c:enumerator:`UNIT_OP_LOAD_UINT64`.

   .. py:attribute:: LOAD_STRING

      Push a string constant. See :c:macro:`UNIT_OP_LOAD_STRING`.

   .. py:attribute:: LOAD_ARGUMENT

      Push a function argument. See :c:macro:`UNIT_OP_LOAD_ARGUMENT`.

   .. py:attribute:: LOAD_LOCAL

      Push a local variable. See :c:macro:`UNIT_OP_LOAD_LOCAL`.

   .. py:attribute:: STORE_LOCAL

      Pop into a local variable. See :c:macro:`UNIT_OP_STORE_LOCAL`.

   **Arithmetic**

   .. py:attribute:: ADD

      See :c:macro:`UNIT_OP_ADD`.

   .. py:attribute:: SUBTRACT

      See :c:macro:`UNIT_OP_SUBTRACT`.

   .. py:attribute:: MULTIPLY

      See :c:macro:`UNIT_OP_MULTIPLY`.

   .. py:attribute:: DIVIDE

      See :c:macro:`UNIT_OP_DIVIDE`.

   .. py:attribute:: MODULO

      See :c:macro:`UNIT_OP_MODULO`.

   **Comparisons**

   .. py:attribute:: COMPARE_EQUAL

      See :c:macro:`UNIT_OP_COMPARE_EQUAL`.

   .. py:attribute:: COMPARE_NOT_EQUAL

      See :c:macro:`UNIT_OP_COMPARE_NOT_EQUAL`.

   .. py:attribute:: COMPARE_LESS

      See :c:macro:`UNIT_OP_COMPARE_LESS`.

   .. py:attribute:: COMPARE_LESS_EQUAL

      See :c:macro:`UNIT_OP_COMPARE_LESS_EQUAL`.

   .. py:attribute:: COMPARE_GREATER

      See :c:macro:`UNIT_OP_COMPARE_GREATER`.

   .. py:attribute:: COMPARE_GREATER_EQUAL

      See :c:macro:`UNIT_OP_COMPARE_GREATER_EQUAL`.

   **Control Flow**

   .. py:attribute:: JUMP

      Unconditional jump. See :c:macro:`UNIT_OP_JUMP_TO`.

   .. py:attribute:: JUMP_IF_TRUE

      Jump if comparison is true. See :c:macro:`UNIT_OP_JUMP_IF_TRUE`.

   .. py:attribute:: JUMP_IF_FALSE

      Jump if comparison is false. See :c:macro:`UNIT_OP_JUMP_IF_FALSE`.

   **Calls**

   .. py:attribute:: PREPARE_CALL

      Internal: prepare a call frame. See :c:macro:`UNIT_OP_PREPARE_CALL`.

   .. py:attribute:: CALL_NAME

      Call an external function by name. See :c:macro:`UNIT_OP_CALL_NAME`.

   .. py:attribute:: CALL_PROCEDURE

      Call a UNIT procedure. See :c:macro:`UNIT_OP_CALL_PROCEDURE`.

   .. py:attribute:: RETURN_VALUE

      Return from procedure. See :c:macro:`UNIT_OP_RETURN_VALUE`.

   .. py:attribute:: EXIT

      Terminate the process. See :c:macro:`UNIT_OP_EXIT`.

   **Stack Operations**

   .. py:attribute:: COPY

      Duplicate a stack item. See :c:macro:`UNIT_OP_COPY`.

   .. py:attribute:: SWAP

      Swap stack items. See :c:macro:`UNIT_OP_SWAP`.

   .. py:attribute:: POP

      Discard top of stack. See :c:macro:`UNIT_OP_POP`.

   **Memory**

   .. py:attribute:: READ_BYTES

      Read from memory. See :c:macro:`UNIT_OP_READ_BYTES`.

   .. py:attribute:: WRITE_BYTES

      Write to memory. See :c:macro:`UNIT_OP_WRITE_BYTES`.

   .. py:attribute:: ADDRESS_OF

      Push address of a local. See :c:macro:`UNIT_OP_ADDRESS_OF`.

   **Type Conversion**

   .. py:attribute:: CONVERT

      Convert integer width. See :c:macro:`UNIT_OP_CONVERT`.
