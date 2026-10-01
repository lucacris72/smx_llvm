# Step 2, ex1: explicit SMX operations from C

Step 1 taught LLVM MC to assemble and disassemble handwritten SMX mnemonics.
This step starts from C instead: `smx_explicit.c` calls the four functions in
`include/smx_intrin.h`. The build defines `SMX_USE_LLVM_INTRINSICS=1`, so those
wrappers call the Xsmx Clang builtins rather than emitting inline assembly.
The preprocessor check in the example prevents a silent fallback to `.insn`.
The function is a small instruction-flow example, not a full softmax.

From the repository root, with the course LLVM build and the step 0 RISC-V
toolchain installed:

```sh
make -C examples/step2/ex1
make -C examples/step2/ex1 check
```

Only the RISC-V sysroot headers are used; this step does not link an ELF or
require a C runtime. The Makefile obtains the sysroot from
`riscv32-unknown-elf-gcc -print-sysroot`.

| File | Compiler stage to inspect |
| --- | --- |
| `examples/step2/ex1/build/smx_explicit.ll` | Clang emits four `llvm.riscv.smx.*` calls |
| `examples/step2/ex1/build/smx_explicit.s` | `llc` selects four `smx.*` instructions |
| `examples/step2/ex1/build/smx_explicit.dump` | The compiled object has named instructions and source locations |
| `examples/step2/ex1/build/smx_explicit.attributes` | ELF architecture includes `xsmx1p0` |
| `examples/step2/ex1/build/smx_explicit.debug-line` | Standard DWARF line table maps code addresses to the C file |

For a concise view of the transformation:

```sh
rg -n 'llvm\.riscv\.smx\.' examples/step2/ex1/build/smx_explicit.ll
rg -n 'smx\.' examples/step2/ex1/build/smx_explicit.s
rg -n 'smx\.' examples/step2/ex1/build/smx_explicit.dump
```

The two tests invoked by `check` live in `clang/test/CodeGen/RISCV/` and
`llvm/test/CodeGen/RISCV/`. The first checks builtin-to-intrinsic lowering;
the second feeds LLVM IR to `llc` and checks intrinsic-to-instruction lowering.
With `-q`, `llvm-lit` exits zero quietly when both pass and reports failures.

The instruction is still requested explicitly by the C code. LLVM does not
recognize a generic min/softmax loop in this step; that is step 3. The C
source and object may have normal `-g` debug information, but there is no
SMX-specific DWARF format or symbol type. In this example, the SMX instruction
addresses map to the inline wrapper lines in `smx_intrin.h`; the `ret` maps to
`smx_explicit.c`.
