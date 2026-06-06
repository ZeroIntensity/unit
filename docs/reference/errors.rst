``UNIT`` reference
==================

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
        For example:

        .. code-block:: c

            if (UNIT_FAILED(UNIT_Something(/* ... */))) {
                // Handle failure somehow
            }


.. c:enum:: UNIT_ErrorCode

   .. c:enumerator:: UNIT_ERROR_NONE

      No error is set.

   .. c:enumerator:: UNIT_ERROR_NO_MEMORY

      A memory allocation failed.

   .. c:enumerator:: UNIT_ERROR_INVALID_USAGE

      An API was misused by the caller. The error message will contain more
      information about what.

   .. c:enumerator:: UNIT_ERROR_OS_FAILURE

      A call to an operating system API failed.


.. c:function:: UNIT_ErrorCode UNIT_GetErrorCode(const UNIT_Context *context)

   Get the current error code.


.. c:function:: const char *UNIT_GetErrorMessage(const UNIT_Context *context)

    Return the error message that is currently active on the context.
    If there isn't any error set, then this returns ``NULL``.

    .. important:: Quirk

        A return value of ``NULL`` does not indicate failure, so no error is
        set in that case.


.. c:function:: const char *UNIT_ErrorCode_ToString(UNIT_ErrorCode code)

   Get the name of the given error code, with the ``UNIT_ERROR`` prefix stripped
   out (so passing ``UNIT_ERROR_NO_MEMORY``, for example, will return the string
   ``"NO_MEMORY"``).

   This function will never return ``NULL``, but passing an invalid error code
   will result in the process crashing.


.. c:function:: void UNIT_PrintError(const UNIT_Context *context, FILE *stream)

    Print out a formatted error message containing the error code and the error messaage
    to a stream. If no error is set, a status message is still written to the stream.
