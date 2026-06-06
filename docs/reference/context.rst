``UNIT_Context`` reference
==========================


.. c:type:: UNIT_Context

   Container type for all of UNIT's state.


.. c:function:: UNIT_Status UNIT_Context_Init(UNIT_Context *context)

   Initialize a context.

   .. important:: Quirk

      If this function fails, there will *not* be any error indicator set,
      because there's no context to store it on.


.. c:function:: UNIT_Context *UNIT_Context_New(void)

   Create a new context.

   .. important:: Quirk

      If this function fails, there will *not* be any error indicator set,
      because there's no context to store it on.


.. c:function:: void UNIT_Context_Clear(UNIT_Context *context)

    Free memory allocated by :c:func:`UNIT_Context_Init`.


.. c:function:: void UNIT_Context_Free(UNIT_Context *context)

    Free memory allocated by :c:func:`UNIT_Context_New`. This will
    also call :c:func:`UNIT_Context_Clear`.
