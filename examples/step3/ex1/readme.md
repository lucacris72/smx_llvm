# Step 3, ex1: lower a simple signed INT8 minimum function

`min_loops.c` contains ordinary C loops and no SMX calls or inline assembly.
The canonical `min_i8` loop is the positive case. Three nearby loops remain
scalar: the load is volatile, the values are unsigned, or the loop also stores.

The function to inspect first is deliberately small:

```c
i8 min_i8(const i8 *a, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}
```

`i8` is a signed byte. There are no includes, runtime libraries, firmware
startup or hardware simulation in the main compiler demonstration.

From the repository root:

```sh
make -C examples/step3/ex1
make -C examples/step3/ex1 check
```

Outputs are in `examples/step3/ex1/build/`:

| Output | What it shows |
| --- | --- |
| `build/min_loops.before.ll` | Clang `-O1` forms `llvm.smin.i8`, before the SMX pass |
| `build/min_loops.pass.ll` | Explicit `opt` run forms packed and scalar paths |
| `build/min_loops.auto.ll` | Clang `-O2 -march=rv32i_xsmx` runs the pass automatically |
| `build/min_loops.auto.s` | Backend emits `smx.contx` only for `min_i8` |
| `build/min_loops.no-xsmx.s` | Same C with `-march=rv32i` stays scalar |
| `build/min_loops.auto.dump` | Disassembly of the generated object with source lines |
| `build/min_loops.remarks.txt` | `-Rpass=smx-min-idiom` reports the recognized loop |

For a short presentation, inspect only `min_i8` in the first five artifacts
in table order. The three functions below it are negative controls, not part
of a more complicated kernel. `checks.txt` ensures their bodies contain no
SMX calls/instructions after the pass. No compiler sources changed when this
example was moved into ex1.

The explicit pass command is `opt -passes='loop-simplify,lcssa,smx-min-idiom'`.
`loop-simplify` and `lcssa` prepare the `-O1` IR in the loop form expected by
the matcher. At `-O2`, the RISC-V target registers the pass in Clang's normal
optimization pipeline, so no `opt` invocation is needed.

The Makefile uses the local course tools, not the system Clang. Generated
artifacts also depend on the corresponding tool binary: rebuilding Clang,
opt or llvm-objdump invalidates its example outputs automatically. Moving or
editing this C example does not require rebuilding LLVM. The compiler-build
integration is explained in [the course roadmap](../../../docs/course-roadmap.md#come-le-modifiche-diventano-tool-utilizzabili).

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
make -C examples/step3/ex1 sim
```

The simulation target uses the existing `examples/smx_min_i8.c` firmware,
whose loop has the same canonical form. It compares the generated code with
an unoptimized scalar reference across 246 cases. This target needs the RTL
simulation toolchain and is separate from the quick compiler tests.

[ex2](../ex2/readme.md) builds a minimum kernel with and without Xsmx, runs both
firmwares on Verilator, compares 60 results, and measures core cycles and speedup.
