.. _cpp-contexts:

Contexts
========

.. cpp:class:: unit::Context

   Manages memory for all UNIT objects. Procedures created with a context
   share its memory allocator. See :c:type:`UNIT_Context`.

   Context cannot be copied or moved. It must outlive all procedures and
   compiled objects created from it.

   .. cpp:function:: Context()

      Create and initialize a new context.

      :throws std::runtime_error: If initialization fails.

      .. code-block:: cpp
         :caption: :iconify:`devicon-plain:cplusplus` Example

         unit::Context ctx;
         unit::Procedure proc(ctx, "main");

   .. cpp:function:: ~Context()

      Clear the context and free all associated memory.

   .. cpp:function:: UNIT_Context *raw()

      Return the underlying C context pointer.
