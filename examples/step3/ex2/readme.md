# Step 3, ex2: scalar versus Xsmx on the RTL core

The same `kernel.c` is compiled by the course Clang at `-O2` twice, with
`-march=rv32imc_zicsr` and `-march=rv32imc_zicsr_xsmx`. Only the second kernel
contains `smx.contx`. Both are linked with the identical `benchmark.o`, startup,
syscalls, linker script, and GNU C runtime. There is no LTO.

```sh
make -C examples/step3/ex2 check
```

The command builds two ELF/HEX images, checks the Python result parser, and runs
both firmwares through the same Verilator model. Each simulation has its own
directory under `build/`, including LUTs, trace, RAM dumps, and `simulation.log`.
The source does not require any additional LLVM or RTL changes.

The input buffer is 257 bytes: vectors up to 256 elements can be tested at
offsets 0 and 1. Three patterns and lengths 0, 1, 7, 8, 9, 15, 16, 31, 255,
and 256 produce 60 cases. The firmware checks a scalar reference; Python also
checks every result against its independently computed reference, then compares
the two firmware outputs. Missing results, a wrong value, a failed firmware,
or an inactive cycle counter fail the command.

Timing uses the core's `mcycle` CSR after clearing bit 0 of `mcountinhibit`.
There is one warm-up call, then 16 calls for each timed combination of length
255/256 and offset 0/1. The measured region includes calls, the repetition loop,
and counter-read overhead; input generation and printing are outside it. The
driver and kernel are separate translation units, so calls cannot be folded
into one calculation. Counter subtraction is unsigned 32-bit, with a simulation
limit far below a complete counter wrap.

`build/results.json` and `build/report.md` contain cycle totals and speedup,
defined as scalar cycles divided by Xsmx cycles. A value below 1 is a slowdown.
These are simulated core cycles, not Python or Verilator wall-clock time.
The original lowering used alignment-1 word loads expanded into byte loads
and shifts. The [step5 compiler](../../step5/readme.md) uses aligned loads
with scalar peeling, so rebuilding with it produces different cycle counts.

## Historical Result Before Step5

Run on 2026-10-01 with the course Clang 18.1.8 (`-O2`), GNU runtime/toolchain
15.1.0, and hardware commit `1d8dff15a190e3e14fa3d1513bc31d5247eee2b7`:

| Offset | Elements | Calls | Scalar cycles | Xsmx cycles | Speedup |
| --- | --- | --- | --- | --- | --- |
| 0 | 255 | 16 | 32966 | 19272 | 1.711x |
| 0 | 256 | 16 | 33094 | 18216 | 1.817x |
| 1 | 255 | 16 | 32966 | 19272 | 1.711x |
| 1 | 256 | 16 | 33094 | 18216 | 1.817x |

All 60 correctness cases passed. Offset 1 has the same timings here because
the generated packed loads are expanded into byte loads for both pointers;
this is not a general statement about unaligned memory accesses. Length 255
requires a scalar tail, while 256 is divisible by the eight-element packed
chunk. Repeat the command above to regenerate the measurements on your build.
