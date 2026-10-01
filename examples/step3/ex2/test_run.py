import unittest

from run import LENGTHS, expected_min, parse_output
from sim_support import validate_exit


def valid_output():
    lines = [f"RESULT pattern={p} offset={o} n={n} min={expected_min(p, o, n)}"
             for p in range(3) for o in range(2) for n in LENGTHS]
    lines += [f"BENCH offset={o} n={n} repeats=16 cycles=100 checksum={expected_min(0, o, n) * 16}"
              for o in range(2) for n in (255, 256)]
    return "\n".join(lines + ["PASS min-benchmark cases=60", "EXIT SUCCESS"])


class OutputTests(unittest.TestCase):
    def test_complete_results(self):
        results, benches = parse_output(valid_output())
        self.assertEqual(len(results), 60)
        self.assertEqual(len(benches), 4)

    def test_missing_result(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().split("\n", 1)[1])

    def test_wrong_result(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().replace("n=0 min=127", "n=0 min=0", 1))

    def test_disabled_counter(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().replace("cycles=100", "cycles=0", 1))

    def test_exit_failure_is_not_success(self):
        with self.assertRaises(ValueError):
            validate_exit("EXIT FAILURE: 1")
