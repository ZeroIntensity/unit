.. _c-procedures:

Procedures
==========

.. c:struct:: UNIT_Procedure

   A container of instructions representing a function that will eventually
   be compiled by UNIT.

   .. c:var:: UNIT_Context *context

      The context being used by this procedure.

   .. c:var:: const char *name

      The name of the procedure.


Lifecycle
---------

.. c:function:: UNIT_Status UNIT_Procedure_Init(UNIT_Procedure *procedure, UNIT_Context *context, const char *name)

   Initialize a procedure. On success, :c:func:`UNIT_Procedure_Clear` must be
   called later to free memory allocated by this function.

   :param procedure: A pointer to a procedure. Memory at this location will
                     be overwritten.
   :param context: The context that will be used when interacting with the procedure.
                   This must be valid for the lifetime of the procedure.
   :param name: A string indicating the name of the procedure. This string is
                copied internally.
   :return: Indicator whether the call was successful.
            See :c:macro:`UNIT_FAILED`.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context context;
      UNIT_Context_Init(&context);

      UNIT_Procedure procedure;
      if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
          UNIT_PrintError(&context, stderr);
          UNIT_Context_Clear(&context);
          return -1;
      }

      // Must call UNIT_Procedure_Clear() later.


.. c:function:: UNIT_Procedure *UNIT_Procedure_New(UNIT_Context *context, const char *name)

   Create a new heap-allocated procedure. On success, :c:func:`UNIT_Procedure_Free`
   must be called later.

   :param context: The context that will be used when interacting with the procedure.
   :param name: The name of the procedure. Copied internally.
   :return: A heap-allocated procedure, or ``NULL`` on failure.


.. c:function:: void UNIT_Procedure_Clear(UNIT_Procedure *procedure)

   Free memory allocated by :c:func:`UNIT_Procedure_Init`.

   :param procedure: The procedure to clear.


.. c:function:: void UNIT_Procedure_Free(UNIT_Procedure *procedure)

   Free memory allocated by :c:func:`UNIT_Procedure_New`. Safe to call
   with ``NULL``.

   :param procedure: The procedure to free, or ``NULL``.


Emitting instructions
---------------------

.. c:function:: UNIT_Status UNIT_Procedure_AddOperation(UNIT_Procedure *procedure, UNIT_OperationCode instruction, int64_t argument)

   Add a stack instruction to the procedure. This is the general-purpose
   function for emitting most instructions. Some instructions require
   specialized functions instead (see below).

   :param procedure: The procedure to add the instruction to.
   :param instruction: The opcode. See :ref:`c-opcodes`.
   :param argument: The operand. Meaning depends on the instruction.
   :return: Indicator whether the call was successful.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // return 42
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);


.. c:function:: UNIT_Status UNIT_Procedure_AddStringLoad(UNIT_Procedure *procedure, const char *str)

   Push a string constant onto the stack. The string is copied internally.

   :param procedure: The procedure.
   :param str: The string to push.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddStringLoad(&proc, "hello %s\n");


.. c:function:: UNIT_Status UNIT_Procedure_AddCallName(UNIT_Procedure *procedure, const char *name, UNIT_Size num_arguments)

   Call an external function by name. The top *num_arguments* stack items
   are passed as arguments (first pushed = first argument). The return value
   is pushed onto the stack.

   This emits both :c:enumerator:`UNIT_OP_PREPARE_CALL` and
   :c:enumerator:`UNIT_OP_CALL_NAME` internally.

   :param procedure: The procedure.
   :param name: The function name. Resolved via ``dlsym`` at link time or
                JIT time. See :c:func:`UNIT_SymbolMap_RegisterSymbol` for
                custom resolution.
   :param num_arguments: The number of arguments to pass.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      // printf("%d\n", 42)
      UNIT_Procedure_AddStringLoad(&proc, "%d\n");
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
      UNIT_Procedure_AddCallName(&proc, "printf", 2);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_POP, 0);


.. c:function:: UNIT_Status UNIT_Procedure_AddCallProcedure(UNIT_Procedure *self, UNIT_Procedure *target, uint8_t nargs)

   Call another UNIT procedure. The subprocedure will be translated and compiled
   during compilation of this procedure.

   :param self: The calling procedure.
   :param target: The procedure to call.
   :param nargs: The number of arguments to pass.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 5);
      UNIT_Procedure_AddCallProcedure(&main_proc, &factorial_proc, 1);


Local Variables
---------------

.. c:function:: UNIT_Status UNIT_Procedure_CreateLocal(UNIT_Procedure *procedure, const char *name, UNIT_Local *local_ptr)

   Create a named local variable. The index is assigned automatically.
   The returned :c:type:`UNIT_Local` handle can be used with
   :c:func:`UNIT_Procedure_AddStoreName` and :c:func:`UNIT_Procedure_AddLoadName`.

   :param procedure: The procedure.
   :param name: A descriptive name for the variable. Copied internally.
   :param local_ptr: Output parameter receiving the local handle.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Local x;
      UNIT_Procedure_CreateLocal(&proc, "x", &x);

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 42);
      UNIT_Procedure_AddStoreName(&proc, x);
      UNIT_Procedure_AddLoadName(&proc, x);


.. c:struct:: UNIT_Local

   A handle to a local variable.

   .. c:var:: int32_t id

      The local variable index.


.. c:function:: UNIT_Status UNIT_Procedure_AddStoreName(UNIT_Procedure *procedure, UNIT_Local local)

   Pop the top of the stack into a local variable by handle. This is
   equivalent to :c:enumerator:`UNIT_OP_STORE_LOCAL` but uses the handle
   from :c:func:`UNIT_Procedure_CreateLocal`.

   :param procedure: The procedure.
   :param local: The local variable handle.


.. c:function:: UNIT_Status UNIT_Procedure_AddLoadName(UNIT_Procedure *procedure, UNIT_Local local)

   Push a local variable onto the stack by handle.

   :param procedure: The procedure.
   :param local: The local variable handle.


Jump Labels
-----------

.. c:struct:: UNIT_JumpLabel

   A jump target, created by :c:func:`UNIT_Procedure_CreateJumpLabel`.

   .. c:var:: const char *name

      The label name (heap-allocated, owned by the procedure).

   .. c:var:: int32_t id

      The label ID.


.. c:function:: UNIT_JumpLabel *UNIT_Procedure_CreateJumpLabel(UNIT_Procedure *procedure, const char *name)

   Create a jump target. The label must later be placed with
   :c:func:`UNIT_Procedure_UseLabel`.

   :param procedure: The procedure.
   :param name: A descriptive name for the label. Copied internally.
   :return: A pointer to the label, or ``NULL`` on failure. The label is
            owned by the procedure and must not be freed by the caller.


.. c:function:: UNIT_Status UNIT_Procedure_UseLabel(UNIT_Procedure *procedure, UNIT_JumpLabel *jump_label)

   Place a label at the current position in the instruction stream.
   All jumps to this label will target the next instruction emitted.

   :param procedure: The procedure.
   :param jump_label: The label to place.


.. c:function:: UNIT_Status UNIT_Procedure_AddJump(UNIT_Procedure *procedure, UNIT_OperationCode instruction, UNIT_JumpLabel *jump_label)

   Emit a jump instruction targeting the given label. The instruction must
   be one of :c:enumerator:`UNIT_OP_JUMP`, :c:enumerator:`UNIT_OP_JUMP_IF_TRUE`,
   or :c:enumerator:`UNIT_OP_JUMP_IF_FALSE`.

   :param procedure: The procedure.
   :param instruction: The jump opcode.
   :param jump_label: The target label.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_JumpLabel *end = UNIT_Procedure_CreateJumpLabel(&proc, "end");

      UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_INTEGER, 0);
      UNIT_Procedure_AddOperation(&proc, UNIT_OP_COMPARE_EQUAL, 0);
      UNIT_Procedure_AddJump(&proc, UNIT_OP_JUMP_IF_TRUE, end);

      // ... code skipped when condition is true ...

      UNIT_Procedure_UseLabel(&proc, end);
      // ... continues here ...


Flags
-----

.. c:function:: void UNIT_Procedure_SetFlags(UNIT_Procedure *procedure, UNIT_Flags flags)

   Set procedure flags. Flags can be combined with bitwise OR.

   :param procedure: The procedure.
   :param flags: Bitwise OR of flag constants.


.. c:function:: UNIT_Flags UNIT_Procedure_GetFlags(const UNIT_Procedure *procedure)

   Return the current procedure flags.

   :param procedure: The procedure.
   :return: The current flags.


.. c:macro:: UNIT_FLAG_NONE

   No flags. This is always ``0``.


.. c:macro:: UNIT_FLAG_NO_OPTIMIZE_TRANSLATION

   Skip register IR optimization (move coalescing, dead move elimination,
   forward copy propagation) during compilation.


.. c:macro:: UNIT_FLAG_FORCE_NO_INLINE

   Prevent this procedure from being inlined into callers, regardless
   of size.


.. c:macro:: UNIT_FLAG_FORCE_INLINE

   Always inline this procedure into callers, regardless of size.


.. c:macro:: UNIT_FLAG_PRINT_TRANSLATION_PREOP

   Print the register IR to stderr before optimization runs. Useful
   for debugging.


.. c:macro:: UNIT_FLAG_PRINT_TRANSLATION_POSTOP

   Print the register IR to stderr after optimization runs. Useful
   for debugging.


.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

   UNIT_Procedure_SetFlags(&proc, UNIT_FLAG_FORCE_NO_INLINE | UNIT_FLAG_NO_OPTIMIZE_TRANSLATION);


Optimization
------------

.. c:function:: UNIT_Status UNIT_Procedure_Optimize(UNIT_Procedure *procedure)

   Run stack IR optimization passes on the procedure. This includes:
   inlining, constant folding, dead store elimination, and local variable
   optimization. Call this before :c:func:`UNIT_Compile`.

   Multiple iterations are run automatically until no further changes occur.

   :param procedure: The procedure to optimize.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_Optimize(&proc);


Debugging
---------

.. c:function:: UNIT_Status UNIT_Procedure_PrintInstructions(const UNIT_Procedure *procedure, FILE *stream, int8_t visualize_stack_effect)

   Print the stack IR to a file stream.

   :param procedure: The procedure.
   :param stream: The output stream (e.g. ``stdout``).
   :param visualize_stack_effect: If nonzero, show the stack state after
      each instruction.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_PrintInstructions(&proc, stdout, 1);

.. c:function:: const char *UNIT_OperationCode_GetName(UNIT_OperationCode instruction)

   Return the name of an opcode as a string (e.g. ``"LOAD_INTEGER"``).

   :param instruction: The opcode.
   :return: A static string. Must not be freed.
