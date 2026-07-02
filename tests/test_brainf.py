import unittest
import string

from _test_case import ExampleTestRunner


class BrainfuckTests(ExampleTestRunner, executable_name="unit_brainf"):
    def _run(self, source: str) -> str:
        self.compile(input=source)
        return self.run_program()

    def test_alphabet(self) -> None:
        for char in string.ascii_letters:
            with self.subTest(char=char):
                amount = ord(char)
                pluses = "+" * amount
                result = self._run(f"{pluses}.")
                self.assertEqual(result, char)

    def test_shift(self) -> None:
        amount = ord("a")
        pluses = "+" * amount
        source = ""
        count = 25
        for _ in range(count):
            source += f"{pluses}>"

        for _ in range(count):
            source += "<"

        for _ in range(count):
            source += ".>"

        message = self._run(source)
        self.assertEqual(message, "a" * count)

    def test_simple_loops(self) -> None:
        amount = ord("J")
        pluses = "+" * amount
        source = f"{pluses}[>+<-]>."
        result = self._run(source)
        self.assertEqual(result, "J")

    def test_hello_world(self) -> None:
        source = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++."
        result = self._run(source)
        self.assertEqual(result, "Hello World!\n")


if __name__ == "__main__":
    unittest.main()
