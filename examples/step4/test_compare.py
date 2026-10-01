from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from compare import parse_assembly, parse_output, summarize
from sim_support import run_firmware


def valid_output():
    lines = [f"SMX_OUTPUT index={i} value={i * 123}" for i in range(15)]
    return "\n".join(lines + ["Top-1 FP32: 19, SMX: 19 -> Match: YES"])


class ComparisonTests(unittest.TestCase):
    def test_compressed_and_custom_instruction(self):
        instructions = parse_assembly("""00000000 <main>:
       0: 82 80        ret
00000002 <.LBB0_1>:
       2: 8b 02 c3 00  smx.contx t0, t1, a2
""")
        counts = summarize(instructions)
        self.assertEqual(counts["instructions"], 2)
        self.assertEqual(counts["bytes"], 6)
        self.assertEqual(counts["smx"]["smx.contx"], 1)
        self.assertEqual(counts["smx_functions"], ["main"])

    def test_empty_disassembly(self):
        with self.assertRaises(ValueError):
            parse_assembly("invalid")

    def test_complete_packed_outputs(self):
        self.assertEqual(len(parse_output(valid_output())["words"]), 15)

    def test_missing_word(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().split("\n", 1)[1])

    def test_duplicate_word(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output() + "\nSMX_OUTPUT index=0 value=0")

    def test_out_of_range_word(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().replace("index=0 value=0", "index=0 value=4294967296"))

    def test_top1_failure(self):
        with self.assertRaises(ValueError):
            parse_output(valid_output().replace("Match: YES", "Match: NO"))


class SimulatorTests(unittest.TestCase):
    def test_timeout_keeps_partial_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            error = subprocess.TimeoutExpired("Vtb_top", 1, output=b"partial output\n")
            with patch("sim_support.shutil.copy2"), patch("builtins.print"), \
                    patch("sim_support.subprocess.run", side_effect=error):
                with self.assertRaises(RuntimeError):
                    run_firmware(directory, directory / "test.hex", directory, timeout=1)
            self.assertEqual((directory / "simulation.log").read_text(),
                             "partial output\n\nTIMEOUT after 1 seconds\n")

    def test_zero_process_exit_does_not_hide_firmware_failure(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            result = SimpleNamespace(returncode=0, stdout="EXIT FAILURE: 1\n")
            with patch("sim_support.shutil.copy2"), patch("builtins.print"), \
                    patch("sim_support.subprocess.run", return_value=result):
                with self.assertRaises(ValueError):
                    run_firmware(directory, directory / "test.hex", directory)
            self.assertEqual((directory / "simulation.log").read_text(), result.stdout)
