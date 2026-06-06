Procedures
==========


.. c:struct:: UNIT_Procedure

   A container of instructions.

    .. c:var:: UNIT_Context *context

        The context being used by this procedure.

    .. c:var:: const char *name

        The name of the procedure.


.. c:function:: UNIT_Status UNIT_Procedure_Init(UNIT_Procedure *procedure, UNIT_Context *context, const char *name)
.. c:function:: UNIT_Procedure *UNIT_Procedure_New(UNIT_Context *context, const char *name)
.. c:function:: void UNIT_Procedure_Clear(UNIT_Procedure *procedure)
.. c:function:: void UNIT_Procedure_Free(UNIT_Procedure *procedure)

   Standard structure lifecycle functions for a procedure.
