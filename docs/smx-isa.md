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

## Encoding version 2

The canonical contract is `hardware/smx/isa/smx.json`. C and TableGen encoding
files are generated with `make isa-sync` and verified with `make isa-check`.
This checkout retains `HasXSMX, IsRV32`: use `+xsmx`/`rv32..._xsmx`; RV64 is
rejected. CONTX/ACC still read and write rd, while EXP/SIGM take two inputs.

CUSTOM0 belongs to CORE-V loads/event loads and immediate branches. Their
immediates occupy bits 31:25, so reserving only funct7 there would collide.
CUSTOM1/funct3=111 was reserved in the canonical OpenHW decoder and invalid
as a legacy PULP postincrement store. All other funct7 in that plane stay
reserved. Rebuild all old SMX binaries: their CUSTOM0 words may now decode
as valid CORE-V instructions, so a trap is not guaranteed.

This migration does not make CORE-V and xpulpv2 binaries interchangeable.
The course's loop passes and builtins are unchanged; only encoding, wrappers,
assembly examples, test vectors and the canonical hardware pin are aligned.

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
