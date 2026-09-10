Building a guessing game
========================

Goal
----

In the previous section, we compiled a procedure that returns 0. Now let's
turn it into an actual guessing game. We'll build it up piece by piece,
testing each step along the way.

We want the final game to look something like this:

.. code-block::

    Welcome to a guessing game!
    The number is between 1 and 100.
    Enter a guess (1-100): 50
    Higher
    Enter a guess (1-100): 75
    Lower
    Enter a guess (1-100): 62
    Higher
    Enter a guess (1-100): 68
    You win!


Hardcoded values
----------------

Before dealing with user input or random numbers, let's hardcode everything
and make sure the arithmetic works. Our procedure will store a guess of 50,
then print it.

UNIT provides local variables for storing values between instructions.
Each local is identified by an integer index. You store a value with
:c:enumerator:`UNIT_OP_STORE_LOCAL` and retrieve it with
:c:enumerator:`UNIT_OP_LOAD_LOCAL`:

.. code-block:: c

   // Local 0 = answer, Local 1 = guess

   // answer (index 0) = 42
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
   ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);

   // guess (index 1) = 50
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 50);
   ADDOP_INT(UNIT_OP_STORE_LOCAL, 1);

.. hint::

    If the idea of using indices as variable names doesn't sit right with you,
    think of it like storing a value at an index in an infinitely large array:

    .. code-block:: c

        // variables[0] = 42
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
        ADDOP_INT(UNIT_OP_STORE_LOCAL, 0);

``STORE_LOCAL`` pops the top of the stack into the local at the given index.
``LOAD_LOCAL`` pushes the value of a local back onto the stack.

You can use any index you like -- UNIT allocates storage automatically.
In fact, the local variable ID isn't really an index at all! The following
is perfectly valid and does not use exorbitant amounts of memory:

.. code-block:: c

   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
   ADDOP_INT(UNIT_OP_STORE_LOCAL, 100000000);

UNIT maintains a mapping of your local variable IDs to real ones used during
translation, so it's perfectly fine to do this.

However, keeping track of which index means what gets tedious quickly.
UNIT provides :c:func:`UNIT_Procedure_CreateLocal` to manage this for you.
It assigns an index internally and gives you a :c:type:`UNIT_Local` handle
that you pass to :c:func:`UNIT_Procedure_AddStoreName` and
:c:func:`UNIT_Procedure_AddLoadName`:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` C

   #define NEW_LOCAL(name)                                                          \
       UNIT_Local name;                                                             \
       if (UNIT_FAILED(UNIT_Procedure_CreateLocal(&procedure, #name, &name))) {     \
           goto error;                                                              \
       }

   #define STORE_NAME(name)                                                         \
          if (UNIT_FAILED(UNIT_Procedure_AddStoreName(&procedure, name))) {         \
           goto error;                                                              \
       }

   NEW_LOCAL(answer);
   NEW_LOCAL(guess);

   // answer = 42
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
   STORE_NAME(answer);

   // guess = 50
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 50);
   STORE_NAME(guess);

This is equivalent to the raw index version, but the names make the code
self-documenting and show up in debug output from
:c:func:`UNIT_Procedure_PrintInstructions`.

We'll use named locals for the rest of the tutorial.

Now let's print the guess to make sure it works. We need to call ``printf``,
which takes a format string and the value to print. Use
:c:func:`UNIT_Procedure_AddStringLoad` for the format string and
:c:func:`UNIT_Procedure_AddCallName` for the call:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` C

    #define LOAD_STRING(value)                                                  \
        if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, value))) {     \
            goto error;                                                         \
        }

    #define LOAD_NAME(name)                                                         \
        if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, name))) {            \
           goto error;                                                              \
        }

    #define CALL_NAME(name, nargs)                                                  \
        if (UNIT_FAILED(UNIT_Procedure_AddCallName(&procedure, name, nargs))) {     \
            goto error;                                                             \
        }

    // printf("You guessed: %d\n", guess)
    LOAD_STRING("You guessed: %d\n");
    LOAD_NAME(guess);
    CALL_NAME("printf", 2);
    ADDOP(UNIT_OP_POP); // discard printf's return value

    ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
    ADDOP(UNIT_OP_RETURN_VALUE);

The :c:enumerator:`UNIT_OP_POP` after the call discards ``printf``'s return
value (the number of characters printed), because we don't need it.

Here's the full program again:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>
    #include <stdio.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            fprintf(stderr, "failed to initialize context\n");
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

        #define ADDOP_INT(op, value)                                                \
            if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
                goto error;                                                         \
            }

        #define ADDOP(op) ADDOP_INT(op, 0)

        #define NEW_LOCAL(name)                                                          \
            UNIT_Local name;                                                             \
            if (UNIT_FAILED(UNIT_Procedure_CreateLocal(&procedure, #name, &name))) {     \
                goto error;                                                              \
            }

        #define STORE_NAME(name)                                                         \
                if (UNIT_FAILED(UNIT_Procedure_AddStoreName(&procedure, name))) {        \
                goto error;                                                              \
            }

        #define LOAD_STRING(value)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, value))) {     \
                goto error;                                                         \
            }

        #define LOAD_NAME(name)                                                         \
            if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, name))) {            \
                goto error;                                                             \
            }

        #define CALL_NAME(name, nargs)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddCallName(&procedure, name, nargs))) {     \
                goto error;                                                             \
            }

        NEW_LOCAL(answer);
        NEW_LOCAL(guess);

        // answer = 42
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 42);
        STORE_NAME(answer);

        // guess = 50
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 50);
        STORE_NAME(guess);

        // printf("You guessed: %d\n", guess)
        LOAD_STRING("You guessed: %d\n");
        LOAD_NAME(guess);
        CALL_NAME("printf", 2);
        ADDOP(UNIT_OP_POP);

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

        if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
            goto error;
        }

        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            goto error;
        }

        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                               UNIT_HOST_FORMAT))) {
            UNIT_CompiledProcedure_Free(compiled);
            goto error;
        }

        printf("Wrote output.o\n");

        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    error:
        UNIT_PrintError(&context, stderr);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }

Build, compile the object file, link, and run:

.. code-block:: bash
   :caption: :iconify:`devicon-plain:bash` bash

   $ gcc main.c -lunit -o guessing_game
   $ ./guessing_game
   $ gcc output.o -o output -lc
   $ ./output
   You guessed: 50


Yay, it works!


Comparing the guess
-------------------

Now let's compare the guess to the answer and print "Higher", "Lower",
or "Correct". This requires jump labels and conditional branches.

The logic is:

.. code-block::

   if guess == answer:
       print "Correct!"
   if guess > answer:
       print "Lower"
   otherwise:
       print "Higher"

However, UNIT does not have "if" clauses like normal programming languages
do. Instead, UNIT has jumps. You can think of this like the ``goto`` statement in C.

To create a point that we can jump to, we need to set a label. For our purposes,
we're going to create three labels -- one for each branch, plus one for the end:

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` C

    #define NEW_JUMP_LABEL(name)                                                    \
        UNIT_JumpLabel *name = UNIT_Procedure_CreateJumpLabel(&procedure, #name);   \
        if (name == NULL) {                                                         \
            goto error;                                                             \
        }

    NEW_JUMP_LABEL(correct);
    NEW_JUMP_LABEL(lower);
    NEW_JUMP_LABEL(end);

A ``UNIT_JumpLabel *`` is owned by the procedure -- we are not in charge of
freeing it (it will be destroyed upon calling :c:func:`UNIT_Procedure_Clear`).

To jump to a label, UNIT provides three instructions:

1. :c:enumerator:`UNIT_OP_JUMP`, which unconditionally jumps to a label.
2. :c:enumerator:`UNIT_OP_JUMP_IF_TRUE`, which jumps to a label if "true"
   is on top of the stack (more on this in a moment).
3. :c:enumerator:`UNIT_OP_JUMP_IF_FALSE`, which jumps to a label if "false"
   is on top of the stack.

Now, we haven't talked about pushing "true" or "false" yet. UNIT has a number
of special comparison instructions that do this:

1. :c:enumerator:`UNIT_OP_COMPARE_EQUAL` (``==``)
2. :c:enumerator:`UNIT_OP_COMPARE_NOT_EQUAL` (``!=``)
3. :c:enumerator:`UNIT_OP_COMPARE_GREATER` (``>``)
4. :c:enumerator:`UNIT_OP_COMPARE_GREATER_EQUAL` (``>=``)
5. :c:enumerator:`UNIT_OP_COMPARE_LESS` (``<``)
6. :c:enumerator:`UNIT_OP_COMPARE_LESS_EQUAL` (``<=``)

Each of these instructions will pop two items off of the stack and compare them.
For our purposes of jumping to our "correct" label, we want to compare guess
and answer as equal, and then jump if the result is true.

.. code-block:: c

   #define LOAD_NAME(name)                                                  \
       if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, name))) {     \
           goto error;                                                      \
       }

    #define ADDOP_JUMP(op, label)                                           \
        if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, op, label))) {   \
            goto error;                                                     \
        }

   // if guess == answer: goto correct
   LOAD_NAME(guess);
   LOAD_NAME(answer);
   ADDOP(UNIT_OP_COMPARE_EQUAL);
   ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);


Great, but this doesn't work so far, because we haven't told UNIT where the "correct"
label is. It has nowhere to jump to!

To set a label, we have to call :c:func:`UNIT_Procedure_UseLabel`. A label
can only be "used" (or "placed") once. When we jump to the label, execution
will continue from the next instruction after it. So, let's adjust our code
to add labels where need them. While we're here, let's also add the other
comparisons necessary for our guessing game:

.. code-block:: c

   #define USE_LABEL(label)                                             \
       if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, label))) {   \
           goto error;                                                  \
       }

   // if guess == answer: goto correct
   LOAD_NAME(guess);
   LOAD_NAME(answer);
   ADDOP(UNIT_OP_COMPARE_EQUAL);
   ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);

   // if guess > answer: goto lower
   LOAD_NAME(guess);
   LOAD_NAME(answer);
   ADDOP(UNIT_OP_COMPARE_GREATER);
   ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, lower);

   // Print out "Higher" here

   ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "lower" block
   USE_LABEL(lower);

   // Print out "Lower" here

   ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "correct" block
   USE_LABEL(correct);

   // Correct, end game.

   USE_LABEL(end);

Great, now let's add the ``printf`` calls we want there.
We'll follow the same practice as before:

.. code-block:: c

   // if guess == answer: goto correct
   LOAD_NAME(guess);
   LOAD_NAME(answer);
   ADDOP(UNIT_OP_COMPARE_EQUAL);
   ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);

   // if guess > answer: goto lower
   LOAD_NAME(guess);
   LOAD_NAME(answer);
   ADDOP(UNIT_OP_COMPARE_GREATER);
   ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, lower);

   // printf("Higher\n!")
   LOAD_STRING("Higher\n");
   CALL_NAME("printf", 1);
   ADDOP(UNIT_OP_POP);

   ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "lower" block
   USE_LABEL(lower);

   // printf("Lower\n!")
   LOAD_STRING("Lower\n");
   CALL_NAME("printf", 1);
   ADDOP(UNIT_OP_POP);

   ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "correct" block
   USE_LABEL(correct);

   // printf("Correct!\n")
   LOAD_STRING("Correct!\n");
   CALL_NAME("printf", 1);
   ADDOP(UNIT_OP_POP);

   // return 0
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
   ADDOP(UNIT_OP_RETURN_VALUE);

   USE_LABEL(end);

Now, if we compile and run:

.. code-block:: bash

   ./guessing_game && gcc output.o -o output -lc && ./output
   Lower

Try changing the hardcoded guess to 42 and verify it prints "Correct!",
or to 30 and verify that it prints "Higher".


Random numbers and user input
-----------------------------

Now let's replace the hardcoded values with a random number and user input.

For the answer, we call ``rand()`` and compute ``(rand() % 100) + 1`` to
get a number between 1 and 100. We also need to call ``srand(time(NULL))``
to seed the random number generator.

Replace the hardcoded answer with:

.. code-block:: c

   // srand(time(NULL)) -- NULL is just 0
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
   CALL_NAME("time", 1);
   // [time_result]

   CALL_NAME("srand", 1);
   ADDOP(UNIT_OP_POP);

   // answer = (rand() % 100) + 1
   CALL_NAME("rand", 0);
   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 100);
   ADDOP(UNIT_OP_MODULO);
   // [rand() % 100]

   ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
   ADDOP(UNIT_OP_ADD);
   // [(rand() % 100) + 1]

   STORE_NAME(answer);

For the guess, we use ``scanf`` to read an integer from the user.
``scanf`` takes a format string and a pointer to the variable to store
the result in. UNIT provides :c:enumerator:`UNIT_OP_ADDRESS_OF` to get
the address of a local variable.

.. note::

   :c:enumerator:`UNIT_OP_ADDRESS_OF` takes a raw local index, not a
   :c:type:`UNIT_Local` handle. Since we used
   :c:func:`UNIT_Procedure_CreateLocal`, we can access the index through
   the ``id`` field. This is the one case where you need the raw index.

Replace the hardcoded guess with:

.. code-block:: c

   // printf("Enter your guess (1-100): ")
   LOAD_STRING("Enter your guess (1-100):");
   CALL_NAME("printf", 1);
   ADDOP(UNIT_OP_POP);

   // scanf("%d", &guess)
   LOAD_STRING("%d");
   ADDOP_INT(UNIT_OP_ADDRESS_OF, guess.id);
   CALL_NAME("scanf", 2);
   ADDOP(UNIT_OP_POP);

:c:enumerator:`UNIT_OP_ADDRESS_OF` pushes the memory address of the local
variable onto the stack. ``scanf`` writes the parsed integer directly to
that address. After the call, loading ``guess`` gives you whatever the user
typed.

Here's the full program again:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>
    #include <stdio.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            fprintf(stderr, "failed to initialize context\n");
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

        #define ADDOP_INT(op, value)                                                \
            if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
                goto error;                                                         \
            }

        #define ADDOP(op) ADDOP_INT(op, 0)

        #define NEW_LOCAL(name)                                                          \
            UNIT_Local name;                                                             \
            if (UNIT_FAILED(UNIT_Procedure_CreateLocal(&procedure, #name, &name))) {     \
                goto error;                                                              \
            }

        #define STORE_NAME(name)                                                         \
                if (UNIT_FAILED(UNIT_Procedure_AddStoreName(&procedure, name))) {        \
                goto error;                                                              \
            }

        #define LOAD_STRING(value)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, value))) {     \
                goto error;                                                         \
            }

        #define LOAD_NAME(name)                                                         \
            if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, name))) {            \
                goto error;                                                             \
            }

        #define CALL_NAME(name, nargs)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddCallName(&procedure, name, nargs))) {     \
                goto error;                                                             \
            }

        #define NEW_JUMP_LABEL(name)                                                    \
            UNIT_JumpLabel *name = UNIT_Procedure_CreateJumpLabel(&procedure, #name);   \
            if (name == NULL) {                                                         \
                goto error;                                                             \
            }

        #define ADDOP_JUMP(op, label)                                           \
            if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, op, label))) {   \
                goto error;                                                     \
            }

        #define USE_LABEL(label)                                             \
            if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, label))) {   \
                goto error;                                                  \
            }

        NEW_JUMP_LABEL(correct);
        NEW_JUMP_LABEL(lower);
        NEW_JUMP_LABEL(end);

        NEW_LOCAL(answer);
        NEW_LOCAL(guess);

        // srand(time(NULL)) -- NULL is just 0
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        CALL_NAME("time", 1);
        // [time_result]

        CALL_NAME("srand", 1);
        ADDOP(UNIT_OP_POP);

        // answer = (rand() % 100) + 1
        CALL_NAME("rand", 0);
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 100);
        ADDOP(UNIT_OP_MODULO);
        // [rand() % 100]

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
        ADDOP(UNIT_OP_ADD);
        // [(rand() % 100) + 1]

        STORE_NAME(answer);

        // printf("Enter your guess (1-100): ")
        LOAD_STRING("Enter your guess (1-100):");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        // scanf("%d", &guess)
        LOAD_STRING("%d");
        ADDOP_INT(UNIT_OP_ADDRESS_OF, guess.id);
        CALL_NAME("scanf", 2);
        ADDOP(UNIT_OP_POP);

        // if guess == answer: goto correct
        LOAD_NAME(guess);
        LOAD_NAME(answer);
        ADDOP(UNIT_OP_COMPARE_EQUAL);
        ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);

        // if guess > answer: goto lower
        LOAD_NAME(guess);
        LOAD_NAME(answer);
        ADDOP(UNIT_OP_COMPARE_GREATER);
        ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, lower);

        // printf("Higher\n!")
        LOAD_STRING("Higher\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "lower" block
        USE_LABEL(lower);

        // printf("Lower\n!")
        LOAD_STRING("Lower\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "correct" block
        USE_LABEL(correct);

        // printf("Correct!\n")
        LOAD_STRING("Correct!\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        // return 0
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(RETURN_VALUE);

        USE_LABEL(end);

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

        if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
            goto error;
        }

        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            goto error;
        }

        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                               UNIT_HOST_FORMAT))) {
            UNIT_CompiledProcedure_Free(compiled);
            goto error;
        }

        printf("Wrote output.o\n");

        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    error:
        UNIT_PrintError(&context, stderr);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }

Build and run:

.. code-block:: bash

   ./guessing_game && gcc output.o -o output -lc && ./output
   Enter your guess (1-100): 50
   Lower

Try a few values. The answer changes each time you run because of the
random seed.


The game loop
-------------

We're almost there!

The last step is to give the user more than one guess.
Let's add a loop so they can keep guessing until they get it right.

The structure is:

.. code-block::

   loop:
    if guess == answer:
        print "Correct!"
        return 0
    if guess > answer:
        print "Lower"
    otherwise:
        print "Higher"
    goto loop


We'll create a ``loop`` label and put it before the store to ``guess``,
and jump to it from our ``end`` label.

.. code-block:: c
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

   NEW_JUMP_LABEL(loop);

   /* Take input, store to guess ... */

   /* Compare the guess with the answer */

   USE_LABEL(end);
   ADDOP_JUMP(UNIT_OP_JUMP, loop);


See the complete program below for the final working guessing game.


The complete program
--------------------

Here is the full guessing game. This is a complete, working program
that you can compile and run:

.. code-block:: c
   :linenos:
   :caption: :iconify:`streamline-logos:c-language-logo-solid` main.c

    #include <unit/unit.h>
    #include <stdio.h>

    int main(void)
    {
        UNIT_Context context;
        if (UNIT_FAILED(UNIT_Context_Init(&context))) {
            fprintf(stderr, "failed to initialize context\n");
            return 1;
        }

        UNIT_Procedure procedure;
        if (UNIT_FAILED(UNIT_Procedure_Init(&procedure, &context, "main"))) {
            UNIT_PrintError(&context, stderr);
            UNIT_Context_Clear(&context);
            return 1;
        }

        #define ADDOP_INT(op, value)                                                \
            if (UNIT_FAILED(UNIT_Procedure_AddOperation(&procedure, op, value))) {  \
                goto error;                                                         \
            }

        #define ADDOP(op) ADDOP_INT(op, 0)

        #define NEW_LOCAL(name)                                                          \
            UNIT_Local name;                                                             \
            if (UNIT_FAILED(UNIT_Procedure_CreateLocal(&procedure, #name, &name))) {     \
                goto error;                                                              \
            }

        #define STORE_NAME(name)                                                         \
                if (UNIT_FAILED(UNIT_Procedure_AddStoreName(&procedure, name))) {        \
                goto error;                                                              \
            }

        #define LOAD_STRING(value)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddStringLoad(&procedure, value))) {     \
                goto error;                                                         \
            }

        #define LOAD_NAME(name)                                                         \
            if (UNIT_FAILED(UNIT_Procedure_AddLoadName(&procedure, name))) {            \
                goto error;                                                             \
            }

        #define CALL_NAME(name, nargs)                                                  \
            if (UNIT_FAILED(UNIT_Procedure_AddCallName(&procedure, name, nargs))) {     \
                goto error;                                                             \
            }

        #define NEW_JUMP_LABEL(name)                                                    \
            UNIT_JumpLabel *name = UNIT_Procedure_CreateJumpLabel(&procedure, #name);   \
            if (name == NULL) {                                                         \
                goto error;                                                             \
            }

        #define ADDOP_JUMP(op, label)                                           \
            if (UNIT_FAILED(UNIT_Procedure_AddJump(&procedure, op, label))) {   \
                goto error;                                                     \
            }

        #define USE_LABEL(label)                                             \
            if (UNIT_FAILED(UNIT_Procedure_UseLabel(&procedure, label))) {   \
                goto error;                                                  \
            }

        NEW_JUMP_LABEL(correct);
        NEW_JUMP_LABEL(lower);
        NEW_JUMP_LABEL(end);
        NEW_JUMP_LABEL(loop);

        NEW_LOCAL(answer);
        NEW_LOCAL(guess);

        // srand(time(NULL)) -- NULL is just 0
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        CALL_NAME("time", 1);
        // [time_result]

        CALL_NAME("srand", 1);
        ADDOP(UNIT_OP_POP);

        // answer = (rand() % 100) + 1
        CALL_NAME("rand", 0);
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 100);
        ADDOP(UNIT_OP_MODULO);
        // [rand() % 100]

        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 1);
        ADDOP(UNIT_OP_ADD);
        // [(rand() % 100) + 1]

        STORE_NAME(answer);

        USE_LABEL(loop);

        // printf("Enter your guess (1-100): ")
        LOAD_STRING("Enter your guess (1-100):");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        // scanf("%d", &guess)
        LOAD_STRING("%d");
        ADDOP_INT(UNIT_OP_ADDRESS_OF, guess.id);
        CALL_NAME("scanf", 2);
        ADDOP(UNIT_OP_POP);

        // if guess == answer: goto correct
        LOAD_NAME(guess);
        LOAD_NAME(answer);
        ADDOP(UNIT_OP_COMPARE_EQUAL);
        ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, correct);

        // if guess > answer: goto lower
        LOAD_NAME(guess);
        LOAD_NAME(answer);
        ADDOP(UNIT_OP_COMPARE_GREATER);
        ADDOP_JUMP(UNIT_OP_JUMP_IF_TRUE, lower);

        // printf("Higher\n!")
        LOAD_STRING("Higher\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "lower" block
        USE_LABEL(lower);

        // printf("Lower\n!")
        LOAD_STRING("Lower\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        ADDOP_JUMP(UNIT_OP_JUMP, end); // Skip past the "correct" block
        USE_LABEL(correct);

        // printf("Correct!\n")
        LOAD_STRING("Correct!\n");
        CALL_NAME("printf", 1);
        ADDOP(UNIT_OP_POP);

        // return 0
        ADDOP_INT(UNIT_OP_LOAD_INTEGER, 0);
        ADDOP(UNIT_OP_RETURN_VALUE);

        USE_LABEL(end);

        ADDOP_JUMP(UNIT_OP_JUMP, loop);

        if (UNIT_FAILED(UNIT_Procedure_Optimize(&procedure))) {
            goto error;
        }

        UNIT_CompiledProcedure *compiled = UNIT_Compile(&procedure, UNIT_HOST_PLATFORM);
        if (compiled == NULL) {
            goto error;
        }

        if (UNIT_FAILED(UNIT_CompiledProcedure_WriteObjectFile(compiled, "output.o",
                                                               UNIT_HOST_FORMAT))) {
            UNIT_CompiledProcedure_Free(compiled);
            goto error;
        }

        printf("Wrote output.o\n");

        UNIT_CompiledProcedure_Free(compiled);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 0;
    error:
        UNIT_PrintError(&context, stderr);
        UNIT_Procedure_Clear(&procedure);
        UNIT_Context_Clear(&context);
        return 1;
    }


.. code-block:: bash

   $ gcc main.c -lunit -o guessing_game
   $ ./guessing_game
   Wrote output.o
   $ gcc output.o -o output -lc
   $ ./output
   Enter your guess (1-100): 50
   Lower
   Enter your guess (1-100): 25
   Higher
   Enter your guess (1-100): 37
   Correct!


Next steps
----------

This covers the majority of UNIT's instruction set. The remaining
instructions (:c:enumerator:`UNIT_OP_READ_BYTES`,
:c:enumerator:`UNIT_OP_WRITE_BYTES`, :c:enumerator:`UNIT_OP_CONVERT`) are
used for lower-level memory manipulation -- see the
`brainfuck example <https://github.com/ZeroIntensity/unit/blob/main/examples/brainf.c>`_
for a program that uses them extensively.

For the full list of instructions, see the :ref:`opcode reference <c-opcodes>`.
