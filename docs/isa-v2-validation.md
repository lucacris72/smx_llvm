# Course ISA v2 validation — 2026-10-13

The course checkout at base `98d895cd2` still used CUSTOM0/funct7=0;
the migration in the separate `pulp-compatible-isa` branch was not part of
its history. The encoding changes were ported while retaining the course's
`+xsmx`, RV32-only predicates, builtins and SMXMinLoop/extrema transformations.
The local branch is now named `smx-extrema-aligned`.

`hardware/smx` is pinned to
`4cbc02bdc0c2b0ba52fa1c12bc9777c812f7e557`, matching the cluster's `deps/smx`.
The contract is CUSTOM1=0x2B, funct3=7, funct7=0x5A/5B/5C/5D for
CONTX/ACC/EXP/SIGM. C and TableGen constants are generated from `isa/smx.json`.
All old SMX binaries must be rebuilt.

## Verification

The tools in `build/llvm-course` were rebuilt with Ninja before these checks.
The hardware submodule remained clean. Verilator 5.020 and GCC RISC-V 15.1.0
were used for RTL execution and bare-metal linking.

| Command | Result |
| --- | --- |
| `make check isa-check` | PASS: host/GCC wrappers, assembly example and generated contract |
| `make check-llvm check-encoding smx-clang-softmax-asm` | PASS: 8 LLVM lit tests, 32 RV32 operand combinations, RV64 rejection, 132 reserved/legacy words, Clang assembly |
| `make check-rtl-isa` | PASS: Clang firmware executes all four instructions and dependent CONTX/ACC on SMX=1, PULP_XPULP=1, PULP_CLUSTER=1 |
| `make -C examples/step0 all ir` | PASS: GCC firmware, dumps and LLVM IR |
| `make -C examples/step1 all check compare-step0` | PASS: MC encoding/disassembly and comparison with GCC firmware |
| `make -C examples/step2 all check` | PASS: builtin lowering and optimization checks |
| `make -C examples/step3/ex1 check` | PASS: minimum-loop compiler tests |
| `make -C examples/step3/ex2 check` | PASS: 60 RTL results match scalar, SMX and Python reference |
| `make -C examples/step4 all test` | PASS: firmware builds and 20 Python tests |
| `make -C examples/step5/ex1 check` | PASS: extrema-loop compiler tests |
| `make -C examples/step5/ex2 check` | PASS: 8 Python tests and 6,512 cases per RTL firmware, scalar = SMX = reference |
| `make smx-min-core-elf` | PASS: standalone minimum firmware rebuild |

Disassembly of GCC wrappers, the standalone assembly example and the step 0
firmware confirms the new instruction words. Rules now depend on the encoding
headers and relevant compiler binaries to avoid stale objects after migration.
The course simulator helper explicitly selects its SMX-only profile and stable
`obj_dir/Vtb_top` path, as required by its event driver.

The full step 4 benchmark sweep was not rerun; published benchmark results and
compiler transformation sources were preserved. These checks cover single-core
compiler/hardware compatibility, not cluster integration or FPGA synthesis.
