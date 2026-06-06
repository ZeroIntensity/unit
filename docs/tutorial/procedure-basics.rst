The basics of procedures
========================

What is a "procedure"?
----------------------

In UNIT, a procedure is referring to the :c:type:`UNIT_Procedure` type, which
holds instructions that will execute at runtime; or, in other words, a procedure
is a function that we'll be generating code for.

.. hint::

   We use the term "procedure" instead of "function" to distinguish between
   things that aren't compiled by UNIT. A procedure is created by you and
   contains UNIT's instructions, whereas a function could be something in
   the C standard library.


Creating the context
--------------------

Before we can emit any instructions, we need to initialize a procedure.
This can be done through :c:func:`UNIT_Context_Init` or :c:func:`UNIT_Procedure_New`.
But, if you look at the signature of those functions, they take a ``UNIT_Context *``.
So, before we can create a procedure, we have to create a new context.

Assuming everything has been installed correctly,
UNIT's primary header file should exist at ``$INCLUDE_PATH/unit/unit.h``. If we
``#include`` that, then everything in UNIT's public C API will become available
in the namespace.

So, using our prior knowledge about how we initialize structures
(``Init`` and ``New`` functions), let's create a new context:

.. code-block:: c

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Context_Clear(&context);
        return 0;
    }

.. admonition:: When to use ``Init`` instead of ``New``?
   :class: hint

   The primary difference between these two functions is what kind of allocation
   the structure is stored on. ``New`` explicitly uses the heap, so only use
   it when you actually need the heap; in other words, if you know that a structure
   will outlive the current function, then use ``New``. Otherwise, use ``Init``.

   In this case, we use ``Init`` because we don't need the context to outlive the
   ``main`` function.


Before proceeding, let's make sure this works. For this tutorial, we'll be
using GCC, but you can use whatever compiler you want, as long as it can
find UNIT's header files and link against it.

So, let's save the above as ``main.c`` and run it:

.. code-block:: bash

   gcc main.c -o out -lunit
   ./out


Creating a procedure
--------------------

At this point, we have a context that we can use, so we can now create our
first procedure!

Following UNIT's naming convention, procedures can be created through
:c:func:`UNIT_Procedure_Init` and :c:func:`UNIT_Procedure_New`. Since we're
running everything inside the ``main`` function, we'll use ``Init`` with stack
memory:

.. code-block:: c
   :emphasize-lines: 10-15

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            return 1;
        }

        UNIT_Procedure_Clear(&procedure)
        UNIT_Context_Clear(&context);
        return 0;
    }


But wait, there's a bug here! Even when ``UNIT_Procedure_Init`` fails, the
context is still alive. Remember, we always need to call ``UNIT_Context_Clear``
after ``UNIT_Context_Init`` was successful, otherwise our program will leak memory.

In addition, ``UNIT_Procedure_Init`` sets an error message. For our own sanity
as developers, let's put a ``UNIT_PrintError`` before returning so we have
some idea of what went wrong if something were to fail:

.. code-block:: c
   :emphasize-lines: 12-13

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

        UNIT_Procedure_Clear(&procedure)
        UNIT_Context_Clear(&context);
        return 0;
    }


Emitting instructions
---------------------

Now that we have all the boilerplate code out of the way, we can start emitting
instructions!

UNIT uses a stack-based IR, or in other words, you write code for a
:ref:`stack machine <stack-machines>`, and then UNIT will translate it
to machine code.

.. caution::

   Be careful to not confuse UNIT's operand stack with the stack present
   in CPUs. The term "stack" in "stack-allocated variable" does *not* mean
   the same thing as "stack" in "operand stack".

All instructions in UNIT have two common components of a stack-based
instruction set:

1. The operation ID, often shortened to "opcode".
2. The operation argument, often shortened to "oparg".

Let's start with the operation ID and ignore the argument for now. In UNIT, all
instructions are available in an enum called :c:enumerator:`UNIT_Instruction`.
The values of this enum are prefixed with ``UNIT_OP_``. But, how do we actually
add instructions to the procedure?

Instructions can be added using a few functions under the ``UNIT_Procedure``
namespace, but for now, let's focus on :c:func:`UNIT_Procedure_AddOperation`,
which takes an opcode and oparg as an integer. If we wanted to make a program
that simply did ``return 0``, we need two instructions:

1. :c:enum:`UNIT_OP_LOAD_INTEGER`, which pushes a constant integer onto the
   operand stack.
2. :c:enum:`UNIT_OP_RETURN_VALUE`, which pops a value off the operand
   stack and returns it to the caller.

For ``LOAD_INTEGER``, we need an oparg. This is the value that will be pushed
onto the stack. In our case, this will be ``0``. For ``RETURN_VALUE``, we don't
need an oparg, so we can put any value we want for the oparg. We'll just stay
simple and pass ``0``.

Now, if we apply this to our code:

.. code-block:: c
   :emphasize-lines: 17-29

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, UNIT_OP_LOAD_INTEGER, 0))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Procedure_Clear(&procedure);
            UNIT_Context_Clear(&context);
            return 1;
        }

        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, UNIT_OP_RETURN_VALUE, 0))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Procedure_Clear(&procedure);
            UNIT_Context_Clear(&context);
            return 1;
        }

        UNIT_Procedure_Clear(&procedure)
        UNIT_Context_Clear(&context);
        return 0;
    }


But, this is really ugly. The error handling gets out of control very quickly,
and this will only get worse as we add more instructions.
We can clean this up by adding some macros tailored to our function, like so:


.. code-block:: c
   :emphasize-lines: 17-28

    #include <unit/unit.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&procedure);
            return 1;
        }

    #define ADDOP_INT(op, value)                                                \
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
            UNIT_PrintError(&context, stderr);                                  \
            UNIT_Procedure_Clear(&context);                                     \
            UNIT_Context_Clear(&procedure);                                     \
            return 1;                                                           \
        }

    #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

    #undef ADDOP_INT
    #undef ADDOP

        UNIT_Procedure_Clear(&procedure)
        UNIT_Context_Clear(&context);
        return 0;
    }

.. warning::

   Control flow in macros is a common source of bugs. Handle this with care.


The macro gymnastics aren't super pretty, but this will help us a lot as
we add more instructions.

.. tip::

   Another way to consolidate error handling code is to add a label above
   the error handling code, and then jump to it with ``goto`` upon failure.
   For example:

   .. code-block:: c

        #include <unit/unit.h>

        int main(void)
        {
            /* ... */
            if (UNIT_FAILED(UNIT_Procedure_AddOperation(&operation, UNIT_OP_LOAD_INTEGER, 42))) {
                goto error;
            }
            /* ... */

            UNIT_Procedure_Clear(&procedure)
            UNIT_Context_Clear(&context);
            return 0;
        error:
            UNIT_Procedure_Clear(&procedure)
            UNIT_Context_Clear(&context);
            return 1;
        }

Okay, let's now try to compile and run our program.

.. code-block:: bash

   gcc main.c -o out -lunit
   ./out
   echo $?
   0

Return code 0 -- that makes sense. Let's now try to return 1, just to confirm
our code is running:

.. code-block:: c
   :emphasize-lines: 14

    int main(void) {
        /* ... */

    #define ADDOP_INT(op, value)                                                \
        if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
            UNIT_PrintError(&context, stderr);                                  \
            UNIT_Procedure_Clear(&context);                                     \
            UNIT_Context_Clear(&procedure);                                     \
            return 1;                                                           \
        }

    #define ADDOP(op) ADDOP_INT(op, 0)

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
        ADDOP(UNIT_OP_RETURN_VALUE);

    #undef ADDOP_INT
    #undef ADDOP

        /* ... */
    }

.. code-block:: bash

   gcc main.c -o out -lunit
   ./out
   echo $?
   0

Huh? 0 again? What's going on?

In the above code, all did was *create* the procedure -- not actually compile
or execute it. Forgetting to actually run the code can be a common mistake
when developing a code generator.

So, how do we actually run our instructions? We'll talk about that next.
