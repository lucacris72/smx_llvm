; RUN: opt -passes=smx-min-idiom -verify-each -S %s \
; RUN:   | FileCheck %s

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32"

declare i8 @llvm.smin.i8(i8, i8)

define i8 @min_i8(ptr %a, i32 %n) #0 {
; CHECK-LABEL: define i8 @min_i8(
; CHECK: smx.packed:
; CHECK: load i32, ptr {{.*}}, align 4
; CHECK: load i32, ptr {{.*}}, align 4
; CHECK: call i32 @llvm.riscv.smx.contx(
; CHECK: smx.scalar:
; CHECK: call i8 @llvm.smin.i8(
entry:
  %empty = icmp eq i32 %n, 0
  br i1 %empty, label %ret, label %preheader

preheader:
  br label %loop

ret:
  %result = phi i8 [ 127, %entry ], [ %lcssa, %loopexit ]
  ret i8 %result

loopexit:
  %lcssa = phi i8 [ %next, %loop ]
  br label %ret

loop:
  %i = phi i32 [ 0, %preheader ], [ %inc, %loop ]
  %min = phi i8 [ 127, %preheader ], [ %next, %loop ]
  %ptr = getelementptr inbounds i8, ptr %a, i32 %i
  %value = load i8, ptr %ptr, align 1
  %next = call i8 @llvm.smin.i8(i8 %min, i8 %value)
  %inc = add nuw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %loopexit, label %loop
}

attributes #0 = { "target-features"="+xsmx" }
