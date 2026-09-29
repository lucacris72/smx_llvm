# RUN: not llvm-mc %s -triple=riscv32 2>&1 | FileCheck %s --check-prefix=NOEXT
# RUN: not llvm-mc %s -triple=riscv64 -mattr=+xsmx 2>&1 \
# RUN:   | FileCheck %s --check-prefix=RV64

smx.contx a0, a1, a2
# NOEXT: error: instruction requires the following: 'Xsmx'
# RV64: error: instruction requires the following: RV32I Base Instruction Set
