import unittest
from _test_case import ExampleTestRunner


class TestFactorial(ExampleTestRunner, executable_name="unit_factorial"):
    def _run(self, number: int) -> str:
        self.compile(args=[str(number)])
        return self.run_program()

    def test_factorial_values(self) -> None:
        for input, output in (
            (0, 1),
            (1, 1),
            (2, 2),
            (3, 6),
            (4, 24),
            (5, 120),
            (6, 720),
            (7, 5040),
        ):
            with self.subTest(input=input, output=output):
                result = self._run(input).strip("\n")
                self.assertEqual(result, str(output))


if __name__ == "__main__":
    unittest.main()
