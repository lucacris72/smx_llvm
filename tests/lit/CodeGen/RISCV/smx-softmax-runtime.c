// RUN: clang -target riscv32 -march=rv32im_smx -mabi=ilp32 -O2 -S \
// RUN:   -I%repo_root/include -DSMX_USE_LLVM_INTRINSICS=1 \
// RUN:   %repo_root/src/smx_softmax_i8.c -o - | FileCheck %s

// CHECK-LABEL: smx_softmax_i8:
// CHECK: smx.contx
// CHECK: smx.acc
// CHECK: smx.exp
// CHECK: smx.sigm
// CHECK: ret

