from __future__ import annotations
from typing import Self

from unit import _core
import contextvars

class Context:
    CURRENT_CONTEXT = contextvars.ContextVar["Context | None"]('CURRENT_CONTEXT', default=None)

    def __init__(self) -> None:
        self._context = _core.Context()
        self._old_token: contextvars.Token | None = None

    def __enter__(self) -> Self:
        self._old_token = self.CURRENT_CONTEXT.set(self)
        return self

    def __exit__(self, *_) -> None:
        assert self._old_token is not None
        self.CURRENT_CONTEXT.reset(self._old_token)

    @classmethod
    def current(cls) -> Context | None:
        """
        Get the current context, if any.
        """

        return cls.CURRENT_CONTEXT.get()

    @classmethod
    def current_or_new(cls) -> Context:
        """
        Get the current context, or create a new one if not set.
        """

        return cls.current() or cls()


class Procedure:
    def __init__(self, *, context: Context | None = None) -> None:
        self.context = context or Context.current_or_new()
        self._procedure = _core.Procedure(self.context._context)
