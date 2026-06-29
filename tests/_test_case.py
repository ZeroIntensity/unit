from collections.abc import Iterable
import os
import unittest
import tempfile
from pathlib import Path
import subprocess

BUILD_DIR = os.environ.get("BUILD_DIR", "./build")


class ExampleTestRunner(unittest.TestCase):
    executable_name: str

    def __init_subclass__(cls, executable_name: str) -> None:
        cls.executable_name = executable_name
        super().__init_subclass__()

    def setUp(self) -> None:
        self.build_dir = Path(BUILD_DIR).absolute()
        if not self.build_dir.exists():
            raise FileNotFoundError(f"{self.build_dir} not found")
        self.temporary = tempfile.TemporaryDirectory()
        path = Path(self.temporary.name)
        self.obj = path / "test.o"
        self.exe = path / "test"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def compile(
        self, args: Iterable[str] | None = None, *, input: str | None = None
    ) -> None:
        subprocess.run(
            [self.build_dir / self.executable_name, *(args or ())],
            input=input,
            check=True,
            cwd=self.temporary.name,
            encoding="utf-8",
            timeout=5,
        )
        import sys
        if sys.platform == "darwin":
            link_cmd = ["cc", "-o", "out", "test.o"]
        else:
            link_cmd = ["cc", "-o", "out", "test.o", "-lc"]
        subprocess.run(
            link_cmd,
            check=True,
            cwd=self.temporary.name,
            timeout=5,
        )

    def run_program(
        self,
        *,
        args: list[str] | None = None,
        input: str | None = None,
    ) -> str:
        result = subprocess.run(
            [Path(self.temporary.name) / "out", *(args or ())],
            capture_output=True,
            input=input,
            encoding="utf-8",
            cwd=self.temporary.name,
            timeout=5,
        )
        assert isinstance(result.stdout, str)
        return result.stdout
