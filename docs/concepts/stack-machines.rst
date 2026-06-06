Stack Machines
==============

What is a stack machine?
------------------------

A stack machine is a type of virtual machine where instructions interact
through a stack. In contrast, CPUs are register machines, meaning they interact
through registers.


Using stack machines
--------------------

In UNIT, every instruction has zero or one integer argument. An instruction may
push or pop items off the virtual stack.

It's much easier to visualize this than to conceptualize it. So, as an example,
imagine that you would like to add two integers. In a stack machine, this looks
something like this:

.. code-block::

   PUSH 2
   PUSH 3
   ADD


After the first two ``PUSH`` instructions, the stack looks like ``[3, 2]``
(where index 0 is the top of the stack). The ``ADD`` instruction then pops
these two items off the stack and pushes the result back onto it. As a result,
the stack at the end of this is ``[5]``.


Why a stack machine?
--------------------

A stack machine has a number of upsides compared to a register machine.
To understand, let's take the same add from above and rewrite in terms of a
register machine.

As previously mentioned, register machines communicate through slots known as
registers, which simply hold values. To add two numbers in a register machine,
you might do something like this:


.. code-block::

   MOVE REGISTER_1, 2
   MOVE REGISTER_2, 3
   ADD REGISTER_1, REGISTER_2
   # The result will be stored in another register.
   # For example's sake, we'll pretend that it's in register 3, but in
   # practice, most register machines will simply write to the first argument.


The main annoyance with this is that there are finite number of CPU registers,
and some instructions want to use specific registers. This complicates code
generation when developing a compiler frontend.

Let's go to a higher level and imagine that you have an AST representing
``2 + 3``:

.. code-block:: python

    class Int:
        def __init__(self, value: int) -> None:
            self.value = value

    class Add:
        def __init__(self, left: Int, right: Int) -> None:
            self.left = left
            self.right = right

    node = Add(Int(2), Int(3))  # 2 + 3


Creating an efficient code generation method for these classes is complicated.
Your first instinct might be to assign a register value to each class, like so:

.. code-block:: python

    class Int:
        ...

        def generate_code(self) -> Iterator[InstructionType]:
            self.register = find_free_register()
            yield ("MOVE", self.register, self.value)

    class Add:
        ...

        def generate_code(self) -> Iterator[InstructionType]:
            yield from self.left.generate_code()
            yield from self.right.generate_code()
            self.register = "REGISTER_3"  # Let's assume ADD always writes to register 3
            yield ("ADD", self.left.register, self.right.register)


This works, but gets difficult very quickly. Again, a register machine has
a finite number of registers, whereas the stack of a stack machine can be
practically infinite.

For example, imagine if ``find_free_register`` in the above code couldn't
find a free register, such as if all the registers are storing local variables.
In this case, you need to spill the result to the stack. But, for example's sake,
let's pretend that our ``ADD`` instruction can only operate on registers.
So, you need to push the current state of a register onto the stack, store the necessary
value into the register, use it in ``ADD``, and then restore the state of the register.
We could hack it together like this:

.. code-block:: python

    class Int:
        ...

        def generate_code(self) -> Iterator[InstructionType]:
            self.register = find_free_register()
            if self.register is None:
                self.register = pop_register()
                self.spilled = True
                yield ("PUSH", self.register)
            yield ("MOVE", self.register, self.value)

        def generate_code_after(self) -> Iterator[InstructionType]:
            # Restore the old state of the register if we had to steal it
            if self.spilled is True:
                yield ("POP", self.register)

    class Add:
        ...

        def generate_code(self) -> Iterator[InstructionType]:
            yield from self.left.generate_code()
            yield from self.right.generate_code()
            self.register = "REGISTER_3"  # Let's assume ADD always writes to register 3
            if self.register in get_used_registers():
                self.spilled = True
                yield ("PUSH", self.register)
            yield ("ADD", self.left.register, self.right.register)
            yield from self.left.generate_code_after()
            yield from self.right.generate_code_after()

        def generate_code_after(self) -> Iterator[InstructionType]:
            if self.spilled is True:
                yield ("POP", self.register)


Notice how every node now needs to track cleanup state and every parent needs
to call it -- this complexity multiplies with every new node type!

But, also note that we applied this behavior to the generic ``Int`` class; if we
wanted to use instructions that *didn't* require a register, we'd be doing needless
register spilling instead of just using the stack.

It's these kinds of problems that make register machines hard to work with, and
is one of the many reasons that developers will often use backend libraries
that have already solved these problems, such as `LLVM <https://llvm.org/>`_,
`QBE <https://c9x.me/compile/>`_, and `Cranelift <https://cranelift.dev/>`_.

But, if we wanted to generate stack machine code for this, then it's easy:

.. code-block:: python

    class Int:
        ...

        def generate_code(self) -> Iterator[tuple[str, int]]:
            yield ("PUSH", self.value)

    class Add:
        ...

        def generate_code(self) -> Iterator[str]:
            yield from self.left.generate_code()
            yield from self.right.generate_code()
            yield "ADD"


That's it! No register allocation and no spilling. The downside of this is
that your CPU isn't a stack machine, so if you're compiling to machine code,
you'll usually have to deal with the difficulties of register machines anyway.
This is why many interpreted languages use a stack-based interpreter for their
primary evaluation loop. For example, the Python interpreter and the JVM are
both stack machines.


.. seealso::

   `Stack machine -- Wikipedia <https://en.wikipedia.org/wiki/Stack_machine>`_
