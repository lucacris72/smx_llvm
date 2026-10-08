# Step 3: automatic INT8 minimum lowering

Both examples use the same SMX LLVM pass, with no additional compiler changes.

- [ex1](ex1/readme.md): follow a simple C minimum function through input IR,
  explicit pass execution, automatic Clang lowering, assembly and disassembly.
  The volatile, unsigned and store-containing variants remain scalar.
- [ex2](ex2/readme.md): compile a minimum kernel with and without Xsmx, compare
  both firmwares on the Verilator core, and measure cycles and speedup.

From the repository root:

```sh
make -C examples/step3           # Build ex1 (compiler tools only)
make -C examples/step3 check     # Check ex1 and the LLVM regression tests
make -C examples/step3 ex2       # Build the two RTL benchmark firmwares
make -C examples/step3 check-rtl # Check ex2 and run its two simulations
make -C examples/step3 check-all # Both compiler and RTL checks
```

Each example can also be built and checked directly with `make -C
examples/step3/ex1 check` or `make -C examples/step3/ex2 check`. The top-level
default and `check` intentionally remain fast and do not require the GNU
runtime or Verilator. `make -C examples/step3 sim` retains the optional legacy
246-case firmware check exposed by ex1. `clean` cleans both example builds.

Compiler demonstration artifacts live under `ex1/build/`; the RTL benchmark
outputs live under `ex2/build/`. Neither build directory is versioned.
