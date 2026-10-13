import unittest

from run import BENCH_KEYS, expected_checksum, expected_pair, expected_results, parse_output


def valid_output() -> str:
    lines = [f"RESULT pattern={p} cases={count} hash={digest:08x}"
             for p, (count, digest) in expected_results().items()]
    lines += [f"BENCH kind={k} offset={o} n={n} repeats=16 cycles=1000 checksum={expected_checksum(k, n)}"
              for k, o, n in sorted(BENCH_KEYS)]
    return "\n".join(lines + ["PASS extrema-benchmark cases=6512"])


class OutputTests(unittest.TestCase):
    def test_empty_and_seeded_results(self):
        self.assertEqual(expected_pair(8, 0, 0), (127, -128))
        self.assertEqual(expected_pair(5, 1, 0), (-40, 40))
        self.assertEqual(expected_pair(5, 1, 256), (-40, 40))
        self.assertEqual(expected_pair(2, 4, 1), (127, 127))

    def test_valid_output(self):
        results, benches = parse_output(valid_output())
        self.assertEqual(sum(count for count, _ in results.values()), 6512)
        self.assertEqual(len(benches), 34)

    def test_missing_result(self):
        with self.assertRaisesRegex(ValueError, "oracle"):
            parse_output(valid_output().replace("RESULT pattern=0", "SKIPPED pattern=0"))

    def test_duplicate_result(self):
        output = valid_output()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            parse_output(output + "\n" + output.splitlines()[0])

    def test_wrong_digest(self):
        output = valid_output().splitlines()
        output[0] = "RESULT pattern=0 cases=814 hash=00000000"
        with self.assertRaisesRegex(ValueError, "oracle"):
            parse_output("\n".join(output))

    def test_invalid_timing_or_checksum(self):
        for bad in (valid_output().replace("cycles=1000", "cycles=0", 1),
                    valid_output().replace("checksum=", "checksum=9", 1)):
            with self.assertRaises(ValueError):
                parse_output(bad)

    def test_missing_benchmark_or_pass(self):
        for bad in (valid_output().replace("BENCH kind=0", "SKIPPED kind=0", 1),
                    valid_output().replace("PASS extrema", "MISSING extrema")):
            with self.assertRaises(ValueError):
                parse_output(bad)

    def test_firmware_failure(self):
        with self.assertRaisesRegex(ValueError, "failure"):
            parse_output(valid_output() + "\nFAIL numerical mismatch")


if __name__ == "__main__":
    unittest.main()
