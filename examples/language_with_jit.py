from collections.abc import Iterable, Iterator, Sequence
import ctypes
from typing import Any, ClassVar, Literal, overload
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from queue import LifoQueue
import io
from enum import Enum, auto
import operator as py_operators
import unit
import contextvars

class OpCode(Enum):
    LOAD_CONST = auto()
    LOAD_NAME = auto()
    STORE_NAME = auto()
    PRINT = auto()
    STORE_FUNCTION = auto()
    LOAD_FUNCTION = auto()
    POP_TOP = auto()
    CALL = auto()
    RETURN = auto()
    JUMP = auto()
    JUMP_IF_TRUE = auto()
    BINARY_OP = auto()
    COMPARE = auto()


@dataclass(frozen=True, slots=True)
class Instruction:
    opcode: OpCode
    oparg: Any = None


@dataclass(slots=True)
class JumpLabel:
    name: str
    index: int | None = field(init=False, default=None)


type Code = Instruction | JumpLabel


class ASTNode(ABC):
    @abstractmethod
    def codegen(self) -> Iterator[Code]: ...

    @abstractmethod
    def codegen_unit(self, procedure: unit.Procedure) -> None: ...


class Expression(ASTNode):
    @abstractmethod
    def codegen(self) -> Iterator[Instruction]: ...


@dataclass(slots=True)
class Constant[T](Expression):
    value: T

    def codegen(self) -> Iterator[Instruction]:
        yield Instruction(OpCode.LOAD_CONST, self.value)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        if isinstance(self.value, int):
            procedure.load_integer(self.value)
        else:
            assert isinstance(self.value, str)
            procedure.load_string(self.value)


@dataclass(slots=True)
class FunctionCall(Expression):
    name: str
    args: Sequence[Expression]

    def codegen(self) -> Iterator[Instruction]:
        yield Instruction(OpCode.LOAD_FUNCTION, self.name)
        for argument in self.args:
            yield from argument.codegen()

        yield Instruction(OpCode.CALL, len(self.args))

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        interpreter = Interpreter.current()
        function = Function.current()

        for argument in self.args:
            argument.codegen_unit(procedure)

        trampoline = interpreter.get_trampoline(self.name)
        function.trampolines[self.name] = trampoline

        procedure.call_name(self.name, len(self.args))



@dataclass(slots=True)
class BinaryOperator(Expression):
    left: Expression
    right: Expression
    operator: str

    def codegen(self) -> Iterator[Instruction]:
        yield from self.left.codegen()
        yield from self.right.codegen()
        yield Instruction(OpCode.BINARY_OP, self.operator)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        operators = {
            '+': procedure.add,
            '-': procedure.subtract,
            '*': procedure.multiply,
            '/': procedure.divide,
        }

        self.left.codegen_unit(procedure)
        self.right.codegen_unit(procedure)
        operators[self.operator]()


@dataclass(slots=True)
class Compare(Expression):
    left: Expression
    right: Expression
    operator: str

    def codegen(self) -> Iterator[Instruction]:
        yield from self.left.codegen()
        yield from self.right.codegen()
        yield Instruction(OpCode.COMPARE, self.operator)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        operators = {
            '==': procedure.compare_equal,
            '!=': procedure.compare_not_equal,
            '>': procedure.compare_greater,
            '>=': procedure.compare_greater_equal,
            '<': procedure.compare_less,
            '<=': procedure.compare_less_equal
        }

        self.left.codegen_unit(procedure)
        self.right.codegen_unit(procedure)
        operators[self.operator]()


@dataclass(slots=True)
class Name(Expression):
    name: str

    def codegen(self) -> Iterator[Instruction]:
        yield Instruction(OpCode.LOAD_NAME, self.name)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        function = Function.current()
        id = function.names.get_name(self.name)
        procedure.load_local(id)


class Statement(ASTNode):
    pass


@dataclass(slots=True)
class Body(ASTNode):
    statements: Iterable[Statement]

    def codegen(self) -> Iterator[Code]:
        for statement in self.statements:
            yield from statement.codegen()

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        for statement in self.statements:
            statement.codegen_unit(procedure)


@dataclass(slots=True)
class InlineExpression(Statement):
    expression: Expression

    def codegen(self) -> Iterator[Instruction]:
        yield from self.expression.codegen()
        yield Instruction(OpCode.POP_TOP)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        self.expression.codegen_unit(procedure)
        procedure.pop()


@dataclass(slots=True)
class Let(Statement):
    name: str
    value: Expression

    def codegen(self) -> Iterator[Instruction]:
        yield from self.value.codegen()
        yield Instruction(OpCode.STORE_NAME, self.name)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        function = Function.current()
        id = function.names.set_name(self.name)
        procedure.store_local(id)


@dataclass(slots=True)
class LocalNameManager:
    names: dict[str, int] = field(default_factory=dict, init=False)
    next_name_id: int = field(default=0, init=False)

    def set_name(self, name: str) -> int:
        if name in self.names:
            return self.names[name]

        id = self.next_name_id
        self.next_name_id += 1
        self.names[name] = id
        return id

    def get_name(self, name: str) -> int:
        return self.names[name]


@dataclass(slots=True)
class Function:
    name: str
    parameters: Sequence[str]
    body: Iterable[Code]
    original: FunctionDefinition
    observed_args: dict[tuple[Any, ...], int] = field(default_factory=dict, init=False)
    specialized: dict[tuple[Any, ...], unit.procedure.ExecutableBuffer] = field(default_factory=dict, init=False)
    trampolines: dict[str, ctypes._CFunctionType] = field(default_factory=dict, init=False)
    names: LocalNameManager = field(default_factory=LocalNameManager, init=False)

    CURRENT_FUNCTION: ClassVar[contextvars.ContextVar] = contextvars.ContextVar("CURRENT_FUNCTION")

    @classmethod
    def current(cls) -> Function:
        return cls.CURRENT_FUNCTION.get()

    def specialized_name(self, args: tuple[Any, ...]) -> str:
        return f"{self.name}${'_'.join(str(arg) for arg in args)}"

    def specialize(self, arguments: tuple[Any, ...]) -> unit.ExecutableBuffer:
        procedure = unit.Procedure(self.specialized_name(arguments))
        for name, value in zip(self.parameters, arguments):
            Constant(value).codegen_unit(procedure)
            id = self.names.set_name(name)
            procedure.store_local(id)

        with self.CURRENT_FUNCTION.set(self):
            self.original.body.codegen_unit(procedure)

        procedure.optimize()
        compiled = procedure.compile()
        print(f"SPECIALIZED {self.name}{arguments}:")
        print(compiled.translation_text())

        trampoline_addresses: dict[str, int] = {}
        for name, trampoline in self.trampolines.items():
            address = ctypes.cast(trampoline, ctypes.c_void_p).value
            assert address is not None
            trampoline_addresses[name] = address

        executable = compiled.jit(extra_symbols=trampoline_addresses)

        self.specialized[arguments] = executable
        return executable


@dataclass(slots=True)
class FunctionDefinition(Statement):
    name: str
    parameters: Sequence[str]
    body: Body

    def codegen(self) -> Iterator[Code]:
        code: list[Code] = []
        for instruction in self.body.codegen():
            code.append(instruction)

        function = Function(self.name, self.parameters, code, self)
        yield Instruction(OpCode.STORE_FUNCTION, function)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        # Nothing to generate here
        pass


@dataclass(slots=True)
class Print(Statement):
    value: Expression

    def codegen(self) -> Iterator[Instruction]:
        yield from self.value.codegen()
        yield Instruction(OpCode.PRINT)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        procedure.load_string("%d")
        self.value.codegen_unit(procedure)
        procedure.call_name("printf", 2)


@dataclass(slots=True)
class Return(Statement):
    value: Expression

    def codegen(self) -> Iterator[Instruction]:
        yield from self.value.codegen()
        yield Instruction(OpCode.RETURN)

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        self.value.codegen_unit(procedure)
        procedure.return_value()


@dataclass(slots=True)
class If(Statement):
    condition: Expression
    if_true: Body
    if_false: Body | None

    def codegen(self) -> Iterator[Code]:
        if_true = JumpLabel("if_true")
        end = JumpLabel("end")
        yield from self.condition.codegen()
        yield Instruction(OpCode.JUMP_IF_TRUE, if_true)

        if self.if_false is not None:
            yield from self.if_false.codegen()

        yield Instruction(OpCode.JUMP, end)

        yield if_true
        yield from self.if_true.codegen()
        yield end

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        if_true = procedure.create_jump_label("if_true")
        end = procedure.create_jump_label("end")

        self.condition.codegen_unit(procedure)
        procedure.jump_if_true(if_true)

        if self.if_false is not None:
            self.if_false.codegen_unit(procedure)

        procedure.jump_to(end)

        procedure.use_label(if_true)
        self.if_true.codegen_unit(procedure)
        procedure.use_label(end)


@dataclass(slots=True)
class Module:
    body: Iterable[Statement]

    def codegen(self) -> Iterator[Code]:
        for statement in self.body:
            yield from statement.codegen()

    def codegen_unit(self, procedure: unit.Procedure) -> None:
        for statement in self.body:
            statement.codegen_unit(procedure)


def tokenize(source: str) -> Iterator[str]:
    for line in source.split("\n"):
        if line == "":
            continue

        buffer = io.StringIO()
        for character in line:
            if character in {"'", '"', "(", ")", "=", ",", " ", "+", "-", "*", "/"}:
                result = buffer.getvalue()
                if result != "":
                    yield result
                    buffer = io.StringIO()

                if character != " ":
                    yield character
            else:
                buffer.write(character)

        if buffer.getvalue() != "":
            yield buffer.getvalue()


class Parser:
    def __init__(self, source: str) -> None:
        self.tokens = tokenize(source)
        self._token_stack = LifoQueue()

    def push_token(self, value: str) -> None:
        self._token_stack.put(value)

    @overload
    def expect_token(
        self, value: str | None = None, *, allow_empty: Literal[False] = False
    ) -> str:
        pass

    @overload
    def expect_token(
        self, value: str | None = None, *, allow_empty: Literal[True] = True
    ) -> str | None:
        pass

    def expect_token(
        self, value: str | None = None, *, allow_empty: bool = False
    ) -> str | None:
        if not self._token_stack.empty():
            return self._token_stack.get()

        try:
            token = next(self.tokens)
        except StopIteration:
            if allow_empty is True:
                return None

            raise SyntaxError("Unexpected end of input") from None

        if value is not None:
            if token != value:
                raise SyntaxError(f"Expected {value!r}, not {token!r}")

        return token

    def pop_token_if_matches(
        self, *values: str, allow_empty: bool = False
    ) -> str | None:
        token = self.expect_token(allow_empty=allow_empty)

        if token in values:
            return token

        if token is not None:
            self.push_token(token)

        return None

    def parse_single_quote_str(self) -> Constant[str]:
        value = self.expect_token()
        self.expect_token("'")
        return Constant(value)

    def parse_double_quote_str(self) -> Constant[str]:
        value = self.expect_token()
        self.expect_token('"')
        return Constant(value)

    def parse_call(self, name: str) -> FunctionCall:
        arguments: list[Expression] = []

        while True:
            if self.pop_token_if_matches(")"):
                return FunctionCall(name, arguments)

            argument = self.parse_expr()
            arguments.append(argument)

            self.pop_token_if_matches(",")

    def parse_inner_expr(self) -> Expression:
        token = self.expect_token()
        if token.isdigit():
            return Constant[int](int(token))
        elif token == "'":
            return self.parse_single_quote_str()
        elif token == '"':
            return self.parse_double_quote_str()
        else:
            if self.pop_token_if_matches("("):
                return self.parse_call(token)
            return Name(token)

    def parse_expr(self) -> Expression:
        first = self.parse_inner_expr()

        if operator := self.pop_token_if_matches("+", "-", "*", "/", allow_empty=True):
            second = self.parse_expr()
            return BinaryOperator(first, second, operator)

        if first_cmp := self.pop_token_if_matches("!", "=", "<", ">", allow_empty=True):
            if second_cmp := self.pop_token_if_matches("="):
                full = first_cmp + second_cmp
                second = self.parse_expr()
                return Compare(first, second, full)

            if first_cmp in {"!", "="}:
                self.push_token(first_cmp)
                return first
            second = self.parse_expr()

            return Compare(first, second, first_cmp)

        return first

    def parse_let(self) -> Let:
        name = self.expect_token()
        self.expect_token("=")
        value = self.parse_expr()
        return Let(name, value)

    def parse_body(self) -> Body:
        body: list[Statement] = []
        self.expect_token("{")
        while True:
            if self.pop_token_if_matches("}"):
                break

            statement = self.parse_statement()
            body.append(statement)

        return Body(body)

    def parse_function_definition(self) -> FunctionDefinition:
        name = self.expect_token()
        self.expect_token("(")
        parameters: list[str] = []

        while True:
            token = self.expect_token()
            if token == ")":
                break

            parameters.append(token)
            self.pop_token_if_matches(",")

        return FunctionDefinition(name, parameters, self.parse_body())

    def parse_print(self) -> Print:
        return Print(self.parse_expr())

    def parse_return(self) -> Return:
        return Return(self.parse_expr())

    def parse_if(self) -> If:
        condition = self.parse_expr()
        if_true = self.parse_body()

        if_false = None
        if self.pop_token_if_matches("else", allow_empty=True):
            if_false = self.parse_body()

        return If(condition, if_true, if_false)

    def parse_statement(self) -> Statement:
        token = self.expect_token()

        if token == "let":
            return self.parse_let()
        elif token == "func":
            return self.parse_function_definition()
        elif token == "print":
            return self.parse_print()
        elif token == "return":
            return self.parse_return()
        elif token == "if":
            return self.parse_if()
        else:
            self.push_token(token)
            return InlineExpression(self.parse_expr())

    def parse_module(self) -> Module:
        statements: list[Statement] = []

        while True:
            token = self.expect_token(allow_empty=True)
            if token is None:
                return Module(statements)

            self.push_token(token)
            statements.append(self.parse_statement())

    @classmethod
    def parse(cls, source: str) -> Module:
        return cls(source).parse_module()


class Stack[T]:
    def __init__(self) -> None:
        self._queue = LifoQueue[T]()

    def push(self, value: T) -> None:
        self._queue.put_nowait(value)

    def pop(self) -> T:
        return self._queue.get_nowait()

    def is_empty(self) -> bool:
        return self._queue.empty()


class Interpreter:
    CURRENT_INTERPRETER = contextvars.ContextVar("CURRENT_INTERPRETER")

    def __init__(self, code: Iterable[Code]) -> None:
        self.code = list(code)
        self.variables: dict[str, Any] = {}
        self.functions: dict[str, Function] = {}
        self.stack = Stack[Any]()
        self._index = 0

    @classmethod
    def current(cls) -> Interpreter:
        return cls.CURRENT_INTERPRETER.get()

    def get_trampoline(self, name: str) -> ctypes._CFunctionType:
        function = self.functions[name]
        parameter_types = [ctypes.c_int64 for _ in function.parameters]

        @ctypes.CFUNCTYPE(ctypes.c_int64, *parameter_types)
        def trampoline_call(*args):
            specialized = function.specialized.get(args)
            if specialized is not None:
                return specialized()

            return self.call(function, args)

        return trampoline_call

    def call(self, function: Function, arguments: tuple[Any, ...]) -> None:
        if arguments in function.observed_args:
            function.observed_args[arguments] += 1
            if function.observed_args[arguments] == 3:
                function.specialize(arguments)
        else:
            function.observed_args[arguments] = 1

        interpreter = Interpreter([inst for inst in function.body])

        # To allow recursive calls:
        interpreter.functions[function.name] = function

        for parameter, value in zip(function.parameters, arguments):
            assert parameter not in interpreter.variables
            interpreter.variables[parameter] = value

        interpreter.interpret()
        if interpreter.stack.is_empty():
            self.stack.push(None)
        else:
            self.stack.push(interpreter.stack.pop())

    def interpret_instruction(self, instruction: Instruction) -> None:
        opcode = instruction.opcode
        oparg = instruction.oparg

        if opcode == OpCode.LOAD_CONST:
            self.stack.push(oparg)
        elif opcode == OpCode.STORE_NAME:
            assert isinstance(oparg, str)
            value = self.stack.pop()
            self.variables[oparg] = value
        elif opcode == OpCode.LOAD_NAME:
            assert isinstance(oparg, str)
            try:
                self.stack.push(self.variables[oparg])
            except KeyError:
                raise RuntimeError(f"No variable named {oparg!r}") from None
        elif opcode == OpCode.PRINT:
            assert oparg is None
            print(self.stack.pop())
        elif opcode == OpCode.STORE_FUNCTION:
            assert isinstance(oparg, Function)
            name = oparg.name
            self.functions[name] = oparg
        elif opcode == OpCode.LOAD_FUNCTION:
            assert isinstance(oparg, str)
            try:
                function = self.functions[oparg]
            except KeyError:
                raise RuntimeError(f"No function named {oparg!r}") from None
            self.stack.push(function)
        elif opcode == OpCode.CALL:
            assert isinstance(oparg, int)
            arguments = [self.stack.pop() for _ in range(oparg)]
            arguments.reverse()

            function = self.stack.pop()
            assert isinstance(function, Function)

            num_parameters = len(function.parameters)
            if num_parameters != len(arguments):
                raise RuntimeError(
                    f"expected {num_parameters} arguments to {function.name} (got {len(arguments)})"
                )

            arguments = tuple(arguments)
            specialized = function.specialized.get(arguments)
            if specialized is not None:
                self.stack.push(specialized())
                return

            self.call(function, arguments)
        elif opcode == OpCode.POP_TOP:
            assert oparg is None
            self.stack.pop()
        elif opcode == OpCode.RETURN:
            assert oparg is None
            # The value should already be on the stack -- the calling interpreter
            # will get it.
            assert not self.stack.is_empty()
            self._index = len(self.code)  # Jump to the end
        elif opcode == OpCode.JUMP:
            assert isinstance(oparg, JumpLabel)
            assert oparg.index is not None
            self._index = oparg.index
        elif opcode == OpCode.JUMP_IF_TRUE:
            assert isinstance(oparg, JumpLabel)
            item = self.stack.pop()
            if bool(item):
                assert oparg.index is not None
                self._index = oparg.index
        elif opcode == OpCode.BINARY_OP:
            assert isinstance(oparg, str)
            operators = {
                "+": py_operators.add,
                "-": py_operators.sub,
                "*": py_operators.mul,
                "/": py_operators.truediv,
            }
            right = self.stack.pop()
            left = self.stack.pop()

            self.stack.push(operators[oparg](left, right))
        elif opcode == OpCode.COMPARE:
            assert isinstance(oparg, str)
            operators = {
                "==": py_operators.eq,
                "!=": py_operators.ne,
                ">": py_operators.gt,
                ">=": py_operators.ge,
                "<": py_operators.lt,
                "<=": py_operators.le,
            }

            right = self.stack.pop()
            left = self.stack.pop()

            self.stack.push(operators[oparg](left, right))
        else:
            raise RuntimeError(f"unknown opcode: {opcode}")

    def _populate_jump_label_indices(self) -> None:
        for index, item in enumerate(self.code):
            if isinstance(item, JumpLabel):
                item.index = index

    def interpret(self) -> None:
        self._populate_jump_label_indices()

        while self._index < len(self.code):
            code = self.code[self._index]
            if isinstance(code, Instruction):
                with self.CURRENT_INTERPRETER.set(self):
                    self.interpret_instruction(code)
            else:
                assert isinstance(code, JumpLabel)

            self._index += 1

    @classmethod
    def run(cls, module: Module) -> None:
        cls(module.codegen()).interpret()


def run(source: str) -> None:
    module = Parser.parse(source)
    Interpreter.run(module)


source = """
func fib(n) {
    if n <= 0 {
        return 0
    }

    if n == 1 {
        return 1
    }

    return fib(n - 1) + fib(n - 2)
}
print fib(10)
"""

run(source)
