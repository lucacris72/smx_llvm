// RUN: %clang_cc1 -triple riscv32 -target-feature +smx -O1 -emit-llvm %s -o - \
// RUN:   | FileCheck %s

typedef unsigned int uint32_t;

// CHECK-LABEL: define{{.*}} i32 @test_contx(
// CHECK: call i32 @llvm.riscv.smx.contx(i32 {{.*}}, i32 {{.*}}, i32 {{.*}})
uint32_t test_contx(uint32_t ctx, uint32_t x, uint32_t y) {
  return __builtin_riscv_smx_contx(ctx, x, y);
}

// CHECK-LABEL: define{{.*}} i32 @test_acc(
// CHECK: call i32 @llvm.riscv.smx.acc(i32 {{.*}}, i32 {{.*}}, i32 {{.*}})
uint32_t test_acc(uint32_t sum, uint32_t x, uint32_t ctx) {
  return __builtin_riscv_smx_acc(sum, x, ctx);
}

// CHECK-LABEL: define{{.*}} i32 @test_exp(
// CHECK: call i32 @llvm.riscv.smx.exp(i32 {{.*}}, i32 {{.*}})
uint32_t test_exp(uint32_t x, uint32_t ctx) {
  return __builtin_riscv_smx_exp(x, ctx);
}

// CHECK-LABEL: define{{.*}} i32 @test_sigm(
// CHECK: call i32 @llvm.riscv.smx.sigm(i32 {{.*}}, i32 {{.*}})
uint32_t test_sigm(uint32_t sum, uint32_t expv) {
  return __builtin_riscv_smx_sigm(sum, expv);
}
