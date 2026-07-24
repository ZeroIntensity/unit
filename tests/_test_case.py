import os
import shutil
import subprocess
import tempfile
import unittest
from collections.abc import Iterable
from pathlib import Path


def get_link_command(obj_path: str, out_path: str) -> list[str]:
    if os.name == "nt":
        if shutil.which("gcc"):
            return ["gcc", "-o", out_path, obj_path]
        if shutil.which("clang"):
            return ["clang", "-o", out_path, obj_path]
        if shutil.which("link"):
            return [
                "link",
                obj_path,
                f"/out:{out_path}",
                "/subsystem:console",
                "/entry:main",
                "msvcrt.lib",
                "ucrt.lib",
                "legacy_stdio_definitions.lib",
            ]
        raise RuntimeError("no C linker found")
    else:
        if shutil.which("gcc"):
            return ["gcc", "-o", out_path, obj_path]
        if shutil.which("cc"):
            return ["cc", "-o", out_path, obj_path]
        if shutil.which("clang"):
            return ["clang", "-o", out_path, obj_path]
        raise RuntimeError("no C linker found")


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
        self.exe = path / ("test.exe" if os.name == "nt" else "test")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _find_executable(self, name: str) -> Path:
        direct = self.build_dir / name
        if direct.exists():
            return direct

        for config in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
            candidate = self.build_dir / config / name
            if candidate.exists():
                return candidate

        raise FileNotFoundError(
            f"{name} not found in {self.build_dir} or config subdirectories"
        )

    def compile(
        self, args: Iterable[str] | None = None, *, input: str | None = None
    ) -> None:
        subprocess.run(
            [self._find_executable(self.executable_name), *(args or ())],
            input=input,
            check=True,
            cwd=self.temporary.name,
            encoding="utf-8",
            timeout=5,
        )
        cmd = get_link_command("test.o", "out")
        subprocess.run(
            cmd,
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
