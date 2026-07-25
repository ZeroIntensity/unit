import unittest
import os

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
        # The same seed on Linux and Windows produces a different random number, so
        # we have to adjust the guesses.
        guesses = [75, 77, 76] if os.name == "nt" else [66, 68, 67]
        results = self._filter_results(self._run(seed=42, guesses=guesses))
        self.assertEqual(results, ["Higher", "Lower", "You win!"])


if __name__ == "__main__":
    unittest.main()
