"""Compare the unchanged step0 algorithm using opaque asm or SMX builtins."""

import argparse
from collections import Counter
import difflib
import hashlib
import json
from pathlib import Path
import re
import sys

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "examples"))
from sim_support import prepare_simulator, run_firmware

VARIANTS = ("gcc-inline", "clang-inline", "clang-builtin")
SMX = ("smx.contx", "smx.acc", "smx.exp", "smx.sigm")
LABEL = re.compile(r"^[0-9a-f]+ <([^>]+)>:$")
INSTRUCTION = re.compile(r"^\s*[0-9a-f]+:\s+((?:[0-9a-f]{2}\s+)+)(\S+)(?:\s+(.*))?$")
OUTPUT = re.compile(r"^SMX_OUTPUT index=(\d+) value=(\d+)$", re.MULTILINE)
TOP1 = re.compile(r"Top-1 FP32: (\d+), SMX: (\d+) -> Match: YES")


def parse_assembly(text: str) -> list[dict]:
    instructions = []
    function = ""
    for line in text.splitlines():
        label = LABEL.fullmatch(line.strip())
        if label:
            name = label.group(1)
            if not name.startswith((".L", "$")):
                function = name
            continue
        match = INSTRUCTION.fullmatch(line)
        if match:
            raw, mnemonic, operands = match.groups()
            if mnemonic.startswith("."):
                continue
            instructions.append({"function": function, "bytes": len(raw.split()),
                                 "mnemonic": mnemonic, "operands": operands or ""})
    if not instructions:
        raise ValueError("no decoded instructions in the disassembly")
    return instructions


def summarize(instructions: list[dict]) -> dict:
    counts = Counter(i["mnemonic"] for i in instructions)
    return {"instructions": len(instructions), "bytes": sum(i["bytes"] for i in instructions),
            "smx": {name: counts[name] for name in SMX}, "moves": counts["mv"],
            "smx_functions": sorted({i["function"] for i in instructions if i["mnemonic"] in SMX})}


def normalized(instructions: list[dict]) -> list[str]:
    return [f'{i["function"]}: {i["mnemonic"]} {i["operands"]}\n' for i in instructions]


def parse_output(output: str) -> dict:
    words = {}
    for match in OUTPUT.finditer(output):
        index, value = map(int, match.groups())
        if index in words or value > 0xFFFFFFFF:
            raise ValueError("invalid or duplicate packed output")
        words[index] = value
    if set(words) != set(range(63 // 4)):
        raise ValueError("missing packed outputs (expected 15 words for N=63)")
    top1 = TOP1.findall(output)
    if top1 != [("19", "19")]:
        raise ValueError("unexpected Top-1 result")
    return {"words": [words[i] for i in range(63 // 4)], "top1": 19}


def analyze(build: Path) -> str:
    instructions = {v: parse_assembly((build / f"{v}.dump").read_text()) for v in VARIANTS}
    metrics = {v: summarize(instructions[v]) for v in VARIANTS}
    for variant in VARIANTS:
        if any(metrics[variant]["smx"][name] == 0 for name in SMX):
            raise ValueError(f"missing SMX instruction in {variant}")
    inline_ir = (build / "clang-inline.ll").read_text()
    builtin_ir = (build / "clang-builtin.ll").read_text()
    if 'asm sideeffect ".insn' not in inline_ir:
        raise ValueError("volatile asm missing from the control IR")
    if 'asm sideeffect ".insn' in builtin_ir:
        raise ValueError("builtin variant fell back to inline asm")
    for suffix in ("contx", "acc", "exp", "sigm"):
        if f"@llvm.riscv.smx.{suffix}(" not in builtin_ir:
            raise ValueError(f"missing {suffix} intrinsic in builtin IR")
    baseline = parse_assembly((build / "step0-original.dump").read_text())
    metrics["step0-main"] = summarize([i for i in baseline if i["function"] == "main"])
    main_metrics = {}
    for variant in VARIANTS:
        linked = parse_assembly((build / f"{variant}.elf.dump").read_text())
        main_metrics[variant] = summarize([i for i in linked if i["function"] == "main"])
    for control in ("gcc-inline", "clang-inline"):
        diff = difflib.unified_diff(normalized(instructions[control]),
                                    normalized(instructions["clang-builtin"]),
                                    fromfile=control, tofile="clang-builtin")
        (build / f"{control}_vs_builtin.diff").write_text("".join(diff))
    source = REPO / "examples/step0/src/softmax_test.c"
    (build / "comparison.json").write_text(json.dumps({
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "application_objects": {v: metrics[v] for v in VARIANTS},
        "linked_main": {"step0": metrics["step0-main"], **main_metrics},
    }, indent=2) + "\n")
    report = ["# Step0 source: volatile asm versus builtins", "",
              "Static instruction sites in application objects (runtime libraries excluded).", "",
              "| Variant | Instructions | Encoded bytes | mv | contx | acc | exp | sigm |",
              "| --- | --- | --- | --- | --- | --- | --- | --- |"]
    for variant in VARIANTS:
        m = metrics[variant]
        report.append(f'| {variant} | {m["instructions"]} | {m["bytes"]} | {m["moves"]} | ' +
                      " | ".join(str(m["smx"][name]) for name in SMX) + " |")
    report += ["", "Static sites are not dynamic executions or cycle counts.",
               "Only clang-inline versus clang-builtin isolates the builtin effect.", "",
               "| Linked main | Instructions | Encoded bytes |",
               "| --- | --- | --- |"]
    for variant, m in {"step0": metrics["step0-main"], **main_metrics}.items():
        report.append(f'| {variant} | {m["instructions"]} | {m["bytes"]} |')
    text = "\n".join(report) + "\n"
    (build / "comparison.md").write_text(text)
    return text


def simulate(build: Path) -> None:
    core = prepare_simulator(REPO)
    results = {}
    for variant in VARIANTS:
        output = run_firmware(core, build / f"{variant}.runtime.hex", build / f"run-{variant}")
        results[variant] = parse_output(output)
    if any(r != results["gcc-inline"] for r in results.values()):
        raise ValueError("builtin or Clang inline results differ from the GCC baseline")
    (build / "runtime-results.json").write_text(json.dumps(results, indent=2) + "\n")
    print("RTL PASS: all 15 packed words and Top-1 match across the three variants.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).parent / "build")
    parser.add_argument("--simulate", action="store_true")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    print(analyze(build), flush=True)
    if args.simulate:
        simulate(build)


if __name__ == "__main__":
    main()
