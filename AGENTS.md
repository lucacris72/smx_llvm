# SMX compiler course

- The canonical ISA lives in `hardware/smx/isa/smx.json`. Use the pinned
  submodule as the hardware reference; update its gitlink deliberately.
- Encoding v2: CUSTOM1=0x2B, funct3=111, funct7 CONTX=0x5A, ACC=0x5B,
  EXP=0x5C, SIGM=0x5D. Preserve tied read/write rd on CONTX/ACC.
- Generate C/TableGen constants with `make isa-sync`; verify with
  `make isa-check`. Use `include/smx_intrin.h` for inline assembly and builtins.
- This checkout uses `HasXSMX, IsRV32`, `+xsmx` and `rv32..._xsmx`, not the
  older compiler branch's +smx feature. Preserve the course's loop passes.
- Rebuild build/llvm-course tools after backend changes, then run make check,
  check-llvm, check-encoding and check-rtl-isa. Exercise affected course steps;
  do not overwrite published benchmark results merely to test ISA changes.
- Firmware/object rules must depend on encoding headers and the relevant
  compiler binaries so an ISA update cannot reuse stale instruction words.
- examples/sim_support.py explicitly builds SMX=1/PULP_XPULP=0/PULP_CLUSTER=0
  at obj_dir/Vtb_top for its event driver; other core profiles use their own
  configuration directories. Run shared core build/simulation flows in sequence.
- Keep unrelated existing user work and other worktrees unchanged.
