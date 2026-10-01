# Step 4: the step0 softmax using Clang builtins

This example reuses `../step0/src/softmax_test.c`, `crt0.S`, `syscalls.c`,
`vectors.S`, and `link.ld`. There is no copied or rewritten softmax algorithm.
The original volatile macros remain the default. Defining
`SMX_USE_LLVM_INTRINSICS=1` selects the existing wrappers in `include/smx_intrin.h`
and refuses to compile if the SMX Clang builtins are unavailable.

```sh
make -C examples/step4 analyze  # Build and compare assembly and optimized IR
make -C examples/step4 check    # Also run Python tests and all three RTL firmwares
```

Three variants are built at `-O2 -g`, without LTO:

| Variant | Compiler | Custom operations |
| --- | --- | --- |
| `gcc-inline` | GNU RISC-V GCC | Original volatile `.insn` |
| `clang-inline` | Course Clang, Xsmx enabled | Same volatile `.insn` |
| `clang-builtin` | Same Clang and flags | Clang builtins via the wrappers |

Clang uses `--target=riscv32-unknown-elf -march=rv32imc_zicsr_xsmx -mabi=ilp32`;
GCC uses `-march=rv32imc_zicsr -mabi=ilp32`. Startup, syscalls, memory-stall
support, GNU linker and C/math runtime are shared across variants. The original
step0 ELF is rebuilt too and decoded with the extended LLVM disassembler as
`build/step0-original.dump`, so custom opcodes have comparable names.

## Artifacts and checks

`build/<variant>.s` is compiler-emitted assembly; `<variant>.dump` decodes only
the application object, and `<variant>.elf.dump` decodes the complete linked
firmware. The static counts in `comparison.md`/`comparison.json` exclude GNU
runtime libraries. They count instruction sites and encoded bytes, not dynamic
executions, total ELF size, or performance. Debug-only local labels do not
split a function in the report. Diff files compare decoded instructions, with
addresses removed; operands still contain object-relative branch addresses,
so layout changes can also appear as differences.

`clang-inline.ll` and `clang-builtin.ll` show optimized LLVM IR. The comparison
checks that the first uses `asm sideeffect` and the second calls all four
`llvm.riscv.smx.*` intrinsics, without an inline-asm fallback. All variants must
contain the four SMX instruction mnemonics in their application disassembly.

The assembly/IR comparison uses **uninstrumented** source. For RTL validation,
separate `.runtime.*` images define `SMX_REPORT_OUTPUT=1` to print the packed
outputs after the computation. Python compares every one of the 15 output
words across the three firmwares, checks `Top-1 FP32: 19, SMX: 19 -> Match: YES`
and requires the simulator's `EXIT SUCCESS`. This is stronger than comparing
Top-1 alone, but it is not an independent numerical softmax oracle. Logs,
traces and RAM dumps live in separate `build/run-*` directories.

## Interpretation and inherited limits

Compare `clang-inline` with `clang-builtin` to isolate the effect of making SMX
operations visible to LLVM. Comparing step0/GCC with Clang also includes
different loop heuristics, inlining and register allocation. A smaller GCC or
Clang output alone does not prove a builtin-specific optimization.

The original test uses `N=63`, processes only `N/4=15` packed words, and leaves
the final three SMX results zero. It assumes aligned packed input and accesses
an int8 buffer through uint32 pointers, an inherited aliasing assumption. This
example preserves those limitations for a controlled assembly comparison;
it does not claim general softmax correctness or support arbitrary N. There
are no LLVM, Clang or hardware source changes in this step.

## Observed assembly differences

Measured on 2026-10-01 with course Clang 18.1.8 and GNU RISC-V GCC 15.1.0.
These counts include the entire application object: the FP32 reference,
quantization, reporting functions and main, not just the SMX computation.

| Variant | Static instructions | Encoded bytes | CONTX sites | ACC sites | EXP sites | SIGM sites |
| --- | --- | --- | --- | --- | --- | --- |
| GCC volatile | 446 | 1356 | 2 | 3 | 1 | 1 |
| Clang volatile | 719 | 2226 | 8 | 15 | 1 | 1 |
| Clang builtin | 738 | 2360 | 8 | 15 | 15 | 15 |

1. GCC keeps compact loops: its two CONTX sites implement the paired-word loop
   and the final odd word; its three ACC sites implement two operations per
   loop iteration and the final odd word. Its EXP/SIGM pair remains a loop.
2. Clang already unrolls the CONTX and ACC passes with volatile asm. Therefore
   their expansion from 2/3 sites to 8/15 sites compared with step0 is a
   compiler-heuristic difference, not proof of a builtin benefit.
3. With builtins, the 15 packed input words are loaded once and retained in
   registers across all three passes. In the Clang volatile variant the input
   words are loaded again for ACC and again for EXP: 45 dynamic packed-input
   loads instead of 15. This reuse is visible in both the IR SSA values and
   disassembly. The intrinsics' `memory(none)` contract exposes the absence of
   memory effects; the opaque side-effecting asm calls do not provide that
   same optimizer contract.
4. The builtin variant completely unrolls the EXP/SIGM loop, removes its
   induction-variable updates and conditional branch, and uses stores at
   constant offsets. There are still 15 EXP and 15 SIGM **executions** in both
   Clang variants; only their static layout changes.
5. There is no elimination of useful SMX results here: all output words are
   consumed. The smaller `mv` count (65 versus 63) is also visible, but does
   not by itself establish better execution time.

The result is genuine compiler optimization (input reuse and less loop
control), **not** a smaller program: the builtin application uses 134 more
encoded bytes than the Clang volatile control. GCC remains much more compact
on this sample. No step4 cycle speedup is inferred from static counts. The
uninstrumented original step0 and step4 GCC variant have identical main counts
(243 instructions, 720 bytes). All 15 packed words and Top-1 matched in the
three instrumented RTL runs.

## Changes to reproduce this step

1. In `examples/step0/src/softmax_test.c`, add the opt-in builtin branch to the
   four existing SMX macros. CONTX and ACC pass the previous destination value
   as an explicit input; EXP and SIGM return a new value. Keep the original
   volatile definitions in the default branch to preserve the baseline.
2. In the same file, add opt-in `SMX_REPORT_OUTPUT` printing after computation.
   Only runtime-validation builds enable it; assembly comparisons do not.
3. Add this directory's `Makefile` to compile the same source in the three
   modes, produce IR/assembly/disassembly, share runtime objects, and rebuild
   the real step0 ELF for comparison.
4. Add `compare.py` to check code generation, generate static metrics and
   decoded diffs, then optionally run all three firmwares and compare output
   words. It reuses `../sim_support.py` for separate run directories and
   explicit simulation-exit validation. The shared helper also keeps partial
   output on timeout and prints the active firmware/log path.
5. Add `test_compare.py` for instruction parsing, missing/duplicate/invalid
   output, Top-1 failure, simulator timeout and a firmware failure despite a
   zero simulator-process exit code. Invoke just these fast tests with
   `make -C examples/step4 test`; invoke everything with `make ... check`.

After changing compiler paths or flags, use `make -C examples/step4 clean`
before rebuilding. The local GNU runtime emits pre-existing syscalls and RWX
linker warnings; this example does not alter that runtime or linker script.
