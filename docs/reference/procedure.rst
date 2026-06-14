Procedures
==========

.. c:struct:: UNIT_Procedure

   A container of instructions.

   .. c:var:: UNIT_Context *context

      The context being used by this procedure.

   .. c:var:: const char *name

      The name of the procedure.


.. c:function:: UNIT_Status UNIT_Procedure_Init(UNIT_Procedure *procedure, UNIT_Context *context, const char *name)

   Initialize a procedure. On success, :c:func:`UNIT_Procedure_Clear` must be
   called later to free memory allocated by this function.

   :param procedure: A pointer to a procedure. Memory at this location will
                     be overwritten.
   :param context: The context that will be used when interacting with the procedure.
                   This must be valid for the lifetime of the procedure.
   :param name: A string indicating the name of the procedure. This string must
                be valid for the lifetime of the procedure.
   :return: Indicator whether the call was successful.
            See :c:macro:`UNIT_FAILED`.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context context;
      /* Initialize the context ... */

      UNIT_Procedure procedure;
      if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
          /* Clear the context ... */
          return -1;
      }

      // Must call UNIT_Procedure_Clear() later.


.. c:function:: UNIT_Procedure *UNIT_Procedure_New(UNIT_Context *context, const char *name)

   Create a new procedure. On success, :c:func:`UNIT_Procedure_Free` must be
   called later to free memory allocated by this function.

   :param context: The context that will be used when interacting with the procedure.
                   This must be valid for the lifetime of the procedure.
   :param name: A string indicating the name of the procedure. This string must
                be valid for the lifetime of the procedure.
   :return: A heap-allocated pointer to the procedure, or ``NULL`` if an allocation
            failed. If non-``NULL``, must be deallocated using
            :c:func:`UNIT_Procedure_Free`.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Context context;
      /* Initialize the context ... */

      UNIT_Procedure *procedure = UNIT_Procedure_New(&context, "main");
      if (procedure == NULL) {
          /* Clear the context ... */
          return -1;
      }

      // Must call UNIT_Procedure_Free() later.


.. c:function:: void UNIT_Procedure_Clear(UNIT_Procedure *procedure)

   Free memory allocated by :c:func:`UNIT_Procedure_Init`.

   :param procedure: The procedure to clear.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure procedure;
      if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, /* ... */))) {
         /* ... */
      }

      /* ... */

      UNIT_Procedure_Clear(&procedure);


.. c:function:: void UNIT_Procedure_Free(UNIT_Procedure *procedure)

   Free memory allocated by :c:func:`UNIT_Procedure_New`

   :param procedure: The procedure to free.

   .. code-block:: c
      :linenos:
      :caption: :iconify:`streamline-logos:c-language-logo-solid` Example

      UNIT_Procedure *procedure = UNIT_Procedure_New(/* ... */);
      if (procedure == NULL) {
         /* ... */
      }

      /* ... */

      UNIT_Procedure_Free(procedure);
