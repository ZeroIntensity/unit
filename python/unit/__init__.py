# ruff: noqa: F403

from unit import context as context
from unit import opcode as opcode
from unit import procedure as procedure
from unit.context import *
from unit.error import *
from unit.opcode import *
from unit.procedure import *

try:
    from unit import _core
except ImportError as error:
    raise ImportError("C extension is not built") from error

__version__ = _core.UNIT_VERSION_STRING
