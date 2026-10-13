# SMX ISA Notes

SMX exposes the adaptive INT8 softmax pipeline as four RISC-V custom
instructions. All instructions use the R-type encoding in the CUSTOM1 opcode
space.

## Encoding

```text
opcode  = 0x2b (CUSTOM1)
funct7  = operation selector (0x5a..0x5d)
funct3  = 7 (111)
format  = R-type: rd, rs1, rs2
```

| Instruction | funct7 | Purpose |
| --- | ---: | --- |
| `smx.contx` | 0x5a | Update packed context: global max, global min, adaptive shift |
| `smx.acc` | 0x5b | Accumulate exponential LUT values into a 32-bit sum |
| `smx.exp` | 0x5c | Produce four packed quantized exponential values |
| `smx.sigm` | 0x5d | Produce four packed INT8 softmax outputs from sum and exp vector |

## Version and compatibility

Encoding version 2 is defined in `hardware/smx/isa/smx.json`. Run `make isa-sync`
after an intentional contract update and `make isa-check` to detect divergence.
`SMX_RTL_DIR=/path/to/smx` selects a live local source tree instead of the pin.
The generated C header and TableGen constants are versioned snapshots; normal
builds check them, never silently migrate the ISA.

This replaces both old CUSTOM0 encodings (funct7=0 in smx/LLVM and funct7=0x5a
in the previous multicore project). Rebuild all objects, libraries and firmware.
An old SMX word can now be a valid CORE-V load or branch: do not expect a trap.
The reserved CUSTOM1/funct3=111 plane avoids the load/immediate overlap and is
also invalid as a legacy PULP postincrement store (size=11).
CORE-V and older PULP ISA binaries are not generally interchangeable; this
change restores the native CORE-V instruction set of the canonical CV32E40P.
The LLVM +smx feature adds SMX only, not an automatic xpulpv2 toolchain port.

## Context Register

`smx.contx` treats `rd` as a read/write accumulator. Initialize it to:

```text
0x00007F80
```

This represents:

```text
max   = -128 -> bits [7:0]  = 0x80
min   =  127 -> bits [15:8] = 0x7f
shift =    0 -> bits [19:16]
```

After the context pass:

```text
[31:20] zero/unused
[19:16] shift amount
[15:8]  global min
[7:0]   global max
```

The RTL reference for this calculation is in:

```text
hardware/smx/SMX_dev/src/rtl/smx_fu_lut.sv
```

`smx.contx` computes max, min, and adaptive shift. A compiler transform must
therefore preserve all three fields; recognizing only a max/min reduction is
not semantically equivalent to `smx.contx`.

## Softmax Kernel Structure

The runtime sequence is three passes over the input:

1. `smx.contx`: consume 8 INT8 elements per iteration using two packed words.
2. `smx.acc`: consume 4 INT8 elements per iteration and accumulate the global
   normalization sum.
3. `smx.exp` + `smx.sigm`: consume 4 INT8 elements per iteration and store four
   packed output bytes.

Initial implementation preconditions:

```text
n is a multiple of 8
input and output are 4-byte aligned
input and output point to at least n bytes
input bytes are interpreted as signed int8 values
```

The LLVM implementation status and test commands are documented in
`docs/smx-llvm-implementation.md`.
