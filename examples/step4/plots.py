"""Plot fixed historical references with the measured SMX LLVM curve."""

import argparse
from io import StringIO
import json
import math
import os
from pathlib import Path

from benchmark import GROUND_TRUTH, SIZES, STEP4, comparison, digest, load_ground_truth

os.environ.setdefault("MPLCONFIGDIR", str(STEP4 / "build/mpl-cache"))
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator

plt.rcParams.update({"svg.fonttype": "none", "svg.hashsalt": "smx-step4", "font.size": 11})


def load_results(directory: Path) -> list[dict]:
    results = json.loads((directory / "results.json").read_text())
    if results["metadata"]["ground_truth_sha256"] != digest(GROUND_TRUTH):
        raise ValueError("results belong to a different ground truth; rerun make run")
    rows = results["rows"]
    if tuple(row["N"] for row in rows) != SIZES:
        raise ValueError("plots require the complete eight-size sweep")
    for old, row in zip(load_ground_truth(), rows):
        if any(row[key] != value for key, value in old.items()):
            raise ValueError("historical results were modified")
        if not isinstance(row["SMX_LLVM"], int) or not 0 < row["SMX_LLVM"] < 2**64:
            raise ValueError("invalid LLVM cycle count")
        expected = comparison(old, row)
        for key in ("SMX_old_over_LLVM", "FP32_ground_truth_over_LLVM", "SW_ground_truth_over_LLVM"):
            if not math.isclose(row[key], expected[key], rel_tol=1e-12):
                raise ValueError("speedups do not use the fixed historical references")
    return rows


def dimension_axis(ax, scale: str) -> None:
    if scale == "linear":
        ax.set_xscale("linear")
        ax.set_xlim(0, 1050)
        ax.xaxis.set_major_locator(MultipleLocator(128))
    else:
        ax.set_xscale("log", base=2)
        ax.set_xticks(SIZES, labels=[str(n) for n in SIZES])
        ax.minorticks_off()
    ax.set_xlabel("Vector Dimension (N)")


def cycles(ax, rows: list[dict], scale: str) -> None:
    for key, style, color, label in (
        ("FP32", "o--", "red", "FP32 (SoftFloat, ground truth)"),
        ("SW_Int8", "s--", "blue", "SW Model (Int8, ground truth)"),
        ("SMX", "^-", "green", "SMX ground truth"),
        ("SMX_LLVM", "D--", "#222222", "SMX LLVM (builtins)"),
    ):
        ax.plot(SIZES, [row[key] for row in rows], style, color=color, label=label, linewidth=2)
    dimension_axis(ax, scale)
    ax.set_yscale("log")
    ax.set_ylabel("Cycles (Log Scale)")
    ax.set_title("Softmax: historical results + SMX LLVM")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=10)


def speedup(ax, rows: list[dict], reference: str, scale: str) -> None:
    color = "purple" if reference == "FP32" else "orange"
    title = "FP32" if reference == "FP32" else "SW Int8"
    ax.plot(SIZES, [row[reference] / row["SMX"] for row in rows],
            "o-", color=color, label="SMX ground truth", linewidth=2)
    ax.plot(SIZES, [row[reference] / row["SMX_LLVM"] for row in rows],
            "D--", color="#222222", label="SMX LLVM (builtins)", linewidth=2)
    dimension_axis(ax, scale)
    ax.set_ylim(bottom=0)
    ax.set_ylabel("Speedup (x)")
    ax.set_title(f"SMX speedup vs historical {title}")
    ax.grid(True, alpha=0.3)
    ax.legend()


def save(fig, directory: Path, name: str) -> None:
    fig.tight_layout()
    fig.savefig(directory / f"{name}.png", dpi=180, facecolor="white")
    svg = StringIO()
    fig.savefig(svg, format="svg", facecolor="white", metadata={"Date": None})
    (directory / f"{name}.svg").write_text("\n".join(line.rstrip() for line in svg.getvalue().splitlines()) + "\n")
    plt.close(fig)


def generate(rows: list[dict], directory: Path) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    for scale, suffix in (("log", ""), ("linear", "_linear")):
        fig, ax = plt.subplots(figsize=(11, 5.8))
        cycles(ax, rows, scale)
        save(fig, directory, "cycles" + suffix)

        fig, axes = plt.subplots(2, 1, figsize=(11, 8.5), sharex=True)
        speedup(axes[0], rows, "FP32", scale)
        speedup(axes[1], rows, "SW_Int8", scale)
        save(fig, directory, "speedup" + suffix)

        fig, axes = plt.subplots(2, 1, figsize=(11, 10))
        cycles(axes[0], rows, scale)
        ax = axes[1]
        for reference, color in (("FP32", "purple"), ("SW_Int8", "orange")):
            for key, style, label in (("SMX", "o-", "Ground truth"), ("SMX_LLVM", "D--", "LLVM")):
                ax.plot(SIZES, [row[reference] / row[key] for row in rows], style,
                        color=color, label=f"{label} vs {reference}")
        dimension_axis(ax, scale)
        ax.set_ylim(bottom=0)
        ax.set_ylabel("Speedup (x)")
        ax.set_title("SMX speedup (fixed historical references)")
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=10)
        save(fig, directory, "softmax_performance" + suffix)

    fig, ax = plt.subplots(figsize=(11, 5.8))
    ax.plot(SIZES, [row["SMX"] for row in rows], "^-", color="green", label="SMX ground truth", linewidth=2)
    ax.plot(SIZES, [row["SMX_LLVM"] for row in rows], "D--", color="#222222", label="SMX LLVM (builtins)", linewidth=2)
    dimension_axis(ax, "linear")
    ax.set_ylim(0, max(max(row["SMX"], row["SMX_LLVM"]) for row in rows) * 1.12)
    ax.set_ylabel("Cycles")
    ax.set_title("SMX vs SMX LLVM (linear axes)")
    ax.grid(True, alpha=0.3)
    ax.legend(loc="upper left")
    for n, position in ((128, (80, 1500)), (1024, (710, 2850))):
        row = next(row for row in rows if row["N"] == n)
        ax.annotate(f'N={n}: {row["SMX_old_over_LLVM"]:.2f}x', xy=(n, row["SMX_LLVM"]),
                    xytext=position, fontsize=10, arrowprops={"arrowstyle": "->", "color": "#555555"})
    save(fig, directory, "smx_only_linear")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", type=Path, default=STEP4 / "results")
    args = parser.parse_args()
    try:
        rows = load_results(args.results_dir)
        generate(rows, args.results_dir / "plots")
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"ERROR: {error}\n")
    print(f"Plots: {args.results_dir / 'plots'}")


if __name__ == "__main__":
    main()
