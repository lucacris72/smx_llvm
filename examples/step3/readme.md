# Step 3: recognize a signed INT8 minimum loop

`min_loops.c` contains ordinary C loops and no SMX calls or inline assembly.
The canonical `min_i8` loop is the positive case. Three nearby loops remain
scalar: the load is volatile, the values are unsigned, or the loop also stores.

From the repository root:

```sh
make -C examples/step3
make -C examples/step3 check
```

| Output | What it shows |
| --- | --- |
| `build/min_loops.before.ll` | Clang `-O1` forms `llvm.smin.i8`, before the SMX pass |
| `build/min_loops.pass.ll` | Explicit `opt` run forms packed and scalar paths |
| `build/min_loops.auto.ll` | Clang `-O2 -march=rv32i_xsmx` runs the pass automatically |
| `build/min_loops.auto.s` | Backend emits `smx.contx` only for `min_i8` |
| `build/min_loops.no-xsmx.s` | Same C with `-march=rv32i` stays scalar |
| `build/min_loops.auto.dump` | Disassembly of the generated object with source lines |
| `build/min_loops.remarks.txt` | `-Rpass=smx-min-idiom` reports the recognized loop |

The explicit pass command is `opt -passes='loop-simplify,lcssa,smx-min-idiom'`.
`loop-simplify` and `lcssa` prepare the `-O1` IR in the loop form expected by
the matcher. At `-O2`, the RISC-V target registers the pass in Clang's normal
optimization pipeline, so no `opt` invocation is needed.

For each iteration, the pass checks whether at least eight elements remain.
The packed path loads two 32-bit words with alignment 1, calls `smx.contx`,
and extracts the signed minimum from bits 15:8 of its context. The original
scalar reduction handles the final 0-7 elements. The original zero-length
guard remains in place and returns the initial value 127.

The two `load i32 align 1` operations are visible in LLVM IR. On this RV32I
target the backend expands each unaligned word load into byte loads and shifts;
`smx.contx` is still emitted, but this example does not claim a speedup.

`make check` verifies the example IR/assembly with `FileCheck` and runs the
two LLVM regression tests for this milestone. For an optional hardware check:

```sh
make -C examples/step3 sim
```

The simulation target uses the existing `examples/smx_min_i8.c` firmware,
whose loop has the same canonical form. It compares the generated code with
an unoptimized scalar reference across 246 cases. This target needs the RTL
simulation toolchain and is separate from the quick compiler tests.

[ex2](ex2/readme.md) builds the same kernel with and without Xsmx, runs both
firmwares on Verilator, compares 60 results, and measures core cycles and speedup.
