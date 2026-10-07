import csv
import json
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from benchmark import (GROUND_TRUTH, REPO, SIZES, comparison, configure, digest,
                       load_ground_truth, load_luts, parse_output, reference_words, save_results)
from sim_support import run_firmware, run_logged


class BenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        generator = runpy.run_path(str(REPO / "hardware/smx/SMX_dev/src/gen_c_luts.py"))
        cls.lut1 = generator["generate_lut1d_exp"]()
        cls.lut2 = [value for row in generator["generate_lut2d_sigm"]() for value in row]

    def output(self, n=8):
        words = reference_words(n, self.lut1, self.lut2)
        return "\n".join([f"SMX_TIMING n={n} cycles=100"] +
                         [f"SMX_OUTPUT index={i} value={value}" for i, value in enumerate(words)] +
                         ["EXIT SUCCESS"])

    def parse(self, output, n=8):
        return parse_output(output, n, reference_words(n, self.lut1, self.lut2))

    def test_all_historical_sizes_and_top1(self):
        for n in SIZES:
            with self.subTest(n=n):
                result = self.parse(self.output(n), n)
                self.assertEqual(len(result["packed"]), n // 4)
                self.assertEqual(result["top1"], {8: 5, 16: 12}.get(n, 19))

    def test_fp32_and_software_cycles_are_fixed(self):
        old = load_ground_truth()[0]
        result = comparison(old, self.parse(self.output()))
        self.assertEqual((result["FP32"], result["SW_Int8"], result["SMX"]), (11623, 290, 29))
        self.assertEqual(result["FP32_ground_truth_over_LLVM"], 116.23)
        self.assertEqual(result["SW_ground_truth_over_LLVM"], 2.9)
        self.assertEqual(result["SMX_old_over_LLVM"], 0.29)

    def test_64_bit_cycle_counter(self):
        output = self.output().replace("cycles=100", "cycles=4294967297")
        self.assertEqual(self.parse(output)["SMX_LLVM"], 4294967297)

    def test_invalid_or_duplicate_timing(self):
        for value in ("0", "-1", str(2**64), "abc"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.parse(self.output().replace("cycles=100", "cycles=" + value))
        with self.assertRaises(ValueError):
            self.parse(self.output() + "\nSMX_TIMING n=8 cycles=100")
        with self.assertRaises(ValueError):
            self.parse(self.output().replace("n=8", "n=16"))

    def test_missing_timing(self):
        with self.assertRaises(ValueError):
            self.parse(self.output().split("\n", 1)[1])

    def test_missing_duplicate_and_invalid_words(self):
        lines = self.output().splitlines()
        for output in ("\n".join(lines[:1] + lines[2:]), self.output() + "\n" + lines[1],
                       self.output().replace("index=0", "index=100"),
                       self.output().replace(lines[1], "SMX_OUTPUT index=0 value=4294967296"),
                       self.output().replace(lines[1], "SMX_OUTPUT index=0 value=-1")):
            with self.subTest(output=output), self.assertRaises(ValueError):
                self.parse(output)

    def test_actual_packed_words_are_checked(self):
        lines = self.output().splitlines()
        lines[1] = "SMX_OUTPUT index=0 value=0"
        with self.assertRaises(ValueError):
            self.parse("\n".join(lines))

    def test_firmware_exit_required(self):
        for output in (self.output().replace("EXIT SUCCESS", "EXIT FAILURE"),
                       self.output().replace("EXIT SUCCESS", ""),
                       self.output() + "\nTEST(S) FAILED"):
            with self.assertRaises(ValueError):
                self.parse(output)

    def test_unknown_benchmark_dimension_rejected(self):
        with self.assertRaises(ValueError):
            reference_words(63, self.lut1, self.lut2)
        with self.assertRaises(ValueError):
            comparison(load_ground_truth()[1], self.parse(self.output()))

    def test_ground_truth_matches_published_table(self):
        rows = {row["N"]: row for row in load_ground_truth()}
        self.assertEqual(rows[64], {"N": 64, "FP32": 110276, "SMX": 112, "SW_Int8": 2851})
        self.assertEqual(rows[1024], {"N": 1024, "FP32": 1759547, "SMX": 4882, "SW_Int8": 46639})

    def test_bad_ground_truth_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "input.csv"
            original = GROUND_TRUTH.read_text()
            for contents in (original.replace("N,FP32", "size,FP32"),
                             original.replace("8,11623,29,290", "8,11623,29"),
                             original.replace("8,11623,29,290", "8,0,29,290"),
                             original.replace("16,27784", "8,27784"),
                             original + "1024,1,1,1\n"):
                path.write_text(contents)
                with self.assertRaises(ValueError):
                    load_ground_truth(path)

    def test_config_does_not_invalidate_independent_sweep(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            header = root / "config.h"
            configure(63, header)
            stamp = header.stat().st_mtime_ns
            configure(63, header)
            self.assertEqual(header.stat().st_mtime_ns, stamp)
            result = root / "results.json"
            result.write_text("sweep")
            configure(256, header)
            self.assertEqual(header.read_text(), "#define N 256u\n")
            self.assertEqual(result.read_text(), "sweep")
            with self.assertRaises(ValueError):
                configure(3, header)

    def test_lut_dimensions_and_range(self):
        with tempfile.TemporaryDirectory() as tmp:
            core = Path(tmp)
            for name, values in (("smx_lut1d.hex", self.lut1), ("smx_lut2d.hex", self.lut2)):
                (core / name).write_text("\n".join(f"{value:02x}" for value in values))
            self.assertEqual(load_luts(core), (self.lut1, self.lut2))
            (core / "smx_lut1d.hex").write_text("100\n" * 101)
            with self.assertRaises(ValueError):
                load_luts(core)
            (core / "smx_lut1d.hex").write_text("00\n")
            with self.assertRaises(ValueError):
                load_luts(core)

    def test_reports_do_not_modify_ground_truth(self):
        before = digest(GROUND_TRUTH)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            row = comparison(load_ground_truth()[0], self.parse(self.output()))
            save_results(root, [row], {"ground_truth_sha256": before})
            self.assertEqual(json.loads((root / "results.json").read_text())["rows"][0], row)
            with (root / "results.csv").open() as stream:
                self.assertEqual(next(csv.DictReader(stream))["FP32"], "11623")
            self.assertNotIn(b"\r", (root / "results.csv").read_bytes())
            self.assertIn("116.23x", (root / "README.md").read_text())
        self.assertEqual(digest(GROUND_TRUTH), before)

    def test_quiet_build_log_and_errors(self):
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "build.log"
            output = Path(tmp) / "output"
            run_logged([sys.executable, "-c", "print('artifact')"], log, output)
            self.assertEqual(output.read_text(), "artifact\n")
            with self.assertRaisesRegex(RuntimeError, "compiler error"):
                run_logged([sys.executable, "-c", "print('compiler error'); raise SystemExit(1)"], log)

    def test_timeout_and_failed_exit_remain_visible(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            with patch("sim_support.shutil.copy2"), patch("sim_support.subprocess.run",
                       side_effect=subprocess.TimeoutExpired("Vtb_top", 1, output=b"partial\n")):
                with self.assertRaises(RuntimeError):
                    run_firmware(root, root / "test.hex", root, quiet=True)
            self.assertIn("partial", (root / "simulation.log").read_text())
            with patch("sim_support.shutil.copy2"), patch("sim_support.subprocess.run",
                       return_value=SimpleNamespace(returncode=0, stdout="EXIT FAILURE")):
                with self.assertRaises(ValueError):
                    run_firmware(root, root / "test.hex", root, quiet=True)


if __name__ == "__main__":
    unittest.main()
