"""Validate aligned extrema lowering against scalar RTL and a Python oracle."""

import argparse
import json
from pathlib import Path
import re
import shutil
import sys

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "examples"))
from sim_support import prepare_event_simulator, prepare_simulator, run_firmware

LENGTHS = (0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12, 15, 16, 17, 31, 32, 33,
           63, 64, 65, 255, 256)
NAMES = ("min", "max", "minmax", "min_seed", "max_seed", "minmax_seed",
         "min_first", "max_first", "minmax_first", "minmax_aligned")
BENCH_KEYS = {(kind, offset, n) for kind in (0, 1, 2, 8, 9)
              for offset in range(1 if kind == 9 else 4) for n in (63, 256)}
RESULT = re.compile(r"^RESULT pattern=(\d+) cases=(\d+) hash=([0-9a-f]{8})$", re.MULTILINE)
BENCH = re.compile(r"^BENCH kind=(\d+) offset=(\d+) n=(\d+) repeats=(\d+) cycles=(\d+) checksum=(\d+)$", re.MULTILINE)


def values(pattern: int, n: int) -> list[int]:
    if pattern == 0:
        return [((i * 37 + 91) % 256) - 128 for i in range(n)]
    if pattern == 1:
        return [-12] * n
    if pattern == 2:
        return [127 if i % 2 == 0 else -128 for i in range(n)]
    if pattern == 3:
        return [-128, 127][:n] + [-7] * max(0, n - 2)
    if pattern == 4:
        return [5] * max(0, n - 2) + ([-128, 127][-n:] if n else [])
    if pattern == 5:
        return [i % 255 - 127 for i in range(n)]
    return [127 if pattern == 6 else -128] * n


def expected_pair(kind: int, pattern: int, n: int) -> tuple[int, int]:
    data = values(pattern, n)
    lo, hi = min(data, default=127), max(data, default=-128)
    if kind in (3, 4, 5):
        lo = min(lo, (0, -40, 127)[pattern % 3])
        hi = max(hi, (0, 40, -128)[pattern % 3])
    if kind in (0, 3, 6):
        hi = -128
    if kind in (1, 4, 7):
        lo = 127
    return lo, hi


def expected_results() -> dict[int, tuple[int, int]]:
    results = {}
    for pattern in range(8):
        count, digest = 0, 2166136261
        for offset in range(4):
            for n in LENGTHS:
                for kind in range(9 if offset else 10):
                    for value in expected_pair(kind, pattern, n):
                        digest = ((digest ^ (value & 255)) * 16777619) & 0xffffffff
                    count += 1
        results[pattern] = count, digest
    return results


def expected_checksum(kind: int, n: int) -> int:
    lo, hi = expected_pair(kind, 0, n)
    return ((lo & 255) | ((hi & 255) << 8)) * 16


def parse_output(output: str) -> tuple[dict, dict]:
    if "FAIL" in output:
        raise ValueError("firmware reported a failure")
    results = {}
    for match in RESULT.finditer(output):
        pattern, count, digest = match.groups()
        key = int(pattern)
        if key in results:
            raise ValueError(f"duplicate pattern {key}")
        results[key] = int(count), int(digest, 16)
    expected = expected_results()
    if results != expected:
        raise ValueError("result counts or hashes differ from the Python oracle")
    total = sum(count for count, _ in expected.values())
    if f"PASS extrema-benchmark cases={total}" not in output:
        raise ValueError("missing firmware PASS marker")
    measurements = {}
    for match in BENCH.finditer(output):
        kind, offset, n, repeats, cycles, checksum = map(int, match.groups())
        key = kind, offset, n
        if key not in BENCH_KEYS or key in measurements or repeats != 16 or cycles <= 0:
            raise ValueError(f"invalid benchmark measurement {key}")
        if checksum != expected_checksum(kind, n):
            raise ValueError(f"invalid benchmark checksum {key}")
        measurements[key] = cycles
    if set(measurements) != BENCH_KEYS:
        raise ValueError("missing benchmark measurements")
    return results, measurements


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).parent / "build")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    for variant, count in (("plain", 0), ("xsmx", len(NAMES))):
        dump = (build / f"{variant}.dump").read_text()
        if dump.count("smx.contx") != count:
            raise ValueError(f"expected {count} CONTX instructions in {variant}")
    core = prepare_simulator(REPO, log=build / "simulator-build.log")
    fast = prepare_event_simulator(core, build)
    for name in ("smx_lut1d.hex", "smx_lut2d.hex"):
        shutil.copy2(core / name, fast / name)
    results, measurements = {}, {}
    for variant in ("plain", "xsmx"):
        run = build / f"run-{variant}"
        run.mkdir(parents=True, exist_ok=True)
        trace = run / "trace_core.log"
        if not trace.exists():
            trace.symlink_to("/dev/null")
        output = run_firmware(fast, build / f"{variant}.hex", run, timeout=240,
                              maxcycles=30_000_000, quiet=True)
        results[variant], measurements[variant] = parse_output(output)
    if results["plain"] != results["xsmx"]:
        raise ValueError("scalar and SMX firmware results differ")
    cases = sum(count for count, _ in results["plain"].values())
    rows = []
    report = ["# Step5 RTL results", "",
              f"{cases} kernel calls per firmware pass the C reference and Python hashes.",
              "Same Clang -O2, source, runtime and RTL; only the kernel ISA flag differs.",
              "Cycles include 16 calls, dispatch, repetition and counter overhead.", "",
              "| Kernel | Offset | N | Scalar cycles | SMX cycles | Speedup |",
              "| --- | ---: | ---: | ---: | ---: | ---: |"]
    for key in sorted(BENCH_KEYS):
        scalar, smx = measurements["plain"][key], measurements["xsmx"][key]
        kind, offset, n = key
        row = {"kernel": NAMES[kind], "offset": offset, "n": n, "repeats": 16,
               "scalar_cycles": scalar, "smx_cycles": smx, "speedup": scalar / smx}
        rows.append(row)
        report.append(f"| {NAMES[kind]} | {offset} | {n} | {scalar} | {smx} | {scalar / smx:.3f}x |")
    (build / "results.json").write_text(json.dumps({"cases": cases, "measurements": rows}, indent=2) + "\n")
    (build / "report.md").write_text("\n".join(report) + "\n")
    print(f"PASS step5/ex2: {cases} cases per firmware; scalar = SMX = reference")
    for row in rows:
        if row["offset"] == 0 and row["n"] == 256:
            print(f'{row["kernel"]:16s} N=256: {row["scalar_cycles"]} / {row["smx_cycles"]} cycles, {row["speedup"]:.3f}x')
    print(f"Full results: {build / 'report.md'}")


if __name__ == "__main__":
    main()
