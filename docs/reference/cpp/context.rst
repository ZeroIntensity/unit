Contexts and Errors
===================

.. cpp:namespace-push:: unit

.. cpp:class:: Context

   Container for UNIT's state. This type cannot be implicitly copied.

   This is a wrapper around :c:type:`UNIT_Context`.

   .. cpp:function:: Context()

      Initialize a context. This calls :c:func:`UNIT_Context_Init` internally.

      .. admonition:: Quirk
         :class: important

         On failure, this raises a ``std::runtime_exception`` instead of a
         :cpp:class:`unit::error`.

   .. cpp:function:: UNIT_Context *raw()

      Get the raw :c:type:`UNIT_Context` that this class is wrapping.

   .. cpp:function:: Context(const Context &) = delete
   .. cpp:function:: Context &operator=(const Context &) = delete

      These prevent implicit copying of this type.


.. cpp:enum-class:: ErrorCode

   Scoped wrapper around :c:enum:`UNIT_ErrorCode`. Values in this enum
   are guaranteed to be equivalent to their C counterpart (and thus statically
   casting this enum to ``UNIT_ErrorCode`` is safe).

   .. list-table::
      :header-rows: 1
      :widths: 40 20

      * - C++ Enum
        - C Enum
      * - .. cpp:enumerator:: NONE
        - :c:enumerator:`UNIT_ERROR_NONE`
      * - .. cpp:enumerator:: NO_MEMORY
        - :c:enumerator:`UNIT_ERROR_NO_MEMORY`
      * - .. cpp:enumerator:: INVALID_USAGE
        - :c:enumerator:`UNIT_ERROR_INVALID_USAGE`
      * - .. cpp:enumerator:: OS_FAILURE
        - :c:enumerator:`UNIT_ERROR_OS_FAILURE`
      * - .. cpp:enumerator:: UNSUPPORTED_PLATFORM
        - :c:enumerator:`UNIT_ERROR_UNSUPPORTED_PLATFORM`


.. cpp:class:: error : public std::runtime_exception

   Wrapper around UNIT's error API as a C++ exception. Functions that return
   :c:type:`UNIT_Status` in UNIT's C API will raise this exception from their
   C++ counterpart.

   .. cpp:function:: error(UNIT_Context *ctx)

      :param ctx: The raw :c:type:`UNIT_Context` in which the error originated.

   .. cpp:function:: ErrorCode code() const

      Get the error code.
