The UNIT API
============

Before we can start generating machine code, we have to know some background
information about using UNIT as a whole.


Naming conventions
------------------

In UNIT's C API, every public API is prefixed with ``UNIT_``.
There is a hidden private namespace under the ``_UNIT_`` prefix, but it is
strongly discouraged to use any APIs under that namespace.

.. note::

   Given that UNIT is in the early stages of development, there are likely
   APIs hidden behind the private ``_UNIT`` namespace that may be useful to
   you as a user. If you encounter something under ``_UNIT`` that you would
   like available publicly, please open an issue on `the issue tracker
   <https://github.com/ZeroIntensity/unit/issues>`_.


In general, every type in UNIT will be under its own namespace. So, for example,
functions relating to a type called ``UNIT_SomeType`` will be named
``UNIT_SomeType_DoSomething``.

.. tip::

    This is best paired with intellisense in your editor; if you're looking
    for a "method" (there aren't really methods in C, but this is how we
    replicate them), simply write the type name and let autocomplete bring up
    the relevant functions.


The global context
------------------

UNIT focuses on being an embeddable C library. This means that UNIT will
never cause any sort of side-effects to the program that is calling it.
In practice, this means that there is no global state. As such, an instance
of UNIT is wrapped in a type called :c:type:`UNIT_Context`. All information
that would otherwise be global, such as the state of the memory allocator and
the error status (similar to ``errno``), is stored here.

Generally speaking, you don't need to care much about what is actually stored
on the context, but you do always need to have one. A context can be constructed
using :c:func:`UNIT_Context_Init` or :c:func:`UNIT_Context_New`, like
we learned above.


Error handling
--------------

In C code, many things can fail, such as a heap allocation or a call to some
operating system API. To produce sane code, we need to handle these errors.
Because C does not have exceptions, we denote failure by returning a sentinel
value.

In UNIT's C API, there are two ways that functions do this:

1. For functions that return pointers, ``NULL`` is returned to indicate
   failure.
2. Everything else uses a special type called :c:type:`UNIT_Status`.

When a function fails, the "error indicator" on the context will almost
always be set. The error indicator has two parts: the status code and the message.

.. hint::

   The error indicator is "almost always" set upon failure because there are
   a few exceptional cases where there's no active UNIT context, and thus
   no error can be set (such as during the construction of a :c:type:`UNIT_Context`).


As an example, let's call some function that sets an error and returns ``NULL``
upon failure. For our purposes, let's simply print out the error message.
We can do this using a function called :c:func:`UNIT_PrintError`, like so:


.. code-block:: c

    UNIT_SomeType *something = UNIT_Something(context);
    if (something == NULL) {
        UNIT_PrintError(context, stderr);
        return 1; // Assuming we're in the main function
    }

If the call to ``UNIT_Something`` fails, then we'll see an error message printed
to ``stderr`` describing what went wrong.


The ``UNIT_Status`` type
************************

:c:type:`UNIT_Status` is a special type used for error handling in UNIT's C API.
Rather than using a sentinel ``0`` or ``-1`` value, it forces us to be explicit
and cautious with error handling.

``UNIT_Status`` is a single-field structure, so it cannot be used in a comparison
to ``0`` or ``-1``. This is intentional. Instead, we need a special macro
called :c:macro:`UNIT_FAILED`, which takes a status and returns 1 or 0, depending
on whether it indicates failure.

From there, we can simply use ``UNIT_FAILED`` in place of ``== NULL``
comparisons for functions that return ``UNIT_Status``. For example:

.. code-block:: c

   if (UNIT_FAILED(UNIT_OtherSomething(context))) {
       UNIT_PrintError(context, stderr);
       return 1;
   }


Structures
----------

In UNIT, structures are named with the ``UNIT_StructureName`` format.
Operations on a structure are then under the format ``UNIT_StructureName_DoSomething``.

All structures in the public C API come with construction and destruction
functions. There are two types of construction and destruction in UNIT's C API.

The first type uses the names ``UNIT_StructureName_Init`` and ``UNIT_StructureName_Clear``.
A ``UNIT_StructureName_Init`` function will always take a pointer to
``UNIT_StructureName`` as the first argument, and then any additional
structure-specific arguments will follow. ``Init`` is intended to initialize a
structure in a block of memory that is already allocated. In practice, this
is usually something like a stack allocation. For example:

.. code-block:: c

    UNIT_StructureName structure;
    if (UNIT_FAILED(UNIT_StructureName_Init(&structure))) {
        // Handle error
    }


A ``UNIT_StructureName_Clear`` function will deallocate all memory allocated
in the ``Init`` function; as such, there should always be at least one call to
``Clear`` for every call to ``Init``. A ``Clear`` function will always take a single
pointer to ``UNIT_StructureName`` and no other arguments. For example:

.. code-block:: c

    UNIT_StructureName structure;
    if (UNIT_FAILED(UNIT_StructureName_Init(&structure))) {
        // Handle error
    }

    /* Do something with the now-initialized structure */

    UNIT_StructureName_Clear(&structure);

``Clear`` functions do *not* zero the memory, but after they do allow for reuse;
``Init`` can be called again on the allocation and the structure will be
initialized in-place.

Now, the second type of construction is similar, but uses its own heap allocation
instead of using any supplied memory block. These functions are called
``UNIT_StructureName_New`` and ``UNIT_StructureName_Free``.

``UNIT_StructureName_New`` does not take a pointer to a ``UNIT_StructureName *``.
Instead, it allocates memory on the heap (through UNIT's internal freelist
allocator) and then initializes it. In fact, a ``New`` function is always
defined as something like this:

.. code-block:: c

    UNIT_StructureName *
    UNIT_StructureName_New(/* Any structure-specific arguments would go here */)
    {
        // In reality, this actually uses a private function called _UNIT_Alloc()
        UNIT_StructureName *structure = malloc(sizeof(UNIT_StructureName));
        if (structure == NULL) {
            return NULL;
        }

        if (UNIT_FAILED(UNIT_StructureName_Init(structure /* Structure-specific args here */))) {
            return NULL;
        }

        return structure;
    }


``UNIT_StructureName_Free`` is similar to ``UNIT_StructureName_Clear``, but it deallocates the
pointer in addition to clearing the structure. A ``Free`` function is defined as:

.. code-block:: c

    void
    UNIT_StructureName_Free(UNIT_StructureName *structure)
    {
        UNIT_StructureName_Clear(structure);
        // Again, the system allocator isn't actually used in reality.
        free(structure);
    }

Because ``New``/``Free`` use UNIT's internal allocator, it is **not** safe to
call ``UNIT_StructureName_Free`` on any heap-allocated memory. For example,
the following code is invalid:

.. code-block:: c

    UNIT_StructureName *structure = malloc(sizeof(UNIT_StructureName));
    UNIT_StructureName_Init(structure);
    UNIT_StructureName_Free(structure); // Invalid! Free expects memory from UNIT's allocator.
