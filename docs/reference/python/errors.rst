.. _python-errors:

Errors
======

All UNIT errors inherit from :py:class:`Error` and also from the most
appropriate Python built-in exception, so standard ``except`` clauses
work naturally.

.. code-block:: python

   try:
       proc.divide()
   except ValueError as e:
       # catches unit.InvalidUsage (which inherits ValueError)
       print(e)

   try:
       proc.compile()
   except unit.Error as e:
       # catches any UNIT error
       print(e)


.. py:class:: unit.Error

   Base class for all UNIT errors. Inherits from the internal C extension
   error type.

   .. py:attribute:: message
      :type: str

      The error message from the C library.


.. py:class:: unit.NoMemory

   Raised when memory allocation fails.
   Inherits from both :py:class:`Error` and :py:exc:`MemoryError`.


.. py:class:: unit.InvalidUsage

   Raised when the API is used incorrectly, such as dividing by zero
   during constant folding, or emitting instructions in an invalid order.
   Inherits from both :py:class:`Error` and :py:exc:`ValueError`.


.. py:class:: unit.OSFailure

   Raised when an operating system call fails, such as ``mmap`` during
   JIT compilation.
   Inherits from both :py:class:`Error` and :py:exc:`OSError`.


.. py:class:: unit.UnsupportedPlatform

   Raised when compiling for a platform that UNIT does not support.
   Inherits from :py:class:`Error`.


.. py:class:: unit.ErrorCode

   Enum of error codes from the C library. Primarily for internal use.

   .. py:attribute:: UNIT_ERROR_NONE
   .. py:attribute:: UNIT_ERROR_NO_MEMORY
   .. py:attribute:: UNIT_ERROR_INVALID_USAGE
   .. py:attribute:: UNIT_ERROR_OS_FAILURE
   .. py:attribute:: UNIT_ERROR_UNSUPPORTED_PLATFORM
