# SMX compiler contract

- The canonical encoding lives in hardware/smx/isa/smx.json, not in this repo.
  SMX ISA v2 uses CUSTOM1=0x2B, funct3=111 and funct7=0x5A..0x5D.
- `make isa-sync` updates generated C/TableGen snapshots; `make isa-check`
  detects drift. SMX_RTL_DIR can select the live canonical development checkout.
- Preserve tied rd read/write operands for CONTX/ACC. EXP/SIGM have two inputs.
- Rebuild LLVM after changes to TableGen, then run make check, check-llvm,
  check-encoding and check-rtl-isa. Never assume existing tool binaries are fresh.
- CORE-V/PULP compiler support is separate from +smx. Do not claim xpulpv2
  binary compatibility for the canonical OpenHW core.
- Keep unrelated existing loop-recognition work out of encoding commits.
