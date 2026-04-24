# SMX LLVM

This repository contains the LLVM/RISC-V integration work for SMX, a compact
RISC-V custom extension for INT8 softmax.

The project is intentionally incremental:

1. Model the four SMX instructions in the LLVM RISC-V backend.
2. Provide C wrappers and a small runtime function that emits SMX operations.
3. Add experimental LLVM loop idiom recognition for canonical softmax context
   reductions.

The local `.context/` directory is ignored by git and is used only as a working
knowledge base.

## SMX Scope

Target:

```text
RISC-V RV32
Opcode: CUSTOM0 / 0x0b / 0001011
Format: R-type
Data: four packed int8 lanes per 32-bit register
```

Instructions:

```asm
smx.contx rd, rs1, rs2
smx.acc   rd, rs1, rs2
smx.exp   rd, rs1, rs2
smx.sigm  rd, rs1, rs2
```

Initial context value:

```c
SMX_CTX_INIT = 0x00007F80u
```

Layout:

```text
[31:20] unused
[19:16] adaptive shift
[15:8]  global min, signed int8
[7:0]   global max, signed int8
```

## Repository Layout

```text
docs/                 Project and ISA notes
include/              Public C headers
src/                  Runtime/library implementation
examples/             Baseline examples and smoke-test kernels
tests/lit/            LLVM lit test drafts for the future LLVM patch
llvm-patches/         Notes and snippets for LLVM backend/pass integration
plan.md               Full project plan
```

## First Build Target

The current runtime layer can be compiled for RISC-V with an assembler that
accepts `.insn r` custom instructions:

```sh
make check
```

Once the LLVM backend learns the SMX mnemonics, define
`SMX_USE_MNEMONICS=1` to emit `smx.contx`, `smx.acc`, `smx.exp`, and
`smx.sigm` directly from inline assembly.

The default RISC-V compiler path is `/opt/riscv/bin/riscv32-unknown-elf-gcc`.
Override it with:

```sh
make RISCV_CC=riscv32-unknown-elf-gcc check
```
