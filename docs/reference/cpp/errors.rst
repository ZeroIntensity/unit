.. _cpp-errors:

Errors
======

.. cpp:class:: unit::error : public std::exception

   Exception thrown by all UNIT C++ operations on failure. Wraps the
   error code and message from the C library.
   See :c:enum:`UNIT_ErrorCode` and :c:func:`UNIT_GetErrorMessage`.

   .. cpp:function:: const char *what() const noexcept

      Return the error message from the C library.

   .. cpp:function:: UNIT_ErrorCode code() const noexcept

      Return the error code.

   .. code-block:: cpp
      :caption: :iconify:`devicon-plain:cplusplus` Example

      try {
          unit::Context ctx;
          unit::Procedure proc(ctx, "main");
          proc.load_integer(42);
          proc.return_value();
          auto compiled = proc.compile(unit::Platform::host());
      } catch (const unit::error &e) {
          std::cerr << "UNIT error: " << e.what() << std::endl;
          // e.code() == UNIT_ERROR_INVALID_USAGE, etc.
      }

   All methods on :cpp:class:`~unit::Context`, :cpp:class:`~unit::Procedure`,
   :cpp:class:`~unit::CompiledProcedure`, and :cpp:class:`~unit::SymbolMap`
   throw ``unit::error`` on failure. The only exception is
   :cpp:func:`Context::Context()`, which throws ``std::runtime_error``
   because there is no context to extract an error from.


.. cpp:enum-class:: unit::ErrorCode : int

   Error codes returned by :cpp:func:`error::code`.
   See :c:enum:`UNIT_ErrorCode`.

   .. cpp:enumerator:: None = UNIT_ERROR_NONE

      No error is set.

   .. cpp:enumerator:: NoMemory = UNIT_ERROR_NO_MEMORY

      A memory allocation failed.

   .. cpp:enumerator:: InvalidUsage = UNIT_ERROR_INVALID_USAGE

      An API was misused by the caller. Check :cpp:func:`error::what`
      for details.

   .. cpp:enumerator:: OSFailure = UNIT_ERROR_OS_FAILURE

      A call to an operating system API failed.

   .. cpp:enumerator:: UnsupportedPlatform = UNIT_ERROR_UNSUPPORTED_PLATFORM

      An operation is not available on this platform.

   .. code-block:: cpp
      :caption: :iconify:`devicon-plain:cplusplus` Example

      try {
          auto compiled = proc.compile(unit::Platform::host());
      } catch (const unit::error &e) {
          if (e.code() == unit::ErrorCode::UnsupportedPlatform) {
              std::cerr << "platform not supported" << std::endl;
          }
      }
