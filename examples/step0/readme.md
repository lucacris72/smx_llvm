# Step 0: baseline RISC-V toolchain

This example builds `src/softmax_test.c` with the GNU RISC-V toolchain. Inline
`.insn` encodes the SMX instructions, while the linker script places the
bare-metal firmware in the core's memory map. No LLVM SMX support is needed.

From the repository root, with the toolchain at `/opt/riscv`:

```sh
make -C examples/step0
make -C examples/step0 sim
```

The first command builds the ELF and both disassemblies. The second also
creates the Verilog HEX and runs the core with Verilator. Its expected result
is `Top-1 FP32: 19, SMX: 19 -> Match: YES` followed by `EXIT SUCCESS`.

Results are relative to the repository root:

| File | Purpose |
| --- | --- |
| `examples/step0/build/softmax_test.elf` | Linked firmware |
| `examples/step0/build/softmax_test.dump` | Plain disassembly (`objdump -d`) |
| `examples/step0/build/softmax_test.source.dump` | C-annotated disassembly (`objdump -dS`) |
| `examples/step0/build/softmax_test.hex` | Firmware image generated for `sim` |
| `hardware/smx/example_tb/core/trace_core.log` | Simulation trace |

In the plain dump, custom instructions appear as `.insn 4, 0x...`, not as
`smx.contx` and related mnemonics. `-S` adds source lines such as
`SMX_CONTX(...)`, which helps locate the instruction but does not decode it.
That missing mnemonic is the debugging problem addressed in the next step.
