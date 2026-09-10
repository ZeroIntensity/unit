.. _c-compilation:

Compilation
===========


Platform
--------

.. c:type:: UNIT_Platform

   An integer encoding the target architecture and ABI. Constructed by
   bitwise OR of a :c:enum:`UNIT_Architecture` and a :c:enum:`UNIT_ABI`.

   .. code-block:: c

      UNIT_Platform platform = UNIT_ARCH_AMD64 | UNIT_ABI_SYSTEMV;


.. c:enum:: UNIT_Architecture

   .. c:enumerator:: UNIT_ARCH_AMD64

      x86-64 (AMD64). This is the only architecture currently implemented.

   .. c:enumerator:: UNIT_ARCH_AARCH64

      ARM64 (AArch64). Not yet implemented.


.. c:enum:: UNIT_ABI

   .. c:enumerator:: UNIT_ABI_SYSTEMV

      System V AMD64 ABI. Used on Linux, FreeBSD, and other Unix-like systems.

   .. c:enumerator:: UNIT_ABI_WIN64

      Windows x64 calling convention. Not yet implemented.

   .. c:enumerator:: UNIT_ABI_APPLE

      Apple ARM64 ABI. Not yet implemented.


.. c:macro:: UNIT_HOST_PLATFORM

   The platform of the current machine, auto-detected at compile time.
   This is the most common value to pass to :c:func:`UNIT_Compile`.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);


.. c:function:: UNIT_ABI UNIT_Platform_GET_ABI(UNIT_Platform platform)

   Extract the ABI from a platform value.

   :param platform: The platform.
   :return: The ABI component.


.. c:function:: UNIT_Architecture UNIT_Platform_GET_ARCH(UNIT_Platform platform)

   Extract the architecture from a platform value.

   :param platform: The platform.
   :return: The architecture component.


Compiling
---------

.. c:function:: UNIT_CompiledProcedure *UNIT_Compile(const UNIT_Procedure *procedure, UNIT_Platform platform)

   Compile a procedure to machine code. This translates the stack IR to
   register IR, performs register allocation, runs register IR optimization
   (unless :c:macro:`UNIT_FLAG_NO_OPTIMIZE_TRANSLATION` is set), and
   encodes the result as machine code.

   :param procedure: The procedure to compile. Must have been optimized
      with :c:func:`UNIT_Procedure_Optimize` first (optional but recommended).
   :param platform: The target platform.
   :return: A heap-allocated compiled procedure, or ``NULL`` on failure.
            Must be freed with :c:func:`UNIT_CompiledProcedure_Free`.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure_Optimize(&proc);
      UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);
      if (compiled == NULL) {
          UNIT_PrintError(proc.context, stderr);
          return -1;
      }

      // Use compiled procedure ...

      UNIT_CompiledProcedure_Free(compiled);


.. c:struct:: UNIT_CompiledProcedure

   A compiled procedure containing machine code, ready for JIT execution
   or object file output.

   .. c:var:: UNIT_Context *context

      The context.

   .. c:var:: UNIT_Platform platform

      The target platform.

   .. c:var:: const char *name

      The procedure name.


.. c:function:: void UNIT_CompiledProcedure_Free(UNIT_CompiledProcedure *compiled)

   Free a compiled procedure. Safe to call with ``NULL``.

   :param compiled: The compiled procedure to free, or ``NULL``.


Object files
------------

.. c:enum:: UNIT_ExecutableFormat

   .. c:enumerator:: UNIT_FORMAT_ELF

      Executable and Linkable Format (ELF). Linux, FreeBSD, etc.

   .. c:enumerator:: UNIT_FORMAT_MACHO

      Mach Object (Mach-O) format. macOS. Not yet implemented.

   .. c:enumerator:: UNIT_FORMAT_COFF

      Common Object File Format. Windows.


.. c:macro:: UNIT_HOST_FORMAT

   The executable format used by the current operating system, auto-detected at
   compile time.
   This is the most common value to pass to :c:func:`UNIT_CompiledProcedure_WriteObjectFile`.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_CompiledProcedure_WriteObjectFile(compiled_procedure, UNIT_HOST_FORMAT);


.. c:function:: UNIT_Status UNIT_CompiledProcedure_WriteObjectFile(const UNIT_CompiledProcedure *compiled, const char *path, UNIT_ExecutableFormat format)

   Write the compiled procedure to an object file. The resulting file
   can be linked with ``gcc`` or ``clang``.

   :param compiled: The compiled procedure.
   :param path: The output file path.
   :param format: The object file format.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o", UNIT_FORMAT_ELF);

   .. code-block:: bash
      :caption: Linking

      gcc output.o -o output


JIT Compilation
---------------

.. c:function:: UNIT_ExecutableBuffer *UNIT_CompiledProcedure_JIT(const UNIT_CompiledProcedure *compiled, const UNIT_SymbolMap *symbol_map)

   JIT compile a procedure into executable memory. Returns an opaque buffer
   containing the compiled function.

   Symbols referenced by :c:enumerator:`UNIT_OP_CALL_NAME` are resolved
   first from *symbol_map* (if provided), then via ``dlsym``.

   :param compiled: The compiled procedure.
   :param symbol_map: Custom symbol resolution map, or ``NULL`` to use
      ``dlsym`` only.
   :return: A heap-allocated executable buffer, or ``NULL`` on failure.
            Must be freed with :c:func:`UNIT_ExecutableBuffer_Free`.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);
      if (buf == NULL) {
          UNIT_PrintError(compiled->context, stderr);
          return -1;
      }

      int64_t (*fn)(int64_t) = (int64_t (*)(int64_t))UNIT_ExecutableBuffer_GetPointer(buf);
      printf("%ld\n", fn(42));

      UNIT_ExecutableBuffer_Free(buf);


.. c:type:: UNIT_ExecutableBuffer

   An opaque type representing JIT-compiled executable memory. The internal
   structure is not publicly visible.


.. c:function:: void *UNIT_ExecutableBuffer_GetPointer(const UNIT_ExecutableBuffer *buffer)

   Get the function pointer from an executable buffer. Cast this to the
   appropriate function pointer type.

   :param buffer: The executable buffer.
   :return: A pointer to the compiled function.


.. c:function:: void UNIT_ExecutableBuffer_Free(UNIT_ExecutableBuffer *buffer)

   Free an executable buffer and its mapped memory. Safe to call with ``NULL``.

   :param buffer: The buffer to free, or ``NULL``.


Symbol maps
-----------

.. c:struct:: UNIT_SymbolMap

   A map from symbol names to addresses, used for custom symbol resolution
   during JIT compilation. Useful for providing trampolines, callbacks,
   or calling between JIT-compiled functions.

   .. c:var:: UNIT_Context *context

      The context.


.. c:function:: UNIT_Status UNIT_SymbolMap_Init(UNIT_SymbolMap *symbol_map, UNIT_Context *context)

   Initialize a symbol map. On success, :c:func:`UNIT_SymbolMap_Clear`
   must be called later.

   :param symbol_map: A pointer to a symbol map.
   :param context: The context.


.. c:function:: UNIT_SymbolMap *UNIT_SymbolMap_New(UNIT_Context *context)

   Create a new heap-allocated symbol map. On success,
   :c:func:`UNIT_SymbolMap_Free` must be called later.

   :param context: The context.
   :return: A heap-allocated symbol map, or ``NULL`` on failure.


.. c:function:: void UNIT_SymbolMap_Clear(UNIT_SymbolMap *symbol_map)

   Free memory allocated by :c:func:`UNIT_SymbolMap_Init`.


.. c:function:: void UNIT_SymbolMap_Free(UNIT_SymbolMap *symbol_map)

   Free memory allocated by :c:func:`UNIT_SymbolMap_New`. Safe to call
   with ``NULL``.


.. c:function:: UNIT_Status UNIT_SymbolMap_RegisterSymbol(UNIT_SymbolMap *symbol_map, const char *name, void *address)

   Register a symbol name with its address. During JIT compilation,
   this address is used instead of ``dlsym`` for the given name.

   :param symbol_map: The symbol map.
   :param name: The symbol name.
   :param address: The function pointer or data address.

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      int64_t my_callback(int64_t x) { return x * 2; }

      UNIT_SymbolMap symbols;
      UNIT_SymbolMap_Init(&symbols, &ctx);
      UNIT_SymbolMap_RegisterSymbol(&symbols, "my_callback",
                                     (void *)my_callback);

      UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, &symbols);

      // The compiled code can now call "my_callback" and it will
      // resolve to the function above.

      UNIT_SymbolMap_Clear(&symbols);


Debugging
---------

.. c:function:: UNIT_Status UNIT_CompiledProcedure_PrintTranslatedIR(const UNIT_CompiledProcedure *compiled, FILE *stream)

   Print the register IR to a file stream. Shows the translation output
   after register allocation and optimization.

   :param compiled: The compiled procedure.
   :param stream: The output stream (e.g. ``stdout``).

   .. code-block:: c
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_CompiledProcedure_PrintTranslatedIR(compiled, stdout);
