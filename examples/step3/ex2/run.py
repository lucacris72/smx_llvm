"""Compare scalar and automatically lowered min kernels on the RTL core."""

import argparse
import json
from pathlib import Path
import re
import sys

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "examples"))
from sim_support import prepare_simulator, run_firmware

LENGTHS = (0, 1, 7, 8, 9, 15, 16, 31, 255, 256)
RESULT = re.compile(r"^RESULT pattern=(\d+) offset=(\d+) n=(\d+) min=(-?\d+)$", re.MULTILINE)
BENCH = re.compile(r"^BENCH offset=(\d+) n=(\d+) repeats=(\d+) cycles=(\d+) checksum=(-?\d+)$", re.MULTILINE)


def expected_min(pattern: int, offset: int, n: int) -> int:
    values = []
    for i in range(offset, offset + n):
        values.append(((i * 37 + 91) & 255) - 128 if pattern == 0 else
                      -12 if pattern == 1 else -128 if i & 1 else 127)
    return min(values, default=127)


def parse_output(output: str) -> tuple[dict, dict]:
    results = {}
    for match in RESULT.finditer(output):
        pattern, offset, n, value = map(int, match.groups())
        key = (pattern, offset, n)
        if key in results:
            raise ValueError(f"duplicate result {key}")
        results[key] = value
    expected = {(p, o, n): expected_min(p, o, n)
                for p in range(3) for o in range(2) for n in LENGTHS}
    if results != expected:
        raise ValueError("results differ from the independent Python reference")
    benches = {}
    for match in BENCH.finditer(output):
        offset, n, repeats, cycles, checksum = map(int, match.groups())
        key = (offset, n)
        if key in benches or repeats != 16 or cycles <= 0:
            raise ValueError(f"invalid benchmark {key}")
        if checksum != expected_min(0, offset, n) * repeats:
            raise ValueError(f"invalid checksum {key}")
        benches[key] = {"repeats": repeats, "cycles": cycles, "checksum": checksum}
    if set(benches) != {(o, n) for o in range(2) for n in (255, 256)}:
        raise ValueError("missing benchmark measurements")
    if "PASS min-benchmark cases=60" not in output:
        raise ValueError("missing firmware PASS marker")
    return results, benches


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).parent / "build")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    for variant, expected in (("plain", False), ("xsmx", True)):
        has_smx = "smx.contx" in (build / f"{variant}.dump").read_text()
        if has_smx != expected:
            raise ValueError(f"unexpected SMX code generation in {variant}")
    core = prepare_simulator(REPO)
    runs = {}
    outputs = {}
    for variant in ("plain", "xsmx"):
        output = run_firmware(core, build / f"{variant}.hex", build / f"run-{variant}")
        outputs[variant], runs[variant] = parse_output(output)
    if outputs["plain"] != outputs["xsmx"]:
        raise ValueError("scalar and Xsmx outputs differ")
    rows = []
    report = ["# RTL minimum benchmark", "", "60 results match each other and the Python reference.", "",
              "| Offset | N | Repeats | Scalar cycles | Xsmx cycles | Speedup |",
              "| --- | --- | --- | --- | --- | --- |"]
    for key in sorted(runs["plain"]):
        scalar = runs["plain"][key]["cycles"]
        smx = runs["xsmx"][key]["cycles"]
        row = {"offset": key[0], "n": key[1], "repeats": 16,
               "scalar_cycles": scalar, "xsmx_cycles": smx, "speedup": scalar / smx}
        rows.append(row)
        report.append(f"| {key[0]} | {key[1]} | 16 | {scalar} | {smx} | {scalar / smx:.3f}x |")
    (build / "results.json").write_text(json.dumps({"cases": 60, "measurements": rows}, indent=2) + "\n")
    (build / "report.md").write_text("\n".join(report) + "\n")
    print("\n".join(report))


if __name__ == "__main__":
    main()
