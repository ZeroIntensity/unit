import io
import os
import tempfile
import unittest
from contextlib import contextmanager
from typing import IO, Generator

from examples.language_with_jit import Interpreter, Parser


@contextmanager
def capture_c_stdout() -> Generator[IO[bytes]]:
    original = os.dup(1)

    with tempfile.TemporaryFile(mode="w+b") as tmp:
        os.dup2(tmp.fileno(), 1)

        try:
            yield tmp
        finally:
            os.dup2(original, 1)
            os.close(original)


class TestLanguageWithJIT(unittest.TestCase):
    def run_string(self, source: str, *, force_specialization: bool = False) -> str:
        if force_specialization is True:
            source = f"""
            func main() {"{"}
                {source}
                return 0
            {"}"}
            main()
            """

        parser = Parser(source)
        module = parser.parse_module()

        buffer = io.StringIO()
        interpreter = Interpreter(
            out_file=buffer, force_specialization=force_specialization
        )

        with capture_c_stdout() as stdout:
            interpreter.interpret(list(module.codegen()))

            # The specializations use printf, so they write to the C stdout rather
            # than our buffer.
            if force_specialization is True:
                stdout.flush()
                import ctypes

                ctypes.CDLL(None).fflush(None)
                stdout.seek(0)
                return stdout.read().decode("utf-8").strip("\n")

        return buffer.getvalue().strip("\n")

    def assert_output(self, source: str, *output: str) -> None:
        result = self.run_string(source)
        self.assertEqual(result.split("\n"), list(output))
        specialized_result = self.run_string(source, force_specialization=True)
        self.assertEqual(specialized_result.split("\n"), list(output))

    def test_print(self):
        self.assert_output("print 123", "123")
        self.assert_output("print '123'", "123")
        self.assert_output('print "123"', "123")

    def test_let(self):
        source = """
        let foo = 1
        let foo2 = 2
        let foo3 = foo2
        let foo4 = 1
        let foo4 = 3
        print foo
        print foo2
        print foo3
        print foo4
        """
        self.assert_output(source, "1", "2", "2", "3")

    def test_if(self):
        source = """
        if 5 == 5 {
            print 1
        }

        let x = 5
        if x == 5 {
            print x
        }

        if x == 6 {
            print 6
        }

        if x == 7 {
            print 7
        } else {
            print 8
        }
        """
        self.assert_output(source, "1", "5", "8")

    def test_arithmetic(self):
        source = """
        print 2 + 3
        print 4 - 3
        print 10 * 10
        print 10 / 2
        """
        self.assert_output(source, "5", "1", "100", "5")

    # FIXME: Failing
    def notest_comparisons(self):
        source = """
        print 2 == 2
        print 2 == 3

        print 2 != 2
        print 2 != 3

        print 2 < 3
        print 3 < 2
        print 3 < 3

        print 0 <= 0
        print 0 <= 1
        print 1 <= 0

        print 4 > 5
        print 5 > 4
        print 5 > 5

        print 5 >= 5
        print 6 >= 5
        print 4 >= 5
        """
        self.assert_output(
            source,
            "True",
            "False",
            "False",
            "True",
            "True",
            "False",
            "False",
            "True",
            "True",
            "False",
            "False",
            "True",
            "False",
            "True",
            "True",
            "False",
        )

    def test_functions(self):
        source = """
        func my_func() {
            return 123
        }

        print my_func()

        func parameters(x, y) {
            print x
            print y
            return y
        }

        print parameters(42, 24)
        """
        self.assert_output(source, "123", "42", "24", "24")

    def test_factorial(self):
        source = """
        func factorial(n) {
            if n == 0 {
                return 1
            }

            return n * factorial(n - 1)
        }

        print factorial(5)
        """
        self.assert_output(source, "120")

    def test_fib(self):
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
        self.assert_output(source, "55")
