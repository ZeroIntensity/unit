Contexts
========


.. c:type:: UNIT_Context

   Container type for all of UNIT's state. This structure has no public
   fields.


.. c:function:: UNIT_Status UNIT_Context_Init(UNIT_Context *context)

   Initialize a context. On success, :c:func:`UNIT_Context_Clear` must be called
   later to free memory allocated by this function.

   :param context: A pointer to a context. Memory at this location will
                   be overwritten.
   :return: Indicator whether the call was successful.
            See :c:macro:`UNIT_FAILED`.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context context;
      if (UNIT_FAILED(UNIT_Context_Init(&context))) {
         return -1;
      }

      // Must call UNIT_Context_Clear() later.

   .. admonition:: Quirk
      :class: important

      If this function fails, there will *not* be any error indicator set,
      because there's no context to store it on.


.. c:function:: UNIT_Context *UNIT_Context_New(void)

   Create a new context. On success, :c:func:`UNIT_Context_Free` must be
   called later to free memory allocated by this function.

   :return: A heap-allocated pointer to the context, or ``NULL`` if an allocation
            failed. If non-``NULL``, must be deallocated using
            :c:func:`UNIT_Context_Free`.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context *context = UNIT_Context_New();
      if (context == NULL) {
         return -1;
      }

      // Must call UNIT_Context_Free() later.

   .. admonition:: Quirk
      :class: important

      If this function fails, there will *not* be any error indicator set,
      because there's no context to store it on.


.. c:function:: void UNIT_Context_Clear(UNIT_Context *context)

   Free memory allocated by :c:func:`UNIT_Context_Init`.

   :param context: The context to clear.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context context;
      if (UNIT_FAILED(UNIT_Context_Init(&context))) {
         /* ... */
      }

      /* ... */

      UNIT_Context_Clear(&context);


.. c:function:: void UNIT_Context_Free(UNIT_Context *context)

   Free memory allocated by :c:func:`UNIT_Context_New`. This will
   also call :c:func:`UNIT_Context_Clear`.

   :param context: The context to free.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context *context = UNIT_Context_New();
      if (context == NULL) {
         /* ... */
      }

      /* ... */

      UNIT_Context_Clear(&context);
