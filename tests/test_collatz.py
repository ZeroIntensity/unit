import unittest
from _test_case import ExampleTestRunner


class TestCollatz(ExampleTestRunner, executable_name="unit_collatz"):
    def _run(self, number: int) -> str:
        self.compile(args=[str(number)])
        return self.run_program()

    def test_collatz_values(self) -> None:
        values = (
            "100",
            "50",
            "25",
            "76",
            "38",
            "19",
            "58",
            "29",
            "88",
            "44",
            "22",
            "11",
            "34",
            "17",
            "52",
            "26",
            "13",
            "40",
            "20",
            "10",
            "5",
            "16",
            "8",
            "4",
            "2",
            "1",
            "",
        )
        for index, line in enumerate(self._run(100).split("\n")):
            with self.subTest(index=index, line=line):
                self.assertEqual(values[index], line)


if __name__ == "__main__":
    unittest.main()
