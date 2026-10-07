import json
from pathlib import Path
import tempfile
import unittest

from benchmark import GROUND_TRUTH, SIZES, comparison, digest, load_ground_truth, save_results
from plots import cycles, generate, load_results, speedup
import matplotlib.pyplot as plt


def sample_rows():
    return [comparison(row, {"N": row["N"], "SMX_LLVM": max(1, row["SMX"] - 1)})
            for row in load_ground_truth()]


class PlotTests(unittest.TestCase):
    def test_original_curves_and_all_sizes_preserved(self):
        rows = sample_rows()
        for scale in ("linear", "log"):
            fig, ax = plt.subplots()
            cycles(ax, rows, scale)
            self.assertEqual(ax.get_xscale(), scale)
            self.assertEqual(ax.get_yscale(), "log")
            self.assertEqual(len(ax.lines), 4)
            for line, key in zip(ax.lines, ("FP32", "SW_Int8", "SMX", "SMX_LLVM")):
                self.assertEqual(list(line.get_xdata()), list(SIZES))
                self.assertEqual(list(line.get_ydata()), [row[key] for row in rows])
            plt.close(fig)

    def test_speedups_use_historical_numerators(self):
        rows = sample_rows()
        fig, ax = plt.subplots()
        speedup(ax, rows, "FP32", "linear")
        self.assertEqual(list(ax.lines[1].get_ydata()), [row["FP32"] / row["SMX_LLVM"] for row in rows])
        plt.close(fig)

    def test_stale_modified_or_incomplete_results_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            metadata = {"ground_truth_sha256": digest(GROUND_TRUTH)}
            rows = sample_rows()
            save_results(root, rows, metadata)
            self.assertEqual(load_results(root), rows)
            for change in ("ground", "size", "ratio", "hash"):
                data = json.loads((root / "results.json").read_text())
                if change == "ground": data["rows"][0]["FP32"] += 1
                if change == "size": data["rows"].pop()
                if change == "ratio": data["rows"][0]["FP32_ground_truth_over_LLVM"] += 1
                if change == "hash": data["metadata"]["ground_truth_sha256"] = "old"
                (root / "results.json").write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    load_results(root)
                save_results(root, rows, metadata)

    def test_all_variants_generated_without_modifying_data(self):
        rows = sample_rows()
        before = json.dumps(rows)
        with tempfile.TemporaryDirectory() as tmp:
            generate(rows, Path(tmp))
            names = ("cycles", "speedup", "softmax_performance", "cycles_linear",
                     "speedup_linear", "softmax_performance_linear", "smx_only_linear")
            for name in names:
                for extension in ("png", "svg"):
                    self.assertGreater((Path(tmp) / f"{name}.{extension}").stat().st_size, 1000)
                for line in (Path(tmp) / f"{name}.svg").read_text().splitlines():
                    self.assertEqual(line, line.rstrip())
        self.assertEqual(json.dumps(rows), before)


if __name__ == "__main__":
    unittest.main()
