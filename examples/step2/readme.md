# Step 2: explicit C operations and optimization

The two examples use the same LLVM/Clang changes from step 2. They do not
require any additional compiler changes.

- [ex1](ex1/readme.md): follow explicit SMX calls from C wrappers through
  Clang builtins, LLVM intrinsics, assembly, and object disassembly.
- [ex2](ex2/readme.md): compile identical C calls through a pure SMX builtin
  and through `asm volatile`, then compare what `-O2` can eliminate or reuse.

From the repository root:

```sh
make -C examples/step2
make -C examples/step2 check
```

The top-level targets run both examples. Each subdirectory can also be built
and checked independently. Neither example links a firmware ELF.
