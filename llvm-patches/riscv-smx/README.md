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
smx.contx rd, rs1, rs2  opcode CUSTOM0, funct3=0, funct7=0, requires +smx
smx.acc   rd, rs1, rs2  opcode CUSTOM0, funct3=1, funct7=0, requires +smx
smx.exp   rd, rs1, rs2  opcode CUSTOM0, funct3=2, funct7=0, requires +smx
smx.sigm  rd, rs1, rs2  opcode CUSTOM0, funct3=3, funct7=0, requires +smx
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
