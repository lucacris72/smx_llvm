# Project Setup

## Phase 0 Deliverables

This repository starts with the Phase 0/1 scaffolding needed to make the LLVM
work concrete:

```text
D0.1 LLVM build instructions and local build directory convention
D0.2 clang -> opt -> llc workflow notes
D0.3 baseline SMX assembly kernel
D0.4 short semantic description of the four SMX instructions
```

## Local LLVM Build Convention

Keep LLVM checkouts and build directories outside this repository, or under
ignored `build/` paths. A typical local setup is:

```sh
cmake -S llvm-project/llvm -B build/llvm-riscv \
  -G Ninja \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="RISCV" \
  -DCMAKE_BUILD_TYPE=Release

ninja -C build/llvm-riscv clang opt llc llvm-mc FileCheck
```

## Baseline Flow

Current repository smoke test:

```sh
make check
```

LLVM flow once `clang`/`llc` are available:

```sh
clang --target=riscv32-unknown-elf -march=rv32im -Iinclude \
  -S -emit-llvm examples/smx_intrin_example.c -o build/smx_intrin_example.ll

llc -mtriple=riscv32 -mattr=+m build/smx_intrin_example.ll \
  -o build/smx_intrin_example.s
```

Before the LLVM backend supports SMX mnemonics, the wrappers emit `.insn r`.
After Phase 1, build with `-DSMX_USE_MNEMONICS=1` to exercise the mnemonic
printer/parser path.
