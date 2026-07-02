from typing import ClassVar
from unit._core import Error as _Error
from unit import _core
from enum import Enum
from dataclasses import dataclass
from collections.abc import Generator
from contextlib import contextmanager

__all__ = "Error", "NoMemory", "InvalidUsage", "OSFailure", "UnsupportedPlatform"

class ErrorCode(Enum):
    UNIT_ERROR_NONE = _core.UNIT_ERROR_NONE
    UNIT_ERROR_NO_MEMORY = _core.UNIT_ERROR_NO_MEMORY
    UNIT_ERROR_INVALID_USAGE = _core.UNIT_ERROR_INVALID_USAGE
    UNIT_ERROR_OS_FAILURE = _core.UNIT_ERROR_OS_FAILURE
    UNIT_ERROR_UNSUPPORTED_PLATFORM = _core.UNIT_ERROR_UNSUPPORTED_PLATFORM


CODES_TO_EXCEPTIONS: dict[ErrorCode, type[Error]] = {}


@dataclass(frozen=True)
class Error(_Error):
    """
    Base class for errors in UNIT.
    """

    message: str
    error_code: ClassVar[ErrorCode]

    def __init_subclass__(cls) -> None:
        super().__init_subclass__()
        assert cls.error_code != ErrorCode.UNIT_ERROR_NONE
        assert cls.error_code not in CODES_TO_EXCEPTIONS, f"{cls.error_code} already set to a class"
        CODES_TO_EXCEPTIONS[cls.error_code] = cls

    @staticmethod
    def from_internal_error(error: _Error) -> Error:
        code = ErrorCode(error.code)
        assert code != ErrorCode.UNIT_ERROR_NONE
        instance = CODES_TO_EXCEPTIONS[code]
        return instance(error.message)

    @classmethod
    @contextmanager
    def capture_internal_errors(cls) -> Generator[None]:
        try:
            yield
        except _Error as internal_error:
            raise cls.from_internal_error(internal_error) from internal_error


class NoMemory(MemoryError, Error):
    error_code = ErrorCode.UNIT_ERROR_NO_MEMORY

class InvalidUsage(ValueError, Error):
    error_code = ErrorCode.UNIT_ERROR_INVALID_USAGE

class OSFailure(OSError, Error):
    error_code = ErrorCode.UNIT_ERROR_OS_FAILURE

class UnsupportedPlatform(Error):
    error_code = ErrorCode.UNIT_ERROR_UNSUPPORTED_PLATFORM
