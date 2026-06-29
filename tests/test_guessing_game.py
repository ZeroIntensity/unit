import unittest
from _test_case import ExampleTestRunner


class TestGuessingGame(ExampleTestRunner, executable_name="unit_guess"):
    def setUp(self) -> None:
        super().setUp()
        self.compile()

    def _run(self, *, seed: int, guesses: list[int]) -> str:
        return self.run_program(
            args=[str(seed)], input="\n".join([str(i) for i in guesses])
        )

    def _filter_results(self, raw: str) -> list[str]:
        results: list[str] = []
        for line in raw.split("\n"):
            if line not in {"Higher", "Lower", "You win!"}:
                continue

            results.append(line)

        return results

    def test_guess_set_seed(self):
        results = self._filter_results(self._run(seed=42, guesses=[66, 68, 67]))
        self.assertEqual(results, ["Higher", "Lower", "You win!"])


if __name__ == "__main__":
    unittest.main()
