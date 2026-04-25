; RUN: llc -mtriple=riscv32 -mattr=+smx -verify-machineinstrs < %s \
; RUN:   | FileCheck %s

declare i32 @llvm.riscv.smx.contx(i32, i32, i32)
declare i32 @llvm.riscv.smx.acc(i32, i32, i32)
declare i32 @llvm.riscv.smx.exp(i32, i32)
declare i32 @llvm.riscv.smx.sigm(i32, i32)

define i32 @test_contx(i32 %ctx, i32 %x, i32 %y) {
; CHECK-LABEL: test_contx:
; CHECK: smx.contx a0, a1, a2
  %r = call i32 @llvm.riscv.smx.contx(i32 %ctx, i32 %x, i32 %y)
  ret i32 %r
}

define i32 @test_acc(i32 %sum, i32 %x, i32 %ctx) {
; CHECK-LABEL: test_acc:
; CHECK: smx.acc a0, a1, a2
  %r = call i32 @llvm.riscv.smx.acc(i32 %sum, i32 %x, i32 %ctx)
  ret i32 %r
}

define i32 @test_exp(i32 %x, i32 %ctx) {
; CHECK-LABEL: test_exp:
; CHECK: smx.exp a0, a0, a1
  %r = call i32 @llvm.riscv.smx.exp(i32 %x, i32 %ctx)
  ret i32 %r
}

define i32 @test_sigm(i32 %sum, i32 %expv) {
; CHECK-LABEL: test_sigm:
; CHECK: smx.sigm a0, a0, a1
  %r = call i32 @llvm.riscv.smx.sigm(i32 %sum, i32 %expv)
  ret i32 %r
}
