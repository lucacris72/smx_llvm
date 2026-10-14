# LLVM RISC-V SMX Patch Notes

This directory tracks the intended LLVM changes before we apply them to an
LLVM checkout.

## Backend Files

Expected areas:

```text
llvm/lib/Target/RISCV/RISCV.td
llvm/lib/Target/RISCV/RISCVInstrInfo*.td
llvm/include/llvm/IR/IntrinsicsRISCV.td
llvm/test/MC/RISCV/
llvm/test/CodeGen/RISCV/
```

## TableGen Shape

The first backend milestone is mnemonic and encoding support:

```text
smx.contx rd, rs1, rs2  opcode CUSTOM1, funct3=7, funct7=0x5a
smx.acc   rd, rs1, rs2  opcode CUSTOM1, funct3=7, funct7=0x5b
smx.exp   rd, rs1, rs2  opcode CUSTOM1, funct3=7, funct7=0x5c
smx.sigm  rd, rs1, rs2  opcode CUSTOM1, funct3=7, funct7=0x5d
```

`smx.contx` and `smx.acc` have accumulator semantics: `rd` is also read by the
hardware. LLVM instruction selection will need to preserve that dependency when
lowering intrinsics.

## Pass Strategy

Do not start by editing the upstream `LoopIdiomRecognize.cpp`. Add a focused
experimental pass named `SMXLoopIdiomRecognize` and register it explicitly.

Initial positive pattern:

```text
countable loop
step 8 induction
two consecutive i32 loads
loop-carried ctx phi
single context update call
no stores, volatile loads, atomics, or unknown side effects
```

