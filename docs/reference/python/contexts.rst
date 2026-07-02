.. _python-contexts:

Contexts
========

.. py:class:: unit.Context

   Manages state and memory for all UNIT objects. Procedures created with a context
   share its memory allocator.

   Context supports the context manager protocol. When used with ``with``,
   it becomes the current context, so procedures can be created without
   passing it explicitly.

   .. code-block:: python

      # Explicit context
      ctx = unit.Context()
      proc = unit.Procedure("main", context=ctx)

      # Implicit via context manager
      with unit.Context():
          proc = unit.Procedure("main")  # uses the current context

   .. py:method:: __enter__() -> Context

      Set this context as the current context.

   .. py:method:: __exit__(*args) -> None

      Restore the previous context.

   .. py:classmethod:: current() -> Context | None

      Return the current context set by a ``with`` block, or ``None``
      if no context is active.

   .. py:classmethod:: current_or_new() -> Context

      Return the current context if one is active, otherwise create and
      return a new one. This is what :py:class:`Procedure` calls internally
      when no context is provided.
