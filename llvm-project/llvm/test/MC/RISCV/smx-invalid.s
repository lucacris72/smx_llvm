# RUN: not llvm-mc -triple riscv32 < %s 2>&1 | FileCheck %s
# RUN: not llvm-mc -triple riscv64 < %s 2>&1 | FileCheck %s

smx.contx a0, a1, a2 # CHECK: :[[@LINE]]:1: error: instruction requires the following: 'SMX' (Scale-adaptive INT8 softmax instructions)

