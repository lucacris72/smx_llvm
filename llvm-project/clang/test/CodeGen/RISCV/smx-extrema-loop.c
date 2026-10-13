// RUN: %clang -target riscv32 -march=rv32i_xsmx -O2 -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang -target riscv32 -march=rv32i_xsmx -O3 -S -emit-llvm %s -o - | FileCheck %s
// RUN: %clang -target riscv32 -march=rv32i -O2 -S -emit-llvm %s -o - | FileCheck %s --check-prefix=NO-SMX
// NO-SMX-NOT: @llvm.riscv.smx.contx

typedef signed char i8;
typedef struct { i8 min, max; } pair;

// CHECK-LABEL: define{{.*}} @max_seed(
// CHECK: call i32 @llvm.riscv.smx.contx(
i8 max_seed(const i8 *a, unsigned n, i8 seed) {
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > seed)
      seed = a[i];
  return seed;
}

// CHECK-LABEL: define{{.*}} @minmax(
// CHECK: load i32, ptr {{.*}}, align 4
// CHECK: load i32, ptr {{.*}}, align 4
// CHECK: call i32 @llvm.riscv.smx.contx(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i32
pair minmax(const i8 *a, unsigned n, i8 lo, i8 hi) {
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < lo)
      lo = a[i];
    if (a[i] > hi)
      hi = a[i];
  }
  return (pair){lo, hi};
}

// CHECK-LABEL: define{{.*}} @first(
// CHECK: call i32 @llvm.riscv.smx.contx(
pair first(const i8 *a, unsigned n) {
  if (!n)
    return (pair){127, -128};
  i8 lo = a[0], hi = a[0];
  for (unsigned i = 1; i < n; ++i) {
    if (a[i] < lo)
      lo = a[i];
    if (a[i] > hi)
      hi = a[i];
  }
  return (pair){lo, hi};
}

// CHECK-LABEL: define{{.*}} @aligned(
// CHECK-NOT: ptrtoint
// CHECK: load i32, ptr {{.*}}, align 4
// CHECK: load i32, ptr {{.*}}, align 4
// CHECK: call i32 @llvm.riscv.smx.contx(
// CHECK-NOT: ptrtoint
// CHECK: ret i8
i8 aligned(const i8 *a, unsigned n) {
  a = __builtin_assume_aligned(a, 4);
  i8 hi = -128;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > hi)
      hi = a[i];
  return hi;
}

// CHECK-LABEL: define{{.*}} @different_arrays(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i32
pair different_arrays(const i8 *a, const i8 *b, unsigned n) {
  i8 lo = 127, hi = -128;
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < lo)
      lo = a[i];
    if (b[i] > hi)
      hi = b[i];
  }
  return (pair){lo, hi};
}

// CHECK-LABEL: define{{.*}} @volatile_max(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i8
i8 volatile_max(const volatile i8 *a, unsigned n) {
  i8 hi = -128;
  for (unsigned i = 0; i < n; ++i) {
    i8 v = a[i];
    if (v > hi)
      hi = v;
  }
  return hi;
}

// CHECK-LABEL: define{{.*}} @unsigned_max(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i8
unsigned char unsigned_max(const unsigned char *a, unsigned n) {
  unsigned char hi = 0;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > hi)
      hi = a[i];
  return hi;
}

// CHECK-LABEL: define{{.*}} @atomic_max(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i8
i8 atomic_max(const _Atomic(i8) *a, unsigned n) {
  i8 hi = -128;
  for (unsigned i = 0; i < n; ++i) {
    i8 v = __c11_atomic_load(a + i, __ATOMIC_RELAXED);
    if (v > hi)
      hi = v;
  }
  return hi;
}

// CHECK-LABEL: define{{.*}} @max_with_store(
// CHECK-NOT: call i32 @llvm.riscv.smx.contx(
// CHECK: ret i8
i8 max_with_store(const i8 *a, i8 *out, unsigned n) {
  i8 hi = -128;
  for (unsigned i = 0; i < n; ++i) {
    out[i] = a[i];
    if (a[i] > hi)
      hi = a[i];
  }
  return hi;
}

extern i8 aligned_data[256] __attribute__((aligned(4)));

// CHECK-LABEL: define{{.*}} @global_max(
// CHECK-NOT: ptrtoint
// CHECK: load i32, ptr {{.*}}, align 4
// CHECK: call i32 @llvm.riscv.smx.contx(
// CHECK-NOT: ptrtoint
// CHECK: ret i8
i8 global_max(unsigned n) {
  i8 hi = -128;
  for (unsigned i = 0; i < n; ++i)
    if (aligned_data[i] > hi)
      hi = aligned_data[i];
  return hi;
}

// CHECK-LABEL: define{{.*}} @shifted_base(
// CHECK: call i32 @llvm.riscv.smx.contx(
i8 shifted_base(const i8 *a, unsigned n) {
  a += 4;
  i8 lo = 17;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] < lo)
      lo = a[i];
  return lo;
}
