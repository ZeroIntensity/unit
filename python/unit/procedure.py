import sys
from unit.context import Context
from unit.opcode import OpCode
from unit import _core
from typing import IO, Any, Literal, TypeAlias, TypeVar, Generic
from dataclasses import dataclass
import ctypes

ArgT = TypeVar("ArgT")
ResT = TypeVar("ResT")


class ExecutableBuffer(Generic[ArgT, ResT]):
    def __init__(self, executable_buffer: _core.ExecutableBuffer) -> None:
        if __debug__ and not isinstance(executable_buffer, _core.ExecutableBuffer):
            raise TypeError(
                f"expected an internal executable buffer, got {executable_buffer!r}"
            )

        self._buffer = executable_buffer

    @property
    def address(self) -> int:
        return self._buffer.address

    def __call__(self, *args: ArgT) -> ResT:
        ctypes_args = []
        arg_types = []
        for arg in args:
            if isinstance(arg, int):
                ctypes_args.append(ctypes.c_int64(arg))
                arg_types.append(ctypes.c_int64)
            elif isinstance(arg, float):
                ctypes_args.append(ctypes.c_double(arg))
                arg_types.append(ctypes.c_double)
            elif isinstance(arg, bytes):
                ctypes_args.append(ctypes.c_char_p(arg))
                arg_types.append(ctypes.c_char_p)
            elif isinstance(arg, str):
                encoded = arg.encode("utf-8")
                ctypes_args.append(ctypes.c_char_p(encoded))
                arg_types.append(ctypes.c_char_p)
            else:
                raise TypeError(
                    f"unsupported argument type: {type(arg)}. "
                    "For finer control over arguments, construct your own "
                    "function pointer via the 'address' attribute."
                )

        func_type = ctypes.CFUNCTYPE(ctypes.c_int64, *arg_types)
        func = func_type(self._buffer.address)
        return func(*ctypes_args)


class CompiledProcedure:
    def __init__(self, compiled_procedure: _core.CompiledProcedure) -> None:
        if __debug__ and not isinstance(compiled_procedure, _core.CompiledProcedure):
            raise TypeError(
                f"expected an internal compiled procedure, got {compiled_procedure!r}"
            )

        self._compiled = compiled_procedure

    def write_object_file(
        self, path: str, format: Literal["elf", "macho", "pe"]
    ) -> None:
        if format == "elf":
            format_enum = _core.UNIT_FORMAT_ELF
        elif format == "macho":
            format_enum = _core.UNIT_FORMAT_MACHO
        elif format == "pe":
            format_enum = _core.UNIT_FORMAT_PE
        else:
            raise ValueError(
                f"unknown format {format!r}, expected one of: elf, macho, pe"
            )
        self._compiled.write_object_file(path, format_enum)

    def jit(self) -> ExecutableBuffer[Any, Any]:
        return ExecutableBuffer(self._compiled.jit())

    def translation_text(self) -> str:
        return self._compiled.print_translation()



Architecture: TypeAlias = Literal["amd64", "aarch64"]
ABI: TypeAlias = Literal["systemv", "apple", "win64"]


@dataclass(slots=True)
class Platform:
    architecture: Architecture
    abi: ABI

    def to_value(self) -> int:
        result = 0
        if self.architecture == "amd64":
            result |= _core.UNIT_ARCH_AMD64
        elif self.architecture == "aarch64":
            result |= _core.UNIT_ARCH_AARCH64
        else:
            raise ValueError(
                f"unknown architecture {self.architecture!r}, expected one of: amd64, aarch64"
            )

        if self.abi == "systemv":
            result |= _core.UNIT_ABI_SYSTEMV
        elif self.abi == "apple":
            result |= _core.UNIT_ABI_APPLE
        elif self.abi == "win64":
            result |= _core.UNIT_ABI_WIN64
        else:
            raise ValueError(
                f"unknown ABI {self.abi!r}, expected one of: systemv, apple, win64"
            )

        return result

    @classmethod
    def host(cls) -> Platform:
        host = _core.UNIT_HOST_PLATFORM

        arch_num = host & _core._UNIT_ARCH_MASK
        architecture: Architecture
        if arch_num == _core.UNIT_ARCH_AMD64:
            architecture = "amd64"
        else:
            assert arch_num == _core.UNIT_ARCH_AARCH64
            architecture = "aarch64"

        abi_num = host & _core._UNIT_ABI_MASK
        abi: ABI
        if abi_num == _core.UNIT_ABI_SYSTEMV:
            abi = "systemv"
        elif abi_num == _core.UNIT_ABI_APPLE:
            abi = "apple"
        else:
            assert abi_num == _core.UNIT_ABI_WIN64
            abi = "win64"

        return cls(architecture=architecture, abi=abi)


class Procedure:
    def __init__(
        self,
        name: str,
        *,
        context: Context | None = None,
        inlining: Literal["force", "never"] | None = None,
        optimize_translation: bool = True,
    ) -> None:
        self.context = context or Context.current_or_new()
        self.name = name
        self._procedure = _core.Procedure(self.context._context, name)
        flags = _core.UNIT_FLAG_NONE

        if inlining == "force":
            flags |= _core.UNIT_FLAG_FORCE_INLINE
        elif inlining == "never":
            flags |= _core.UNIT_FLAG_FORCE_NO_INLINE

        # We don't need a case for True because translation optimization is
        # enabled by default.
        if optimize_translation is False:
            flags |= _core.UNIT_FLAG_NO_OPTIMIZE_TRANSLATION

        self._procedure.set_flags(flags)

    def _add_op_int(self, opcode: OpCode, oparg: int) -> None:
        self._procedure.add_operation(opcode.value, oparg)

    def _add_op(self, opcode: OpCode) -> None:
        self._add_op_int(opcode, 0)

    def load_integer(self, value: int, /) -> None:
        self._add_op_int(OpCode.LOAD_INTEGER, value)

    def store_local(self, id: int, /) -> None:
        self._add_op_int(OpCode.STORE_LOCAL, id)

    def load_local(self, id: int, /) -> None:
        self._add_op_int(OpCode.LOAD_LOCAL, id)

    def add(self) -> None:
        self._add_op(OpCode.ADD)

    def subtract(self) -> None:
        self._add_op(OpCode.SUBTRACT)

    def multiply(self) -> None:
        self._add_op(OpCode.MULTIPLY)

    def divide(self) -> None:
        self._add_op(OpCode.DIVIDE)

    def modulo(self) -> None:
        self._add_op(OpCode.MODULO)

    def exit(self) -> None:
        self._add_op(OpCode.EXIT)

    def return_value(self) -> None:
        self._add_op(OpCode.RETURN_VALUE)

    def load_argument(self, arg_number: int, /) -> None:
        self._add_op_int(OpCode.LOAD_ARGUMENT, arg_number)

    def call_name(self, name: str, num_args: int, /) -> None:
        self._procedure.add_call_name(name, num_args)

    def compare_equal(self) -> None:
        self._add_op(OpCode.COMPARE_EQUAL)

    def compare_not_equal(self) -> None:
        self._add_op(OpCode.COMPARE_NOT_EQUAL)

    def compare_greater(self) -> None:
        self._add_op(OpCode.COMPARE_GREATER)

    def compare_greater_equal(self) -> None:
        self._add_op(OpCode.COMPARE_GREATER_EQUAL)

    def compare_less(self) -> None:
        self._add_op(OpCode.COMPARE_LESS)

    def compare_less_equal(self) -> None:
        self._add_op(OpCode.COMPARE_LESS_EQUAL)

    def copy(self, offset_from_top: int, /) -> None:
        self._add_op_int(OpCode.COPY, offset_from_top)

    def swap(self, offset_from_top: int, /) -> None:
        self._add_op_int(OpCode.SWAP, offset_from_top)

    def pop(self, offset_from_top: int, /) -> None:
        self._add_op_int(OpCode.POP, offset_from_top)

    def address_of(self, id: int, /) -> None:
        self._add_op_int(OpCode.ADDRESS_OF, id)

    def read_bytes(self, num_bytes: int, /) -> None:
        self._add_op_int(OpCode.READ_BYTES, num_bytes)

    def write_bytes(self, num_bytes: int, /) -> None:
        self._add_op_int(OpCode.WRITE_BYTES, num_bytes)

    def compile(self, platform: Platform | None = None) -> CompiledProcedure:
        platform = platform or Platform.host()
        return CompiledProcedure(self._procedure.compile(platform.to_value()))

    def optimize(self) -> None:
        self._procedure.optimize()

    def instructions_text(self, *, visualize_stack_effect: bool = True) -> None:
        return self._procedure.print_instructions(int(visualize_stack_effect))
