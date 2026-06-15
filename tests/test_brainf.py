import subprocess
import tempfile
import os
import unittest
from pathlib import Path
import string

BUILD_DIR = os.environ.get("BUILD_DIR", "build")


class BrainfuckTests(unittest.TestCase):
    def setUp(self) -> None:
        self.build_dir = Path(BUILD_DIR)
        self.temporary = tempfile.TemporaryDirectory()
        path = Path(self.temporary.name)
        self.obj = path / "test.o"
        self.exe = path / "test"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _run(self, source: str, *, input_text: str | None = None) -> str:
        subprocess.run(
            [self.build_dir / "unit_brainf"],
            input=source.encode("utf-8"),
            check=True,
            cwd=self.temporary.name,
        )
        # TODO: Detect MSVC and Clang
        subprocess.run(
            ["gcc", "-o", "out", "test.o", "-lc"], check=True, cwd=self.temporary.name,
        )
        result = subprocess.run(
            [Path(self.temporary.name) / "out"], capture_output=True, input=input_text,
            encoding="utf-8",
        )
        assert isinstance(result.stdout, str)
        return result.stdout

    def test_alphabet(self) -> None:
        for char in string.ascii_letters:
            with self.subTest(char=char):
                amount = ord(char)
                pluses = "+" * amount
                result = self._run(f"{pluses}.")
                self.assertEqual(result, char)


if __name__ == "__main__":
    unittest.main()
