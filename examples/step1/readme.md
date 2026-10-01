# Step 1: named SMX instructions in LLVM MC

This example needs only the course LLVM build, not a C library or linker.
The four short functions in `smx_instructions.S` demonstrate the four instruction
spellings. `a0` is read and written by `smx.contx` and `smx.acc`; it is only
written by `smx.exp` and `smx.sigm`. The functions are for inspection, not a
complete softmax program.

From the repository root:

```sh
make -C examples/step1
make -C examples/step1 check
```

The first command invokes Clang's RISC-V integrated assembler with
`-march=rv32i_xsmx`, then runs `llvm-objdump`, `llvm-readelf`, and `llvm-mc`.
The second runs the two LLVM MC regression tests from this milestone. With
`-q`, `llvm-lit` stays quiet on success and exits nonzero on failure.

| File | What to inspect |
| --- | --- |
| `examples/step1/build/smx_instructions.dump` | Four named SMX instructions in the object disassembly |
| `examples/step1/build/smx_instructions.attributes` | `rv32i2p1_xsmx1p0` ELF architecture attribute |
| `examples/step1/build/smx_instructions.encoding` | Encoded instruction bytes reported by `llvm-mc` |

To compare the **same ELF** produced in step 0 with two disassemblers:

```sh
make -C examples/step1 compare-step0
rg -n -m 8 '\.insn' examples/step0/build/softmax_test.dump
rg -n -m 8 'smx\.' examples/step1/build/softmax_test.llvm.dump
```

`compare-step0` updates the step 0 build, then uses the modified
`llvm-objdump` on its GNU-built ELF. That ELF does not declare `xsmx`, so the
command explicitly supplies `--mattr=+xsmx` to decode custom instructions.
No firmware is relinked or executed in this step. The source-annotated dump in
step 0 may contain SMX macro names, but only the LLVM disassembly names the
decoded machine instructions.

The two `smx-*.s` files under `llvm-project/llvm/test/MC/RISCV/` are regression
tests, not firmware examples. They check exact bytes, round-trip disassembly,
and rejection without `xsmx` or on RV64. `llvm-lit` discovers each `RUN:` line
in those files and reports PASS or FAIL; `FileCheck` checks the tool output.
