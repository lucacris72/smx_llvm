# Step 2, ex2: optimizer visibility versus volatile assembly

The same `optimization.c` is compiled twice at `-O2` for RV32 Xsmx:

- **builtin**: `SMX_USE_LLVM_INTRINSICS=1` makes `smx_intrin.h` call the Clang
  builtin, which becomes a pure `llvm.riscv.smx.*` intrinsic in LLVM IR.
- **volatile**: `SMX_USE_MNEMONICS=1` selects the existing `asm volatile`
  wrapper, which is represented in IR as `asm sideeffect`.

From the repository root:

```sh
make -C examples/step2/ex2
make -C examples/step2/ex2 check
```

| Function | Builtin at `-O2` | `asm volatile` at `-O2` |
| --- | --- | --- |
| `discarded` ignores the CONTX result | Instruction removed | Instruction retained |
| `repeated` XORs two identical EXP results | Common expression reused; returns zero | Both EXP instructions retained |
| `consumed` returns the CONTX result | Instruction retained | Instruction retained |

Inspect `build/builtin.ll` and `build/volatile.ll` to see intrinsic calls versus
`asm sideeffect`; inspect the corresponding `.s` files for the final machine
instructions. `make check` uses `FileCheck` with `checks.txt` to assert all
three contrasts in both IR and assembly. This example only compiles IR and
assembly: it does not link
or execute a program, and it is not a performance benchmark.

The optimization is correct here because the SMX operations are modeled as
pure register-to-register computations. If an instruction had an externally
visible hardware side effect, `IntrNoMem`/`IntrSpeculatable` and dead-code
elimination would not be a valid model for it. `asm volatile` deliberately
withholds that purity information from the optimizer.
