# UNIT

This is a work-in-progress!

## What is UNIT?

UNIT stands for "Unified Native Instruction Translator".
It is a compiler backend library that enables developers to generate machine
code from a stack-based IR.

Currently, it supports:

- Compiling to x86-64.
- Writing ELF object files.
- Some very simple optimization passes.

UNIT is in the early stages of development; see below for UNIT's limitations.

## Installation

### C/C++ Library

This installs the C library (complete with C++ bindings) onto your system:

```bash
git clone https://github.com/ZeroIntensity/unit && cd unit
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Python Library

This installs the PyPI package into your local Python environment:

```bash
pip install --pre -i https://test.pypi.org/simple/ unit-compiler
```

Alternatively, you can build it from source:

```bash
git clone https://github.com/ZeroIntensity/unit && cd unit
pip install .
```

UNIT's Python bindings have no dependencies, but it is recommended to
use a virtual environment nonetheless.

## Examples

### C Example

```c
#include <unit/unit.h>

int main(void)
{
    UNIT_Context ctx;
    UNIT_Context_Init(&ctx);

    UNIT_Procedure proc;
    UNIT_Procedure_Init(&proc, &ctx, "add");

    // int add(int a, int b) { return a + b; }
    UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 0);
    UNIT_Procedure_AddOperation(&proc, UNIT_OP_LOAD_ARGUMENT, 1);
    UNIT_Procedure_AddOperation(&proc, UNIT_OP_ADD, 0);
    UNIT_Procedure_AddOperation(&proc, UNIT_OP_RETURN_VALUE, 0);

    // Optimize and compile
    UNIT_Procedure_Optimize(&proc);
    UNIT_CompiledProcedure *compiled = UNIT_Compile(&proc, UNIT_HOST_PLATFORM);

    // Write to object file
    UNIT_CompiledProcedure_WriteObjectFile(compiled, "add.o",
                                           UNIT_FORMAT_ELF);

    // Or JIT and call directly
    UNIT_ExecutableBuffer *buf = UNIT_CompiledProcedure_JIT(compiled, NULL);
    int64_t (*add_two)(int64_t, int64_t) =
        (int64_t (*)(int64_t, int64_t))UNIT_ExecutableBuffer_GetPointer(buf);

    printf("%ld\n", add_two(3, 4)); // 7

    UNIT_ExecutableBuffer_Free(buf);
    UNIT_CompiledProcedure_Free(compiled);
    UNIT_Procedure_Clear(&proc);
    UNIT_Context_Clear(&ctx);
    return 0;
}
```

### C++ Example

```cpp
#include <cstdio>
#include <unit/unit.hpp>

int main()
{
    unit::Context ctx;
    unit::Procedure proc(ctx, "add");

    proc.load_argument(0);
    proc.load_argument(1);
    proc.add();
    proc.return_value();

    proc.optimize();
    auto compiled = proc.compile(unit::Platform::host());
    auto add = compiled.jit<int64_t(*)(int64_t, int64_t)>();

    printf("%ld\n", add(3, 4)); // 7
    return 0;
}
```

### Python Example

```py
import unit

proc = unit.Procedure("add")

proc.load_argument(0)
proc.load_argument(1)
proc.add()
proc.return_value()

proc.optimize()
compiled = proc.compile()
add = compiled.jit()

print(add(3, 4))  # 7
```

## Limitations and roadmap

UNIT is missing support for the following features:

- Compiling to AArch64.
- Writing object files in the PE/COFF (Windows) or Mach-O (macOS) format.
- Floating point operations.
- SSA (this is currently partial; locations are assigned once per block, but
  not once per procedure).

## Should I use UNIT?

UNIT is **not yet suitable for production**.
By using UNIT, you can expect API breakages, slow machine code, and a lack of platform support.
However, you are encouraged to experiment with the compiler to find bugs or
other areas for improvement.

## Copyright

UNIT is distributed under the terms of the [MIT](https://spdx.org/licenses/MIT.html) license.
