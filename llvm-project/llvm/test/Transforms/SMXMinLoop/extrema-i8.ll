; RUN: opt -passes=smx-min-idiom -verify-each -verify-loop-info -verify-dom-info -verify-scev -verify-loop-lcssa -S %s | FileCheck %s
; RUN: opt -mtriple=riscv64 -passes=smx-min-idiom -S %s | FileCheck %s --check-prefix=NO-SMX
; RUN: opt -data-layout=E-p:32:32 -passes=smx-min-idiom -S %s | FileCheck %s --check-prefix=NO-SMX
; RUN: opt -data-layout=e-p:64:64 -passes=smx-min-idiom -S %s | FileCheck %s --check-prefix=NO-SMX
; NO-SMX-NOT: @llvm.riscv.smx.contx

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32"

declare i8 @llvm.smin.i8(i8, i8)
declare i8 @llvm.smax.i8(i8, i8)

; The independent seeds, including a potentially poison seed, are frozen before
; packing. Both reductions share one CONTX; scalar peeling updates its context.
define {i8, i8} @minmax_seed(ptr %a, i32 %n, i8 %lo, i8 %hi) #0 {
; CHECK-LABEL: define { i8, i8 } @minmax_seed(
; CHECK: freeze i8 %lo
; CHECK: freeze i8 %hi
; CHECK: %smx.ctx = phi i32
; CHECK: %smx.remaining = sub i32 %n, %i
; CHECK: icmp uge i32 %smx.remaining, 8
; CHECK: and i32 {{.*}}, 3
; CHECK: %smx.aligned = icmp eq i32 {{.*}}, 0
; CHECK: br i1 %smx.use.packed, label %smx.packed, label %smx.scalar
; CHECK: smx.packed:
; CHECK: load i32, ptr %p, align 4
; CHECK: load i32, ptr %smx.p1.ptr, align 4
; CHECK: %smx.next.ctx = call i32 @llvm.riscv.smx.contx
; CHECK: lshr i32 %smx.next.ctx, 8
; CHECK: trunc i32 %smx.next.ctx to i8
; CHECK-NOT: call i32 @llvm.riscv.smx.contx
; CHECK: smx.scalar:
; CHECK: %v = load i8, ptr %p, align 1
; CHECK: %min.next = call i8 @llvm.smin.i8
; CHECK: %max.next = call i8 @llvm.smax.i8
; CHECK: [[SCALARCTX:%.*]] = or i32
; CHECK: smx.latch:
; CHECK: phi i32 [ %smx.next.ctx, %smx.packed ], [ [[SCALARCTX]], %smx.scalar ]
; CHECK: %smx.min.next = phi i8
; CHECK: %smx.max.next = phi i8
; CHECK-NOT: call i32 @llvm.riscv.smx.contx
; CHECK: ret { i8, i8 }
entry:
  %empty = icmp eq i32 %n, 0
  br i1 %empty, label %empty.ret, label %pre
empty.ret:
  %e0 = insertvalue {i8, i8} poison, i8 %lo, 0
  %e1 = insertvalue {i8, i8} %e0, i8 %hi, 1
  ret {i8, i8} %e1
pre:
  br label %loop
loop:
  %i = phi i32 [0, %pre], [%inc, %loop]
  %min = phi i8 [%lo, %pre], [%min.next, %loop]
  %max = phi i8 [%hi, %pre], [%max.next, %loop]
  %p = getelementptr inbounds i8, ptr %a, i32 %i
  %v = load i8, ptr %p, align 1
  %min.next = call i8 @llvm.smin.i8(i8 %min, i8 %v)
  %max.next = call i8 @llvm.smax.i8(i8 %v, i8 %max)
  %inc = add nuw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  %rmin = phi i8 [%min.next, %loop]
  %rmax = phi i8 [%max.next, %loop]
  %r0 = insertvalue {i8, i8} poison, i8 %rmin, 0
  %r1 = insertvalue {i8, i8} %r0, i8 %rmax, 1
  ret {i8, i8} %r1
}

; An aligned base and index 0 prove all packed addresses aligned. A tail only
; decreases the remaining count, so it cannot re-enter the packed path.
define i8 @max_aligned(ptr align 4 %a, i32 %n) #0 {
; CHECK-LABEL: define i8 @max_aligned(
; CHECK-NOT: ptrtoint
; CHECK: phi i32 [ 32640, %pre ], [ %smx.ctx.next, %smx.latch ]
; CHECK-NOT: ptrtoint
; CHECK: smx.packed:
; CHECK: load i32, ptr {{.*}}, align 4
; CHECK: load i32, ptr {{.*}}, align 4
; CHECK: call i32 @llvm.riscv.smx.contx
; CHECK: trunc i32 %smx.next.ctx to i8
; CHECK-NOT: ptrtoint
; CHECK: ret i8
entry:
  %empty = icmp eq i32 %n, 0
  br i1 %empty, label %empty.ret, label %pre
empty.ret:
  ret i8 -128
pre:
  br label %loop
loop:
  %i = phi i32 [0, %pre], [%inc, %loop]
  %max = phi i8 [-128, %pre], [%next, %loop]
  %p = getelementptr inbounds i8, ptr %a, i32 %i
  %v = load i8, ptr %p, align 1
  %next = call i8 @llvm.smax.i8(i8 %max, i8 %v)
  %inc = add nuw i32 %i, 1
  %done = icmp ne i32 %n, %inc
  br i1 %done, label %loop, label %exit
exit:
  %result = phi i8 [%next, %loop]
  ret i8 %result
}

; a is aligned, but a + 1 is not: a first-element seed must still peel.
define i8 @min_first_aligned(ptr align 4 %a, i32 %n) #0 {
; CHECK-LABEL: define i8 @min_first_aligned(
; CHECK: %seed = load i8, ptr %a, align 4
; CHECK: zext i8 %seed to i32
; CHECK: %i = phi i32 [ 1, %pre ], [ %smx.i.next, %smx.latch ]
; CHECK: %smx.aligned
; CHECK: load i32, ptr {{.*}}, align 4
; CHECK: call i32 @llvm.riscv.smx.contx
; CHECK: ret i8
entry:
  %empty = icmp eq i32 %n, 0
  br i1 %empty, label %empty.ret, label %first
empty.ret:
  ret i8 127
first:
  %seed = load i8, ptr %a, align 4
  %single = icmp eq i32 %n, 1
  br i1 %single, label %single.ret, label %pre
single.ret:
  ret i8 %seed
pre:
  br label %loop
loop:
  %i = phi i32 [1, %pre], [%inc, %loop]
  %min = phi i8 [%seed, %pre], [%next, %loop]
  %p = getelementptr inbounds i8, ptr %a, i32 %i
  %v = load i8, ptr %p, align 1
  %next = call i8 @llvm.smin.i8(i8 %min, i8 %v)
  %inc = add nuw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  %result = phi i8 [%next, %loop]
  ret i8 %result
}

define i8 @disabled(ptr %a, i32 %n) #1 {
; CHECK-LABEL: define i8 @disabled(
; CHECK-NOT: call i32 @llvm.riscv.smx.contx
; CHECK: ret i8
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%inc, %loop]
  %max = phi i8 [-128, %entry], [%next, %loop]
  %p = getelementptr inbounds i8, ptr %a, i32 %i
  %v = load i8, ptr %p, align 1
  %next = call i8 @llvm.smax.i8(i8 %max, i8 %v)
  %inc = add nuw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  %result = phi i8 [%next, %loop]
  ret i8 %result
}

define i8 @atomic_max(ptr %a, i32 %n) #0 {
; CHECK-LABEL: define i8 @atomic_max(
; CHECK-NOT: call i32 @llvm.riscv.smx.contx
; CHECK: load atomic i8
; CHECK-NOT: call i32 @llvm.riscv.smx.contx
; CHECK: ret i8
entry:
  br label %loop
loop:
  %i = phi i32 [0, %entry], [%inc, %loop]
  %max = phi i8 [-128, %entry], [%next, %loop]
  %p = getelementptr inbounds i8, ptr %a, i32 %i
  %v = load atomic i8, ptr %p unordered, align 1
  %next = call i8 @llvm.smax.i8(i8 %max, i8 %v)
  %inc = add nuw i32 %i, 1
  %done = icmp eq i32 %inc, %n
  br i1 %done, label %exit, label %loop
exit:
  %result = phi i8 [%next, %loop]
  ret i8 %result
}

attributes #0 = { "target-features"="+xsmx" }
attributes #1 = { "target-features"="+xsmx,-xsmx" }
