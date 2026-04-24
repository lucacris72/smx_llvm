# RUN: llvm-mc -triple=riscv32 -show-encoding < %s | FileCheck %s

# CHECK: smx.contx a0, a1, a2
# CHECK: smx.acc a0, a1, a2
# CHECK: smx.exp a0, a1, a2
# CHECK: smx.sigm a0, a1, a2
smx.contx a0, a1, a2
smx.acc   a0, a1, a2
smx.exp   a0, a1, a2
smx.sigm  a0, a1, a2

