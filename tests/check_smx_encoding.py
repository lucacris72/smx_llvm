#!/usr/bin/env python3
"""Check actual LLVM assembler/disassembler against the canonical SMX contract."""
import argparse
import json
import re
import subprocess
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--smx-root", type=Path, required=True)
    p.add_argument("--llvm-tools", type=Path, required=True)
    a = p.parse_args()
    isa = json.loads((a.smx_root / "isa/smx.json").read_text())
    mc = str(a.llvm_tools / "llvm-mc")
    asm, expected = [], []
    for op in isa["operations"]:
        for rd, rs1, rs2 in [(0,0,0), (31,31,31), (10,11,12), (1,2,3),
                             (10,10,12), (10,11,10), (0,31,1), (31,0,31)]:
            asm.append(f'smx.{op["name"]} x{rd}, x{rs1}, x{rs2}')
            word = (op["funct7"]<<25)|(rs2<<20)|(rs1<<15)|(isa["funct3"]<<12)|(rd<<7)|isa["opcode"]
            expected.append(word.to_bytes(4,"little"))
    for triple in ("riscv32", "riscv64"):
        result = subprocess.run([mc, f"-triple={triple}", "-mattr=+smx", "-show-encoding"],
                                input="\n".join(asm)+"\n", text=True, capture_output=True, check=True)
        encoded = [bytes(int(n,16) for n in line.split(','))
                   for line in re.findall(r"encoding: \[([^]]+)\]",result.stdout)]
        if encoded != expected:
            raise SystemExit(f"LLVM encoding differs from smx.json ({triple})")
    # Neither reserved neighbors nor either old CUSTOM0 encoding may decode as SMX.
    invalid = [((f7<<25)|(12<<20)|(11<<15)|(7<<12)|(10<<7)|0x2b)
               for f7 in range(128) if f7 not in [op['funct7'] for op in isa['operations']]]
    invalid += [(f7<<25)|(12<<20)|(11<<15)|(f3<<12)|(10<<7)|0x0b
                for f7 in (0,90) for f3 in range(4)]
    raw = "\n".join(" ".join(f"0x{b:02x}" for b in n.to_bytes(4,"little")) for n in invalid)
    result = subprocess.run([mc,"-triple=riscv32","-mattr=+smx","-disassemble"],
                            input=raw+"\n", text=True, capture_output=True)
    if "smx." in result.stdout or result.returncode != 0:
        raise SystemExit("Invalid SMX namespace disassembly:\n"+result.stdout+result.stderr)
    print("SMX encoding PASS: 32 operand combinations on RV32/RV64; 132 negative words")


if __name__ == "__main__":
    main()
