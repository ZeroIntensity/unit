.. _python-procedures:

Procedures
==========

.. py:class:: unit.Procedure(name, *, context=None, inlining=None, optimize_translation=True)

   A procedure represents a single function to be compiled. Provides
   methods to emit stack-based IR instructions.

   :param str name: Symbol name for the compiled function.
   :param Context context: The context that owns this procedure's memory.
      If ``None``, uses the current context or creates a new one (via
      :py:meth:`unit.Context.current_or_new`).
   :param str inlining: Inlining behavior -- ``"force"``, ``"never"``, or
      ``None`` (default). See :c:macro:`UNIT_FLAG_FORCE_INLINE`
      and :c:macro:`UNIT_FLAG_FORCE_NO_INLINE`.
   :param bool optimize_translation: Whether to run register IR optimization
      during compilation. See :c:macro:`UNIT_FLAG_NO_OPTIMIZE_TRANSLATION`.

   .. code-block:: python
      :caption: :iconify:`akar-icons:python-fill` Example

      # Simple usage
      proc = unit.Procedure("main")
      proc.load_integer(42)
      proc.return_value()

      # With explicit options
      proc = unit.Procedure(
          "helper",
          context=ctx,
          inlining="never",
          optimize_translation=False,
      )

   **Properties**

   .. py:property:: inlining
      :type: str | None

      Get or set the inlining behavior. ``"force"``, ``"never"``, or ``None``.

   .. py:property:: optimize_translation
      :type: bool

      Get or set whether register IR optimization runs during compilation.

   **Compilation**

   .. py:method:: optimize() -> None

      Run stack IR optimization passes: inlining, constant folding,
      dead store elimination, and local variable optimization.
      Call this before :py:meth:`compile`.

      :raises unit.Error: If optimization fails.

   .. py:method:: compile(platform=None) -> CompiledProcedure

      Compile the procedure to machine code.

      :param Platform platform: Target platform. Defaults to
         :py:meth:`Platform.host()`.
      :returns: A compiled procedure ready for JIT or object file output.
      :rtype: CompiledProcedure
      :raises unit.Error: If compilation fails.

   .. py:method:: instructions_text(*, visualize_stack_effect=True, ignore_errors=True) -> str

      Return the stack IR as a string. Useful for debugging.

      :param bool visualize_stack_effect: Show stack state after each instruction.
      :param bool ignore_errors: Suppress errors from incomplete procedures.

   **Stack Operations**

   .. py:method:: load_integer(value) -> None

      Push an integer constant onto the stack.
      See :c:macro:`UNIT_OP_LOAD_INTEGER`.

      **Stack effect:** ``-- value``

      :param int value: The integer to push.
      :raises TypeError: If *value* is not an int.

   .. py:method:: load_string(value) -> None

      Push a string constant onto the stack.
      See :c:func:`UNIT_Procedure_AddStringLoad`.

      **Stack effect:** ``-- pointer``

      :param str value: The string to push. Copied internally.
      :raises TypeError: If *value* is not a str.

   .. py:method:: load_argument(arg_number) -> None

      Push a function argument onto the stack. Argument 0 is the first
      parameter.
      See :c:macro:`UNIT_OP_LOAD_ARGUMENT`.

      **Stack effect:** ``-- value``

      :param int arg_number: The argument index.

   .. py:method:: load_local(id) -> None

      Push a local variable onto the stack.
      See :c:macro:`UNIT_OP_LOAD_LOCAL`.

      **Stack effect:** ``-- value``

      :param int id: The local variable index.

   .. py:method:: store_local(id) -> None

      Pop the top of the stack into a local variable.
      See :c:macro:`UNIT_OP_STORE_LOCAL`.

      **Stack effect:** ``value --``

      :param int id: The local variable index.

   .. py:method:: pop() -> None

      Discard the top of the stack.
      See :c:macro:`UNIT_OP_POP`.

      **Stack effect:** ``value --``

   .. py:method:: copy(offset_from_top) -> None

      Duplicate the stack item at the given depth. ``copy(0)`` duplicates
      the top.
      See :c:macro:`UNIT_OP_COPY`.

      **Stack effect:** ``-- value``

      :param int offset_from_top: Depth of the item to copy (0 = top).

      .. code-block:: python

         proc.load_integer(10)   # stack: [10]
         proc.load_integer(20)   # stack: [10, 20]
         proc.copy(1)            # stack: [10, 20, 10]

   .. py:method:: swap(offset_from_top) -> None

      Swap the top of the stack with the item at the given depth.
      See :c:macro:`UNIT_OP_SWAP`.

      **Stack effect:** unchanged (items rearranged)

      :param int offset_from_top: Depth of the item to swap with (1 = second item).

   **Arithmetic**

   .. py:method:: add() -> None

      Pop two values, push their sum.
      See :c:macro:`UNIT_OP_ADD`.

      **Stack effect:** ``a b -- a+b``

   .. py:method:: subtract() -> None

      Pop two values, push their difference.
      See :c:macro:`UNIT_OP_SUBTRACT`.

      **Stack effect:** ``a b -- a-b``

   .. py:method:: multiply() -> None

      Pop two values, push their product.
      See :c:macro:`UNIT_OP_MULTIPLY`.

      **Stack effect:** ``a b -- a*b``

   .. py:method:: divide() -> None

      Pop two values, push their quotient (integer division).
      See :c:macro:`UNIT_OP_DIVIDE`.

      **Stack effect:** ``a b -- a/b``

   .. py:method:: modulo() -> None

      Pop two values, push the remainder.
      See :c:macro:`UNIT_OP_MODULO`.

      **Stack effect:** ``a b -- a%b``

   **Comparisons**

   All comparisons pop two values and push a comparison result consumed
   by :py:meth:`jump_if_true` or :py:meth:`jump_if_false`.

   .. py:method:: compare_equal() -> None

      See :c:macro:`UNIT_OP_COMPARE_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. py:method:: compare_not_equal() -> None

      See :c:macro:`UNIT_OP_COMPARE_NOT_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. py:method:: compare_less() -> None

      See :c:macro:`UNIT_OP_COMPARE_LESS`.

      **Stack effect:** ``a b -- result``

   .. py:method:: compare_less_equal() -> None

      See :c:macro:`UNIT_OP_COMPARE_LESS_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. py:method:: compare_greater() -> None

      See :c:macro:`UNIT_OP_COMPARE_GREATER`.

      **Stack effect:** ``a b -- result``

   .. py:method:: compare_greater_equal() -> None

      See :c:macro:`UNIT_OP_COMPARE_GREATER_EQUAL`.

      **Stack effect:** ``a b -- result``

   **Control Flow**

   .. py:method:: create_jump_label(name) -> JumpLabel

      Create a jump target. Place it later with :py:meth:`use_label`.

      :param str name: A descriptive name for the label.
      :returns: A label handle.
      :rtype: JumpLabel
      :raises TypeError: If *name* is not a str.

   .. py:method:: use_label(label) -> None

      Place a label at the current position. All jumps to this label
      target the next instruction emitted.

      :param JumpLabel label: The label to place.

   .. py:method:: jump(label) -> None

      Unconditional jump.
      See :c:macro:`UNIT_OP_JUMP_TO`.

      :param JumpLabel label: The target label.

   .. py:method:: jump_if_true(label) -> None

      Pop a comparison result. Jump if true.
      See :c:macro:`UNIT_OP_JUMP_IF_TRUE`.

      **Stack effect:** ``comparison --``

      :param JumpLabel label: The target label.

   .. py:method:: jump_if_false(label) -> None

      Pop a comparison result. Jump if false.
      See :c:macro:`UNIT_OP_JUMP_IF_FALSE`.

      **Stack effect:** ``comparison --``

      :param JumpLabel label: The target label.

   .. code-block:: python

      end = proc.create_jump_label("end")
      proc.load_integer(5)
      proc.load_integer(5)
      proc.compare_equal()
      proc.jump_if_true(end)
      # ... not-equal path ...
      proc.use_label(end)
      # ... equal path continues here ...

   **Calls**

   .. py:method:: call_name(name, num_args) -> None

      Call an external function by name. The top *num_args* stack items
      are passed as arguments (first argument pushed first). The return
      value is pushed onto the stack.
      See :c:func:`UNIT_Procedure_AddCallName`.

      **Stack effect:** ``arg1 arg2 ... argN -- result``

      :param str name: The function name (resolved via ``dlsym`` or
         :py:class:`SymbolMap` during JIT).
      :param int num_args: Number of arguments.
      :raises TypeError: If *name* is not a str or *num_args* is not an int.
      :raises ValueError: If *num_args* is negative.

      .. code-block:: python

         proc.load_string("%d\n")
         proc.load_integer(42)
         proc.call_name("printf", 2)
         proc.pop()  # discard printf return value

   .. py:method:: return_value() -> None

      Pop the top of the stack and return it to the caller.
      See :c:macro:`UNIT_OP_RETURN_VALUE`.

      **Stack effect:** ``value --``

   .. py:method:: exit() -> None

      Terminate the process immediately.
      See :c:macro:`UNIT_OP_EXIT`.

   **Memory Access**

   .. py:method:: read_bytes(num_bytes) -> None

      Pop an address, read *num_bytes* from it (1, 2, 4, or 8),
      push the value.
      See :c:macro:`UNIT_OP_READ_BYTES`.

      **Stack effect:** ``address -- value``

      :param int num_bytes: Number of bytes to read.

   .. py:method:: write_bytes(num_bytes) -> None

      Pop an address and a value, write *num_bytes* of the value to
      the address.
      See :c:macro:`UNIT_OP_WRITE_BYTES`.

      **Stack effect:** ``address value --``

      :param int num_bytes: Number of bytes to write.

   .. py:method:: address_of(id) -> None

      Push the memory address of a local variable.
      See :c:macro:`UNIT_OP_ADDRESS_OF`.

      **Stack effect:** ``-- address``

      :param int id: The local variable index.


.. _python-jumplabels:

Jump labels
-----------

.. py:class:: unit.JumpLabel

   A handle to a jump target, returned by
   :py:meth:`Procedure.create_jump_label`. Not constructed directly.


.. _python-platform:

Platforms
---------

.. py:class:: unit.Platform(architecture, abi)

   Target platform for compilation.

   :param str architecture: ``"amd64"`` or ``"aarch64"``.
   :param str abi: ``"systemv"``, ``"apple"``, or ``"win64"``.

   .. code-block:: python
      :caption: :iconify:`akar-icons:python-fill` Example


      # Explicit platform
      platform = unit.Platform("amd64", "systemv")
      compiled = proc.compile(platform)

      # Host platform (auto-detected)
      compiled = proc.compile()  # uses Platform.host() internally

   .. py:classmethod:: host() -> Platform

      Return the platform of the current machine.

   .. py:attribute:: architecture
      :type: str

      ``"amd64"`` or ``"aarch64"``.

   .. py:attribute:: abi
      :type: str

      ``"systemv"``, ``"apple"``, or ``"win64"``.


.. _python-compiled-procedures:

Compiled procedures
-------------------

.. py:class:: unit.CompiledProcedure

   A compiled procedure, ready for JIT execution or object file output.
   Returned by :py:meth:`Procedure.compile`. Not constructed directly.

   .. py:method:: jit(extra_symbols=None) -> ExecutableBuffer

      JIT compile and return an executable buffer. Symbols are resolved
      via ``dlsym``. Custom symbols can be provided for JIT-to-JIT calls
      or trampolines.

      :param dict extra_symbols: Optional mapping of symbol names to
         integer addresses. See :ref:`python-symbol-resolution`.
      :returns: A callable executable buffer.
      :rtype: ExecutableBuffer
      :raises unit.Error: If JIT compilation fails.

      .. code-block:: python
         :caption: :iconify:`akar-icons:python-fill` Example

         # Simple JIT
         buf = compiled.jit()
         result = buf(42)

         # With custom symbols
         import ctypes
         addr = ctypes.cast(my_func, ctypes.c_void_p).value
         buf = compiled.jit(extra_symbols={"my_func": addr})

   .. py:method:: write_object_file(path, format) -> None

      Write the compiled procedure to an object file.

      :param str path: Output file path.
      :param str format: ``"elf"``, ``"macho"``, ``"coff"``, or ``None`` (meaning auto-detect).
      :raises ValueError: If *format* is not recognized.
      :raises unit.Error: If writing fails.

      .. code-block:: python
         :caption: :iconify:`akar-icons:python-fill` Example

         compiled.write_object_file("output.o", "elf")

   .. py:method:: translation_text() -> str

      Return the register IR as a string. Useful for debugging.


.. _python-executable-buffers:

Executable buffers
------------------

.. py:class:: unit.ExecutableBuffer

   A JIT-compiled function, callable directly. Returned by
   :py:meth:`CompiledProcedure.jit`. Not constructed directly.

   Arguments are automatically converted to ctypes types:

   - ``int`` -- ``c_int64``
   - ``float`` -- ``c_double``
   - ``str`` -- ``c_char_p`` (encoded to UTF-8)
   - ``bytes`` -- ``c_char_p``

   The return type is always ``c_int64``. For finer control, use
   the :py:attr:`address` property with ctypes directly.

   .. py:method:: __call__(*args) -> int

      Call the JIT-compiled function.

      :raises TypeError: If an argument has an unsupported type.

      .. code-block:: python

         buf = compiled.jit()
         result = buf(10, 20)  # call with two int arguments

   .. py:attribute:: address
      :type: int

      The raw function pointer address. Use with ctypes for custom
      calling conventions or return types.

      .. code-block:: python

         import ctypes
         func_type = ctypes.CFUNCTYPE(ctypes.c_double, ctypes.c_double)
         fn = func_type(buf.address)
         result = fn(3.14)


.. _python-symbol-resolution:

Symbol resolution
-----------------

The ``extra_symbols`` parameter on :py:meth:`CompiledProcedure.jit` accepts
a dictionary mapping symbol names to integer addresses. This is used for
custom symbol resolution during JIT compilation -- for example, to provide
trampolines for calling between JIT-compiled functions.

.. code-block:: python
   :caption: :iconify:`akar-icons:python-fill` Example

   import ctypes

   @ctypes.CFUNCTYPE(ctypes.c_int64, ctypes.c_int64)
   def my_trampoline(n):
       return n * 2

   addr = ctypes.cast(my_trampoline, ctypes.c_void_p).value
   buf = compiled.jit(extra_symbols={"my_func": addr})

Symbols in the dictionary are checked before falling back to ``dlsym``.
See :c:type:`UNIT_SymbolMap` for the underlying C API.
