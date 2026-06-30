from unit import context as context
from unit import opcode as opcode
from unit import procedure as procedure

from unit.context import Context as Context
from unit.procedure import (
    Procedure as Procedure,
    CompiledProcedure as CompiledProcedure,
    JumpLabel as JumpLabel,
    ExecutableBuffer as ExecutableBuffer,
)
from unit.opcode import OpCode as OpCode

try:
    from unit import _core
except ImportError as error:
    raise ImportError("C extension is not built") from error

__version__ = _core.UNIT_VERSION_STRING
