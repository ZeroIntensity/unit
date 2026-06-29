from typing import Any
import unit
import unittest


class BasicTests(unittest.TestCase):
    def make_procedure(self):
        return unit.Procedure(self.id())

    def compile_and_run(self, procedure: unit.Procedure, expected: Any, *args: Any):
        unoptimized_function = procedure.compile().jit()
        self.assertEqual(unoptimized_function(*args), expected)
        procedure.optimize()
        optimized_function = procedure.compile().jit()
        self.assertEqual(optimized_function(*args), expected)

    def test_return_constant(self):
        proc = self.make_procedure()
        proc.load_integer(42)
        proc.return_value()
        self.compile_and_run(proc, 42)

    def test_add(self):
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(20)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 30)

    def test_sub(self):
        proc = self.make_procedure()

        proc.load_integer(10)
        proc.load_integer(20)
        proc.add()
        proc.return_value()

        self.compile_and_run(proc, 30)

    def test_mul(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.load_integer(10)
        proc.multiply()
        proc.return_value()

        self.compile_and_run(proc, 50)

    def test_div(self):
        proc = self.make_procedure()

        proc.load_integer(20)
        proc.load_integer(10)
        proc.divide()
        proc.return_value()

        self.compile_and_run(proc, 2)

    def test_mod(self):
        proc = self.make_procedure()

        proc.load_integer(5)
        proc.load_integer(2)
        proc.modulo()
        proc.return_value()

        self.compile_and_run(proc, 1)

    def test_locals(self):
        proc = self.make_procedure()

        proc.load_integer(42)
        proc.store_local(0)

        proc.load_integer(24)
        proc.store_local(0)

        proc.load_local(0)
        proc.return_value()

        self.compile_and_run(proc, 24)

    def test_arguments(self):
        proc = self.make_procedure()

        proc.load_argument(0)  # 2
        proc.load_integer(5)
        proc.multiply()  # 10

        proc.load_argument(1)  # 3
        proc.add()

        proc.return_value()

        self.compile_and_run(proc, 13, 2, 3)

    def test_call_name(self):
        proc = self.make_procedure()

        proc.load_string("hello")
        proc.call_name("strlen", 1)
        proc.return_value()

        self.compile_and_run(proc, 5)


if __name__ == "__main__":
    unittest.main()
