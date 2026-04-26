; RUN: opt -passes=smx-loop-idiom -S %s \
; RUN:   | FileCheck %s

target triple = "riscv32"

declare i32 @llvm.smax.i32(i32, i32)
declare i32 @llvm.smin.i32(i32, i32)
declare i8 @llvm.ctlz.i8(i8, i1 immarg)

define i32 @scalar_contx(ptr %in, i32 %n) #0 {
; CHECK-LABEL: define i32 @scalar_contx(
; CHECK: call i32 @llvm.riscv.smx.contx(
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

  %ctx.max.sh = shl i32 %ctx, 24
  %ctx.max = ashr exact i32 %ctx.max.sh, 24
  %ctx.min.sh = shl i32 %ctx, 16
  %ctx.min = ashr exact i32 %ctx.min.sh, 24

  %p0.l0.sh = shl i32 %p0, 24
  %p0.l0 = ashr exact i32 %p0.l0.sh, 24
  %p0.l1.sh = shl i32 %p0, 16
  %p0.l1 = ashr exact i32 %p0.l1.sh, 24
  %p0.l2.sh = shl i32 %p0, 8
  %p0.l2 = ashr exact i32 %p0.l2.sh, 24
  %p0.l3 = ashr exact i32 %p0, 24
  %p1.l0.sh = shl i32 %p1, 24
  %p1.l0 = ashr exact i32 %p1.l0.sh, 24
  %p1.l1.sh = shl i32 %p1, 16
  %p1.l1 = ashr exact i32 %p1.l1.sh, 24
  %p1.l2.sh = shl i32 %p1, 8
  %p1.l2 = ashr exact i32 %p1.l2.sh, 24
  %p1.l3 = ashr exact i32 %p1, 24

  %max0 = call i32 @llvm.smax.i32(i32 %ctx.max, i32 %p0.l0)
  %max1 = call i32 @llvm.smax.i32(i32 %max0, i32 %p0.l1)
  %max2 = call i32 @llvm.smax.i32(i32 %max1, i32 %p0.l2)
  %max3 = call i32 @llvm.smax.i32(i32 %max2, i32 %p0.l3)
  %max4 = call i32 @llvm.smax.i32(i32 %max3, i32 %p1.l0)
  %max5 = call i32 @llvm.smax.i32(i32 %max4, i32 %p1.l1)
  %max6 = call i32 @llvm.smax.i32(i32 %max5, i32 %p1.l2)
  %max7 = call i32 @llvm.smax.i32(i32 %max6, i32 %p1.l3)

  %min0 = call i32 @llvm.smin.i32(i32 %ctx.min, i32 %p0.l0)
  %min1 = call i32 @llvm.smin.i32(i32 %min0, i32 %p0.l1)
  %min2 = call i32 @llvm.smin.i32(i32 %min1, i32 %p0.l2)
  %min3 = call i32 @llvm.smin.i32(i32 %min2, i32 %p0.l3)
  %min4 = call i32 @llvm.smin.i32(i32 %min3, i32 %p1.l0)
  %min5 = call i32 @llvm.smin.i32(i32 %min4, i32 %p1.l1)
  %min6 = call i32 @llvm.smin.i32(i32 %min5, i32 %p1.l2)
  %min7 = call i32 @llvm.smin.i32(i32 %min6, i32 %p1.l3)

  %delta = sub nsw i32 %max7, %min7
  %delta8 = trunc i32 %delta to i8
  %lzc8 = call i8 @llvm.ctlz.i8(i8 %delta8, i1 false)
  %lzc = zext i8 %lzc8 to i32
  %shift.raw = add nsw i32 %lzc, -1
  %shift.hi = call i32 @llvm.smin.i32(i32 %shift.raw, i32 7)
  %shift = call i32 @llvm.smax.i32(i32 %shift.hi, i32 -8)
  %shift.mask = and i32 %shift, 15
  %shift.pack = shl i32 %shift.mask, 16
  %min.mask = and i32 %min7, 255
  %min.pack = shl i32 %min.mask, 8
  %max.mask = and i32 %max7, 255
  %lo = or i32 %min.pack, %max.mask
  %next = or i32 %shift.pack, %lo

  %inc = add i32 %i, 8
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop

exit:
  %ret = phi i32 [ %next, %loop ]
  ret i32 %ret
}

attributes #0 = { "target-features"="+smx" }

