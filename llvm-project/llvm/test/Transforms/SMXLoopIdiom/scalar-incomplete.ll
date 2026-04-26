; RUN: opt -passes=smx-loop-idiom -S %s \
; RUN:   | FileCheck %s

target triple = "riscv32"

declare i32 @llvm.smax.i32(i32, i32)
declare i32 @llvm.smin.i32(i32, i32)

define i32 @max_min_without_shift(ptr %in, i32 %n) #0 {
; CHECK-LABEL: define i32 @max_min_without_shift(
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
  %ctx.max = and i32 %ctx, 255
  %ctx.min.raw = lshr i32 %ctx, 8
  %ctx.min = and i32 %ctx.min.raw, 255
  %max0 = call i32 @llvm.smax.i32(i32 %ctx.max, i32 %p0)
  %max1 = call i32 @llvm.smax.i32(i32 %max0, i32 %p1)
  %min0 = call i32 @llvm.smin.i32(i32 %ctx.min, i32 %p0)
  %min1 = call i32 @llvm.smin.i32(i32 %min0, i32 %p1)
  %min.mask = and i32 %min1, 255
  %min.pack = shl i32 %min.mask, 8
  %max.mask = and i32 %max1, 255
  %next = or i32 %min.pack, %max.mask
  %inc = add i32 %i, 8
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop

exit:
  %ret = phi i32 [ %next, %loop ]
  ret i32 %ret
}

attributes #0 = { "target-features"="+smx" }

