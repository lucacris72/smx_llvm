# SMX LLVM

Il percorso didattico della nuova implementazione e' in
[`docs/course-roadmap.md`](docs/course-roadmap.md). Questa branch ricostruisce
il supporto LLVM passo per passo partendo dall'import LLVM 18; il vecchio
pass SMX resta soltanto nella checkout legacy.

This repository contains the LLVM/RISC-V integration work for SMX, a compact
RISC-V custom extension for INT8 softmax.

The project is intentionally incremental:

1. Model the four SMX instructions in the LLVM RISC-V backend.
2. Provide C wrappers and a small runtime function that emits SMX operations.
3. Add experimental LLVM loop idiom recognition for canonical softmax context
   reductions.

The local `.context/` directory is ignored by git and is used only as a working
knowledge base.

## Course Examples

| Example | Purpose | Command |
| --- | --- | --- |
| [step0](examples/step0/readme.md) | GNU baseline with opaque custom opcodes | `make -C examples/step0` |
| [step1](examples/step1/readme.md) | MC encoding, mnemonics and debug information | `make -C examples/step1 check` |
| [step2](examples/step2/readme.md) | Builtin/IR/backend pipeline and optimizer comparison | `make -C examples/step2 check` |
| [step3/ex1](examples/step3/ex1/readme.md) | Simple C minimum lowered automatically to SMX | `make -C examples/step3/ex1 check` |
| [step3/ex2](examples/step3/ex2/readme.md) | Scalar/Xsmx RTL equivalence, cycles and speedup | `make -C examples/step3/ex2 check` |
| [step4](examples/step4/readme.md) | Builtin IR/assembly and LLVM SMX sweep versus fixed hardware results | `make -s -C examples/step4 run` |

The hardware examples require the initialized RTL submodule, Verilator and
the GNU RISC-V runtime in `/opt/riscv`; compiler examples use the local course
build in `build/llvm-course/bin`. Compilation and simulation artifacts remain
under ignored `build/` directories. Step4 publishes its reference data,
measured results and plots under `examples/step4/results/` and `ground_truth.csv`.

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
hardware/smx/         SMX RTL, CV32E40P integration, Verilator flows
plan.md               Full project plan
```

## RTL Submodule

The SMX hardware repository is tracked as a git submodule under
`hardware/smx`. After cloning this repository, initialize it with:

```sh
git submodule update --init --recursive
```

The submodule provides three useful hardware-side entry points:

```text
hardware/smx/SMX_dev/src       Unit-level SMX functional-unit flow
hardware/smx/example_tb/core   Full-core CV32E40P Verilator flow
hardware/smx/docker/shell.sh   Docker wrapper with Verilator and RISC-V tools
```

The root Makefile exposes thin wrappers around those flows:

```sh
make smx-rtl-unit
make smx-core-hello
make smx-core-softmax
make smx-docker-unit
make smx-docker-core-softmax
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
