import sys

try:
    import unit
except ModuleNotFoundError as e:
    e.add_note(
        "UNIT's Python bindings have not been built and/or installed. "
        "To install them, run 'pip install .' in the root directory.",
    )
    raise


def process_line() -> None:
    try:
        line = input("> ")
    except (EOFError, KeyboardInterrupt):
        sys.exit(0)

    if line in {"exit", "quit"}:
        sys.exit(0)

    parts = line.split(" ")
    if len(parts) != 3:
        print("Invalid syntax. Usage: <number> <op> <number>", file=sys.stderr)
        return

    left, op, right = parts

    try:
        left = int(left)
        right = int(right)
    except ValueError:
        print("Invalid number(s)", file=sys.stderr)
        return

    procedure = unit.Procedure("expr")
    procedure.load_integer(left)
    procedure.load_integer(right)

    if op == "+":
        procedure.add()
    elif op == "-":
        procedure.subtract()
    elif op == "*":
        procedure.multiply()
    elif op == "/":
        procedure.divide()
    elif op == "%":
        procedure.modulo()
    else:
        print(
            f"Unknown operator: {op}. "
            "Valid operators are: +, -, *, /, %",
            file=sys.stderr
        )
        return

    procedure.return_value()
    compiled = procedure.compile()
    print(compiled.translation_text())
    executable = compiled.jit()
    print(executable())


def main() -> None:
    print("REPL calculator")
    print("Ex: 1 + 2, 8 * 4")

    while True:
        process_line()


if __name__ == "__main__":
    main()
