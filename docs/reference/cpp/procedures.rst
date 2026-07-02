.. _cpp-procedures:

Procedures
==========

.. cpp:class:: unit::Procedure

   A procedure represents a single function to be compiled. It holds
   the stack-based IR and provides methods to emit instructions.

   Procedures cannot be copied or moved.

   .. cpp:function:: explicit Procedure(Context &ctx, const std::string &name)

      Create a new procedure.

      :param ctx: The context that owns this procedure's memory.
      :param name: The symbol name for the compiled function.
      :throws unit::error: If initialization fails.

   .. cpp:function:: UNIT_Procedure *raw()

      Return the underlying C procedure pointer.

   .. cpp:function:: void optimize()

      Run stack IR optimization passes: inlining, constant folding,
      dead store elimination, and local variable optimization.
      Call this before :cpp:func:`compile`.

      :throws unit::error: If optimization fails.

   .. cpp:function:: CompiledProcedure compile(Platform platform)

      Compile the procedure to machine code for the given platform.

      :param platform: Target platform (e.g. ``Platform::host()``).
      :returns: A compiled procedure ready for JIT or object file output.
      :throws unit::error: If compilation fails.

   .. cpp:function:: void print_instructions(FILE *stream = stdout)

      Print the stack IR to a file stream. Useful for debugging.

   .. cpp:function:: void set_flags(Flag flags)

      Set procedure flags.

      :param flags: Bitwise OR of :cpp:enum:`Flag` values.

   .. cpp:function:: Flag get_flags() const

      Return the current procedure flags.

   **Stack Operations**

   .. cpp:function:: void load_integer(int64_t value)

      Push an integer constant onto the stack.
      See :c:macro:`UNIT_OP_LOAD_INTEGER`.

      **Stack effect:** ``-- value``

      .. code-block:: cpp
         :caption: :iconify:`devicon-plain:cplusplus` Example

         proc.load_integer(42); // stack: [42]

   .. cpp:function:: void load_string(const std::string &value)

      Push a string constant onto the stack. The string is copied internally.
      See :c:func:`UNIT_Procedure_AddStringLoad`.

      **Stack effect:** ``-- pointer``

   .. cpp:function:: void load_argument(uint8_t index)

      Push a function argument onto the stack. Argument 0 is the first
      parameter.
      See :c:macro:`UNIT_OP_LOAD_ARGUMENT`.

      **Stack effect:** ``-- value``

   .. cpp:function:: void load_local(int64_t index)

      Push a local variable onto the stack by index.
      See :c:macro:`UNIT_OP_LOAD_LOCAL`.

      **Stack effect:** ``-- value``

   .. cpp:function:: void store_local(int64_t index)

      Pop the top of the stack into a local variable by index.
      See :c:macro:`UNIT_OP_STORE_LOCAL`.

      **Stack effect:** ``value --``

   .. cpp:function:: void load_name(Local local)

      Push a local variable onto the stack by handle.
      See :c:func:`UNIT_Procedure_AddLoadName`.

      **Stack effect:** ``-- value``

   .. cpp:function:: void store_name(Local local)

      Pop the top of the stack into a local variable by handle.
      See :c:func:`UNIT_Procedure_AddStoreName`.

      **Stack effect:** ``value --``

   .. cpp:function:: void pop()

      Discard the top of the stack.
      See :c:macro:`UNIT_OP_POP`.

      **Stack effect:** ``value --``

   .. cpp:function:: void copy(int64_t offset)

      Duplicate the stack item at the given depth. ``copy(0)`` duplicates
      the top.
      See :c:macro:`UNIT_OP_COPY`.

      **Stack effect:** ``-- value``

      .. code-block:: cpp

         proc.load_integer(10);   // stack: [10]
         proc.load_integer(20);   // stack: [10, 20]
         proc.copy(1);            // stack: [10, 20, 10]

   .. cpp:function:: void swap(int64_t offset)

      Swap the top of the stack with the item at the given depth.
      See :c:macro:`UNIT_OP_SWAP`.

      **Stack effect:** unchanged (items rearranged)

   **Arithmetic**

   .. cpp:function:: void add()

      Pop two values, push their sum.
      See :c:macro:`UNIT_OP_ADD`.

      **Stack effect:** ``a b -- a+b``

   .. cpp:function:: void subtract()

      Pop two values, push their difference.
      See :c:macro:`UNIT_OP_SUBTRACT`.

      **Stack effect:** ``a b -- a-b``

   .. cpp:function:: void multiply()

      Pop two values, push their product.
      See :c:macro:`UNIT_OP_MULTIPLY`.

      **Stack effect:** ``a b -- a*b``

   .. cpp:function:: void divide()

      Pop two values, push their quotient (integer division, truncated toward zero).
      See :c:macro:`UNIT_OP_DIVIDE`.

      **Stack effect:** ``a b -- a/b``

   .. cpp:function:: void modulo()

      Pop two values, push the remainder.
      See :c:macro:`UNIT_OP_MODULO`.

      **Stack effect:** ``a b -- a%b``

   **Comparisons**

   All comparisons pop two values, push a comparison result consumed by
   :cpp:func:`jump_if_true` or :cpp:func:`jump_if_false`.

   .. cpp:function:: void compare_equal()

      See :c:macro:`UNIT_OP_COMPARE_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. cpp:function:: void compare_not_equal()

      See :c:macro:`UNIT_OP_COMPARE_NOT_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. cpp:function:: void compare_less()

      See :c:macro:`UNIT_OP_COMPARE_LESS`.

      **Stack effect:** ``a b -- result``

   .. cpp:function:: void compare_less_equal()

      See :c:macro:`UNIT_OP_COMPARE_LESS_EQUAL`.

      **Stack effect:** ``a b -- result``

   .. cpp:function:: void compare_greater()

      See :c:macro:`UNIT_OP_COMPARE_GREATER`.

      **Stack effect:** ``a b -- result``

   .. cpp:function:: void compare_greater_equal()

      See :c:macro:`UNIT_OP_COMPARE_GREATER_EQUAL`.

      **Stack effect:** ``a b -- result``

   **Control Flow**

   .. cpp:function:: JumpLabel create_jump_label(const std::string &name)

      Create a jump target. The label must later be placed with
      :cpp:func:`use_label`.

      :returns: A label handle for use with jump instructions.
      :throws unit::error: If allocation fails.

   .. cpp:function:: void use_label(JumpLabel label)

      Place a label at the current position in the instruction stream.
      All jumps to this label will target the next instruction emitted.

   .. cpp:function:: void jump_to(JumpLabel label)

      Unconditional jump.
      See :c:macro:`UNIT_OP_JUMP_TO`.

   .. cpp:function:: void jump_if_true(JumpLabel label)

      Pop a comparison result. Jump if true.
      See :c:macro:`UNIT_OP_JUMP_IF_TRUE`.

      **Stack effect:** ``comparison --``

   .. cpp:function:: void jump_if_false(JumpLabel label)

      Pop a comparison result. Jump if false.
      See :c:macro:`UNIT_OP_JUMP_IF_FALSE`.

      **Stack effect:** ``comparison --``

   .. code-block:: cpp
      :caption: :iconify:`devicon-plain:cplusplus` Example

      auto end = proc.create_jump_label("end");
      proc.load_integer(5);
      proc.load_integer(5);
      proc.compare_equal();
      proc.jump_if_true(end);
      // ... not-equal path ...
      proc.use_label(end);
      // ... equal path continues here ...

   **Calls**

   .. cpp:function:: void call_name(const std::string &name, uint8_t nargs)

      Call an external function by name. The top *nargs* stack items are
      passed as arguments (first argument pushed first). The return value
      is pushed onto the stack.
      See :c:func:`UNIT_Procedure_AddCallName`.

      **Stack effect:** ``arg1 arg2 ... argN -- result``

      .. code-block:: cpp
         :caption: :iconify:`devicon-plain:cplusplus` Example

         proc.load_string("%d\n");
         proc.load_integer(42);
         proc.call_name("printf", 2);
         proc.pop();  // discard printf return value

   .. cpp:function:: void call_procedure(Procedure &target, uint8_t nargs)

      Call another UNIT procedure. This enables inlining during optimization.
      See :c:func:`UNIT_Procedure_AddCallProcedure`.

      **Stack effect:** ``arg1 arg2 ... argN -- result``

   .. cpp:function:: void return_value()

      Pop the top of the stack and return it to the caller.
      See :c:macro:`UNIT_OP_RETURN_VALUE`.

      **Stack effect:** ``value --``

   .. cpp:function:: void exit()

      Terminate the process immediately.
      See :c:macro:`UNIT_OP_EXIT`.

   **Memory Access**

   .. cpp:function:: void read_bytes(uint8_t num_bytes)

      Pop an address, read *num_bytes* from it (1, 2, 4, or 8), push
      the value.
      See :c:macro:`UNIT_OP_READ_BYTES`.

      **Stack effect:** ``address -- value``

   .. cpp:function:: void write_bytes(uint8_t num_bytes)

      Pop an address and a value, write *num_bytes* of the value to
      the address.
      See :c:macro:`UNIT_OP_WRITE_BYTES`.

      **Stack effect:** ``address value --``

   .. cpp:function:: void address_of(Local local)

      Push the memory address of a local variable.
      See :c:macro:`UNIT_OP_ADDRESS_OF`.

      **Stack effect:** ``-- address``

   **Type Conversion**

   .. cpp:function:: void convert(IntegerType type)

      Convert the top of the stack to a different integer width.
      See :c:macro:`UNIT_OP_CONVERT`.

      **Stack effect:** ``value -- converted_value``

   **Locals**

   .. cpp:function:: Local create_local(const std::string &name)

      Create a named local variable. Returns a handle for use with
      :cpp:func:`store_name` and :cpp:func:`load_name`.

      :throws unit::error: If allocation fails.


.. _cpp-local-variables:

Local variables
---------------

.. cpp:class:: unit::Local

   A handle to a local variable, returned by
   :cpp:func:`Procedure::create_local`.

   .. cpp:function:: constexpr bool operator==(Local other) const
   .. cpp:function:: constexpr bool operator!=(Local other) const

   .. cpp:function:: UNIT_Local raw() const

      Return the underlying C local handle.


.. _cpp-jump-labels:

Jump labels
-----------

.. cpp:class:: unit::JumpLabel

   A handle to a jump target, returned by
   :cpp:func:`Procedure::create_jump_label`.

   .. cpp:function:: bool operator==(JumpLabel other) const
   .. cpp:function:: bool operator!=(JumpLabel other) const

   .. cpp:function:: UNIT_JumpLabel *raw() const

      Return the underlying C label pointer.

.. _cpp-flags:

Flags
-----

.. cpp:enum-class:: unit::Flag : uint32_t

   Procedure flags, combined with bitwise OR.

   .. cpp:enumerator:: NONE = 0

      No flags.

   .. cpp:enumerator:: NO_OPTIMIZE_TRANSLATION = UNIT_FLAG_NO_OPTIMIZE_TRANSLATION

      Skip register IR optimization (move coalescing, dead move elimination,
      forward copy propagation) during compilation.

   .. cpp:enumerator:: FORCE_NO_INLINE = UNIT_FLAG_FORCE_NO_INLINE

      Prevent this procedure from being inlined into callers during
      optimization, regardless of size.

   .. cpp:enumerator:: FORCE_INLINE = UNIT_FLAG_FORCE_INLINE

      Always inline this procedure into callers, regardless of size.

   .. cpp:enumerator:: PRINT_TRANSLATION_PREOP = UNIT_FLAG_PRINT_TRANSLATION_PREOP

      Print the register IR to stderr before optimization runs.
      Useful for debugging.

   .. cpp:enumerator:: PRINT_TRANSLATION_POSTOP = UNIT_FLAG_PRINT_TRANSLATION_POSTOP

      Print the register IR to stderr after optimization runs.
      Useful for debugging.

   .. code-block:: cpp
      :caption: :iconify:`devicon-plain:cplusplus` Example

      proc.set_flags(unit::Flag::FORCE_NO_INLINE);

      // Combine flags with bitwise OR
      proc.set_flags(unit::Flag::FORCE_NO_INLINE | unit::Flag::NO_OPTIMIZE_TRANSLATION);


.. _cpp-compiled-procedures:

Compiled procedures
-------------------

.. cpp:class:: unit::CompiledProcedure

   A compiled procedure, ready for JIT execution or object file output.
   Returned by :cpp:func:`Procedure::compile`. Cannot be copied. Move-only.

   .. cpp:function:: template <typename Func> ExecutableBuffer<Func> jit()

      JIT compile and return an executable buffer. Symbols are resolved
      via ``dlsym``.

      :throws unit::error: If JIT compilation fails.

   .. cpp:function:: template <typename Func> ExecutableBuffer<Func> jit(SymbolMap &symbols)

      JIT compile with custom symbol resolution. Symbols in the map are
      checked before falling back to ``dlsym``.

      :throws unit::error: If JIT compilation fails.

   .. cpp:function:: void write_object_file(const std::string &path, ExecutableFormat format)

      Write the compiled procedure to an object file.

      :param path: Output file path.
      :param format: Object file format (e.g., ``ExecutableFormat::ELF``).
      :throws unit::error: If writing fails.

   .. cpp:function:: void print_translated(FILE *stream = stdout)

      Print the register IR to a file stream. Useful for debugging.

   .. cpp:function:: UNIT_CompiledProcedure *raw() const

      Return the underlying C compiled procedure pointer.


.. _cpp-executable-buffers:

Executable buffers
------------------

.. cpp:class:: template <typename Func> unit::ExecutableBuffer

   A JIT-compiled function, callable directly. Returned by
   :cpp:func:`CompiledProcedure::jit`. Cannot be copied. Move-only.
   The executable memory is freed on destruction.

   ``Func`` is the function pointer type, e.g. ``int64_t(*)(int64_t)``.

   .. cpp:function:: template <typename... Args> auto operator()(Args... args) const

      Call the JIT-compiled function.

      .. code-block:: cpp

         auto add = compiled.jit<int64_t(*)(int64_t, int64_t)>();
         int64_t result = add(3, 4);  // 7


.. _cpp-executable-maps:

Symbol maps
-----------

.. cpp:class:: unit::SymbolMap

   A map from symbol names to addresses, used for custom symbol resolution
   during JIT compilation. Cannot be copied or moved.

   .. cpp:function:: explicit SymbolMap(Context &ctx)

      Create an empty symbol map.

      :throws unit::error: If initialization fails.

   .. cpp:function:: void register_symbol(const std::string &name, void *address)

      Register a symbol name with its address.

      :param name: The symbol name (e.g. ``"my_function"``).
      :param address: The function pointer or data address.

   .. code-block:: cpp
      :caption: :iconify:`devicon-plain:cplusplus` Example

      unit::SymbolMap symbols(ctx);
      symbols.register_symbol("my_callback", (void *)my_callback);
      auto fn = compiled.jit<int64_t(*)()>(symbols);
