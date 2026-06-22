.. _translation-pipeline:

The Translation Pipeline
========================

.. note::

   If you're unfamiliar with the differences between stack machines and
   register machines, see the :ref:`stack-machines` section.

The basics
----------

Conceptually, UNIT is a library to generate machine code from stack-based IR.
But, CPUs are register machines -- how does UNIT convert stack machine code
to register machine code?

In a stack machine, an instruction with a result pushes onto the stack, and
instructions operands are popped off the stack.
UNIT translates this to register machine code by defining instructions
with results as writing to a destination, and instructions that
consume arguments off the stack use these destinations as operands.

In general, there are three types of destinations:

1. Constant values.
2. Register values.
3. Stack-allocated values.

For example, let's take the following UNIT stack machine instructions and
translate them to register machine instructions.

.. code-block::
   :caption: :iconify:`ri:stack-fill` Stack machine

   LOAD_INTEGER 1
   LOAD_INTEGER 2
   ADD

After the first two instructions, the stack is ``[1, 2]``. ``1`` and ``2``
are constant values, so ``ADD`` will use them directly as operands. However,
``ADD`` is not considered constant (because constant folding happens during a
later optimization pass instead of during translation), so it has to write to
a register.

So, UNIT represents the above as a single ``ADD`` instruction with two
operands:

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   location_1 = ADD 1, 2

Now, let's extend this to use the result of ``ADD`` so we can see how
``location_1`` flows to the next instruction:

.. code-block::
   :caption: :iconify:`ri:stack-fill` Stack machine

   LOAD_INTEGER 1
   LOAD_INTEGER 2
   ADD
   LOAD_INTEGER 3
   MULTIPLY

``ADD`` writes to ``location_0``, so ``MULTIPLY`` will use that as one of its
operands.

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   location_0 = ADD 1, 2
   location_1 = MULTIPLY location_0, 3


Special destination types
-------------------------

In some cases, there isn't any clean way to represent register machine IR
from stack machine IR, at least based on the rules we defined above.
For example, how do we manage calls? In our register machine IR, we can
only have a few operands to a single instruction, so we can't translate
cleanly:

.. code-block::
   :caption: :iconify:`ri:stack-fill` Stack machine

   LOAD_INTEGER 1
   LOAD_INTEGER 2
   LOAD_INTEGER 3
   PREPARE_CALL 3  # This is a special instruction for calls
   CALL_NAME add

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   # We can't fit all three arguments into a three-operand instruction,
   # because one of the operands has to contain the name!
   location_0 = CALL_NAME add, 1, 2


For calls, UNIT solves this by introducing a special "call arguments" type,
which is emitted by the ``PREPARE_CALL`` instruction. So, the real translation
for the above is as follows:

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   location_0 = CALL_NAME add, [1, 2, 3]

Cases like these are eventually resolved during compilation. We won't cover all
of the special cases in UNIT's translation step, but if you're confused about
how UNIT translates something, it may be done using a special case like this.


Register allocation
-------------------

So far, we've only used "locations" instead of actual registers. But,
per their name, register machines operate on registers, not locations.
So, after a program has been completely translated, locations have to be converted
into register values. But, in a physical register machine (such as your CPU), there
are a finite number of registers, so we have to reuse registers, and if we run
out of registers, we have to "spill" some values onto the stack. This process is
called "register allocation".

In small programs, register allocation is easy. For now, let's assume there are
8 registers available. Since we only have two locations, we can just assign them
to the first and second register:

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   register_0 = ADD 1, 2
   register_1 = MULTIPLY register_0, 3

For a more complicated example, let's add a ``SUBTRACT`` instruction:

.. code-block::
   :caption: :iconify:`ri:stack-fill` Stack machine

   LOAD_INTEGER 1
   LOAD_INTEGER 2
   ADD
   LOAD_INTEGER 3
   MULTIPLY
   LOAD_INTEGER 4
   SUBTRACT

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   register_0 = ADD 1, 2
   register_1 = MULTIPLY register_0, 3
   location_2 = SUBTRACT register_1, 4

For example's sake, let's limit ourselves to just two registers. This means
that we can't simply assign ``location_2`` to a third register. So what do we do?
In this case, the register allocator can look at the lifetimes of each location.
After a location's last reference, it can be considered "dead", and thus its
register can be reused.

To visualize:

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   location_0 = ADD 1, 2
   location_1 = MULTIPLY location_0, 3
   # location_0 is not used after this point, so its register can be used for
   # something else.
   location_2 = SUBTRACT location_1, 4


.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   register_0 = ADD 1, 2
   register_1 = MULTIPLY register_0, 3
   # The value from ADD is lost here, but that's okay because we know it won't
   # be used again.
   register_0 = SUBTRACT register_1, 4


But, what happens if we do want to use ``location_0`` again later? Or, more
generally, what if we need to assign a location and there aren't any registers
to reuse?

In this scenario, we need to store the value into memory by pushing it onto
the stack. This is called "spilling". For our purposes, the stack is practically
infinite, at the cost of being slower to access. This is why we try to store
values into registers instead of always using the stack.

To demonstrate, let's limit our example to just a single register:

.. code-block::
   :caption: :iconify:`ri:cpu-line` Register machine

   register_0 = ADD 1, 2
   stack_slot_0 = MULTIPLY register_0, 3
   register_0 = SUBTRACT stack_slot_0, 4

We repeat this process of assigning, reusing, and spilling for every location
in the translation until there are no locations left. At this point, the
translation is ready for compilation.


Compiling to machine code
-------------------------

After translation is complete and all registers have been assigned, we're ready
to emit real machine code. Internally, UNIT has two parts to writing machine code:

1. IR "lowering" -- the logic to convert UNIT's register machine IR into
   true instructions for the target architecture and ABI.
2. Encoding -- writing the instructions as executable text that the CPU
   can understand.

We won't go too much into encoding, but the lowering process does have a few
hidden details that make the resulting machine code look a bit different from
the IR. For example, on AMD64, not all instructions support operating on
stack slots, so UNIT has to move the value into a "scratch" register and then
operate on it. (The scratch register is not allowed to be assigned to a location
during register allocation, so it's always available for use in IR lowering.)

The result is a sequence of bytes that can be written to an ELF object file
or executed directly via JIT compilation.
