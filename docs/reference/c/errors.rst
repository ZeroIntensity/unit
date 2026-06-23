Errors
======

.. c:struct:: UNIT_Status

    The type indicating success or failure from a function call.

    .. c:var:: int8_t _status

        The actual status value. This is ``0`` for success and
        ``-1`` for failure. In general, you should never access this field
        manually, and thus why it is prefixed with ``_``. However, it may be
        necessary to access this field if calling UNIT from somewhere where
        C macros are not accessible (and thus :c:macro:`UNIT_FAILED` cannot
        be used), such as from an FFI.


    .. c:macro:: UNIT_FAILED(expr)

        Check if a :c:type:`UNIT_Status` returned by a function indicates failure.
        This is almost always used in ``if`` statements or in assertions.

        .. code-block:: c
           :linenos:
           :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

            if (UNIT_FAILED(UNIT_Context_Init(/* ... */))) {
                /* Handle failure somehow. */
                return -1;
            }

        .. admonition:: Looking for a ``UNIT_OK`` macro?

           UNIT does not provide a macro for checking if :c:type:`UNIT_Status`
           indicates success. For this pattern, simply negate the result of
           ``UNIT_FAILED``:

           .. code-block:: c
              :caption: :iconify:`streamline-logos:c-language-logo-solid` C

              #define OK(expr) (!UNIT_FAILED(expr))



.. c:enum:: UNIT_ErrorCode

   .. c:enumerator:: UNIT_ERROR_NONE

      No error is set.

   .. c:enumerator:: UNIT_ERROR_NO_MEMORY

      A memory allocation failed.

   .. c:enumerator:: UNIT_ERROR_INVALID_USAGE

      An API was misused by the caller. The error message will contain more
      information about what.

   .. c:enumerator:: UNIT_ERROR_OS_FAILURE

      A call to an operating system API failed. This implies that the
      C ``errno`` is set.

   .. c:enumerator:: UNIT_ERROR_UNSUPPORTED_PLATFORM

      An operation is not available on this platform.


.. c:function:: UNIT_ErrorCode UNIT_GetErrorCode(const UNIT_Context *context)

   Get the current error code.

   :param context: The context holding the error state.
   :return: The error code.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      if (UNIT_FAILED(UNIT_Procedure_Init(context, /* ... */))) {
          if (UNIT_GetErrorCode(context) == UNIT_ERROR_OS_FAILURE) {
              perror("Error whilst initializing UNIT procedure");
              return 1;
          }
      }


.. c:function:: const char *UNIT_GetErrorMessage(const UNIT_Context *context)

   Return the error message that is currently active on the context.
   If there isn't any error set, then this returns ``NULL``.

   :param context: The context holding the error state.
   :return: A string holding the error message. The caller is *not* responsible
            for managing the memory of this string.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      if (UNIT_FAILED(UNIT_Procedure_Init(context, /* ... */))) {
         const char *error = UNIT_GetErrorMessage(context);
         fprintf(stderr, "Error whilst initializing UNIT procedure: %s\n", error);
         return 1;
      }


.. c:function:: const char *UNIT_ErrorCode_ToString(UNIT_ErrorCode code)

   Get the name of the given error code, with the ``UNIT_ERROR`` prefix stripped
   out (so passing ``UNIT_ERROR_NO_MEMORY``, for example, will return the string
   ``"NO_MEMORY"``).

   This function will never return ``NULL``, but passing an invalid error code
   will result in the process crashing.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      if (UNIT_FAILED(UNIT_Procedure_Init(context, /* ... */))) {
          UNIT_ErrorCode code = UNIT_GetErrorCode();
          const char *name = UNIT_ErrorCode_ToString(code);
          fprintf(stderr, "UNIT failed with error code %d (%s)\n", code, name);
          return 1;
      }


.. c:function:: void UNIT_PrintError(const UNIT_Context *context, FILE *stream)

   Print out a formatted error message containing the error code and the error messaage
   to a stream. If no error is set, nothing is written to the stream.

   :param context: The context holding the error state.
   :param stream: The stream to write the error message to.
                  This is usually ``stderr``.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      if (UNIT_FAILED(UNIT_Procedure_Init(context, /* ... */))) {
          UNIT_PrintError(context, stderr);
          return 1;
      }
