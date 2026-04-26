; RUN: opt -passes=smx-loop-idiom -S %s \
; RUN:   | FileCheck %s

target triple = "riscv32"

declare i32 @__smx_contx_model(i32, i32, i32)

define i32 @no_smx(ptr %in, i32 %n) #0 {
; CHECK-LABEL: define i32 @no_smx(
; CHECK: call i32 @__smx_contx_model(
; CHECK-NOT: call i32 @llvm.riscv.smx.contx(
; CHECK: ret i32
entry:
  br label %loop

loop:
  %ctx = phi i32 [ 32640, %entry ], [ %next, %loop ]
  %i = phi i32 [ 0, %entry ], [ %inc, %loop ]
  %p0.ptr = getelementptr i8, ptr %in, i32 %i
  %p0 = load i32, ptr %p0.ptr, align 4
  %i4 = add i32 %i, 4
  %p1.ptr = getelementptr i8, ptr %in, i32 %i4
  %p1 = load i32, ptr %p1.ptr, align 4
  %next = call i32 @__smx_contx_model(i32 %ctx, i32 %p0, i32 %p1)
  %inc = add i32 %i, 8
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop

exit:
  %ret = phi i32 [ %next, %loop ]
  ret i32 %ret
}

attributes #0 = { "target-features"="+m" }

