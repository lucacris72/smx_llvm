"""Measure only SMX LLVM against the hardware project's fixed historical CSV."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

STEP4 = Path(__file__).resolve().parent
REPO = STEP4.parents[1]
GROUND_TRUTH = STEP4 / "ground_truth.csv"
SIZES = (8, 16, 32, 64, 128, 256, 512, 1024)
sys.path.insert(0, str(REPO / "examples"))
from sim_support import prepare_simulator, run_firmware, run_logged, validate_exit


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_ground_truth(path: Path = GROUND_TRUTH) -> list[dict]:
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != ["N", "FP32", "SMX", "SW_Int8"]:
            raise ValueError("unexpected ground truth columns")
        rows = []
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise ValueError("malformed ground truth row")
            rows.append({key: int(value) for key, value in row.items()})
    if tuple(row["N"] for row in rows) != SIZES or any(
        value <= 0 for row in rows for value in row.values()
    ):
        raise ValueError("invalid or duplicate historical benchmark dimensions/cycles")
    return rows


def configure(n: int, output: Path) -> None:
    if n < 4:
        raise ValueError("N must be an integer >= 4 for IR/assembly inspection")
    contents = f"#define N {n}u\n"
    if output.exists() and output.read_text() == contents:
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(contents)


def load_luts(core: Path) -> tuple[list[int], list[int]]:
    tables = [[int(token, 16) for token in (core / name).read_text().split()]
              for name in ("smx_lut1d.hex", "smx_lut2d.hex")]
    if [len(table) for table in tables] != [101, 660] or any(
        not 0 <= value <= 255 for table in tables for value in table
    ):
        raise ValueError("invalid RTL LUT contents or dimensions")
    return tables[0], tables[1]


def reference_words(n: int, lut1: list[int], lut2: list[int]) -> list[int]:
    if n not in SIZES:
        raise ValueError("benchmark dimension is not present in the ground truth")
    # Quantization is exact for the original quarter-step floating-point input.
    inputs = [((i * 37) % 64) * 4 - 128 for i in range(n)]
    high, low = max(inputs), min(inputs)
    shift = 8 - (high - low).bit_length() - 1
    exps = []
    for value in inputs:
        delta = high - value
        index = delta >> -shift if shift < 0 else delta << shift
        exps.append(lut1[min(index, 100)])
    row = min(sum(exps) >> 8, 10)
    probabilities = [lut2[row * 60 + min(value >> 2, 59)] for value in exps]
    return [sum(probabilities[i + j] << (j * 8) for j in range(4))
            for i in range(0, n, 4)]


def parse_output(output: str, n: int, expected: list[int]) -> dict:
    validate_exit(output)
    timing, words = [], {}
    for line in output.splitlines():
        if line.startswith("SMX_TIMING"):
            match = re.fullmatch(r"SMX_TIMING n=(\d+) cycles=(\d+)", line)
            if not match or int(match[1]) != n or not 0 < int(match[2]) < 2**64:
                raise ValueError("invalid firmware size or SMX timing")
            timing.append(int(match[2]))
        elif line.startswith("SMX_OUTPUT"):
            match = re.fullmatch(r"SMX_OUTPUT index=(\d+) value=(\d+)", line)
            if not match or int(match[1]) in words or not 0 <= int(match[2]) < 2**32:
                raise ValueError("malformed or duplicate packed output")
            words[int(match[1])] = int(match[2])
    if len(timing) != 1 or set(words) != set(range(n // 4)) or len(expected) != n // 4:
        raise ValueError("missing/duplicate timing or packed output words")
    packed = [words[i] for i in range(n // 4)]
    if packed != expected:
        raise ValueError("SMX output differs from the host LUT software model")
    probabilities = [word >> (byte * 8) & 255 for word in packed for byte in range(4)]
    top1 = probabilities.index(max(probabilities))
    if top1 != max(range(min(n, 64)), key=lambda i: (i * 37) % 64):
        raise ValueError("unexpected Top-1 index")
    return {"N": n, "SMX_LLVM": timing[0], "top1": top1, "packed": packed}


def comparison(old: dict, measured: dict) -> dict:
    if old["N"] != measured["N"]:
        raise ValueError("historical and measured dimensions differ")
    cycles = measured["SMX_LLVM"]
    return old | measured | {
        "SMX_old_over_LLVM": old["SMX"] / cycles,
        "FP32_ground_truth_over_LLVM": old["FP32"] / cycles,
        "SW_ground_truth_over_LLVM": old["SW_Int8"] / cycles,
    }


def prepare_event_simulator(core: Path, build: Path) -> Path:
    target = build / "simulator"
    stamp = target / "signature.json"
    signature = {"rtl_model": digest(core / "obj_dir/Vtb_top"),
                 "hardware_driver": digest(core / "sim_main.cpp"),
                 "event_driver": digest(STEP4 / "event_main.cpp")}
    if stamp.exists() and json.loads(stamp.read_text()) == signature and (target / "obj_dir/Vtb_top").exists():
        return target
    # Only the C++ driver is rebuilt; the generated RTL model is copied unchanged.
    shutil.copytree(core / "obj_dir", target / "obj_dir", dirs_exist_ok=True)
    run_logged([
        "make", "-s", "-C", str(target / "obj_dir"), "-f", "Vtb_top.mk", "Vtb_top",
        "VM_USER_CLASSES=event_main", f"VM_USER_DIR={STEP4}",
        f'VM_USER_CFLAGS=-O2 -std=c++20 -I"{core}"',
    ], build / "simulator-build.log")
    stamp.write_text(json.dumps(signature, indent=2) + "\n")
    return target


def run_image(core: Path, firmware: Path, run: Path) -> str:
    run.mkdir(parents=True, exist_ok=True)
    trace = run / "trace_core.log"
    if not trace.exists():
        trace.symlink_to("/dev/null")
    return run_firmware(core, firmware, run, timeout=180, maxcycles=5_000_000, quiet=True)


def result_text(rows: list[dict]) -> str:
    lines = ["# Benchmark Results", "", "FP32, SW Int8 and SMX baseline are fixed historical data.",
             "Only SMX LLVM is measured: one call, -O3, RV32IMC_Zicsr_Xsmx, mcycle64.", "",
             "| N | SMX historical | SMX LLVM | Historical / LLVM | FP32 historical / LLVM | SW historical / LLVM |",
             "|---:|---:|---:|---:|---:|---:|"]
    for row in rows:
        lines.append(f'| {row["N"]} | {row["SMX"]} | {row["SMX_LLVM"]} | '
                     f'{row["SMX_old_over_LLVM"]:.2f}x | {row["FP32_ground_truth_over_LLVM"]:.2f}x | '
                     f'{row["SW_ground_truth_over_LLVM"]:.2f}x |')
    lines += ["", "PASS: every packed output matches the host LUT model; Top-1 is correct for all sizes.",
              "Initialization/quantization, printing and validation are outside the timer.",
              "Counter overhead is included. FP32 historical data uses soft-float.",
              "This is a comparison to historical results, not an isolated attribution to LLVM builtins.", ""]
    return "\n".join(lines)


def save_results(directory: Path, rows: list[dict], metadata: dict) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "results.json").write_text(json.dumps({"metadata": metadata, "rows": rows}, indent=2) + "\n")
    columns = ["N", "FP32", "SMX", "SW_Int8", "SMX_LLVM", "SMX_old_over_LLVM",
               "FP32_ground_truth_over_LLVM", "SW_ground_truth_over_LLVM"]
    with (directory / "results.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns, extrasaction="ignore", lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    (directory / "README.md").write_text(result_text(rows))


def simulate(build: Path, results: Path, clang: Path) -> None:
    before = digest(GROUND_TRUTH)
    historical = load_ground_truth()
    core = prepare_simulator(REPO, log=build / "simulator-build.log")
    fast = prepare_event_simulator(core, build)
    for name in ("smx_lut1d.hex", "smx_lut2d.hex"):
        shutil.copy2(core / name, fast / name)
    lut1, lut2 = load_luts(core)
    rows = []
    for old in historical:
        n = old["N"]
        work = build / "bench" / str(n)
        disassembly = (work / "benchmark.dump").read_text()
        if not all(f"smx.{name}" in disassembly for name in ("contx", "acc", "exp", "sigm")):
            raise ValueError(f"firmware N={n} is missing SMX instructions")
        expected = reference_words(n, lut1, lut2)
        output = run_image(fast, work / "benchmark.hex", work / "run")
        measured = parse_output(output, n, expected)
        if n == SIZES[0]:
            original = run_image(core, work / "benchmark.hex", work / "original_driver_check")
            if parse_output(original, n, expected) != measured or (
                (work / "run/sim_ram.bin").read_bytes() !=
                (work / "original_driver_check/sim_ram.bin").read_bytes()
            ):
                raise ValueError("event-driven simulation differs from the hardware driver")
        rows.append(comparison(old, measured))
    if digest(GROUND_TRUTH) != before:
        raise ValueError("historical ground truth changed during the sweep")
    metadata = {"ground_truth_sha256": before, "benchmark_sha256": digest(STEP4 / "benchmark.c"),
                "clang": subprocess.check_output([str(clang), "--version"], text=True).splitlines()[0],
                "hardware_revision": subprocess.check_output(
                    ["git", "-C", str(REPO / "hardware/smx"), "rev-parse", "HEAD"], text=True).strip(),
                "optimization": "-O3", "arch": "rv32imc_zicsr_xsmx", "abi": "ilp32", "repeat": 1,
                "original_event_driver_cycles_and_ram_identical_N8": True,
                "only_smx_remeasured": True}
    save_results(results, rows, metadata)
    print(f"PASS: {len(rows)} SMX sizes, packed outputs and simulator equivalence; historical data unchanged.")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    run = commands.add_parser("run")
    run.add_argument("--build-dir", type=Path, required=True)
    run.add_argument("--results-dir", type=Path, required=True)
    run.add_argument("--clang", type=Path, required=True)
    config = commands.add_parser("configure")
    config.add_argument("--n", type=int, required=True)
    config.add_argument("--output", type=Path, required=True)
    execute = commands.add_parser("exec")
    execute.add_argument("--log", type=Path, required=True)
    execute.add_argument("--output", type=Path)
    execute.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        if args.action == "configure":
            configure(args.n, args.output)
        elif args.action == "exec":
            command = args.command[1:] if args.command[:1] == ["--"] else args.command
            if not command:
                parser.error("missing build command")
            run_logged(command, args.log, args.output)
        else:
            simulate(args.build_dir.resolve(), args.results_dir.resolve(), args.clang.resolve())
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"ERROR: {error}\n")


if __name__ == "__main__":
    main()
