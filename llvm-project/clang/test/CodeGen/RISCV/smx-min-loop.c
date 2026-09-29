// RUN: %clang -target riscv32 -march=rv32i_xsmx -mabi=ilp32 -O2 \
// RUN:   -S -emit-llvm %s -o - | FileCheck %s --check-prefix=IR
// RUN: %clang -target riscv32 -march=rv32i_xsmx -mabi=ilp32 -O2 \
// RUN:   -S %s -o - | FileCheck %s --check-prefix=ASM
// RUN: %clang -target riscv32 -march=rv32i -mabi=ilp32 -O2 \
// RUN:   -S %s -o - | FileCheck %s --check-prefix=NO-SMX

typedef signed char i8;

// IR-LABEL: define{{.*}} @min_i8(
// IR: call i32 @llvm.riscv.smx.contx(
// ASM-LABEL: min_i8:
// ASM: smx.contx
// NO-SMX-NOT: smx.contx
i8 min_i8(const i8 *a, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

// IR-LABEL: define{{.*}} @min_volatile(
// IR-NOT: call i32 @llvm.riscv.smx.contx(
// ASM-LABEL: min_volatile:
// ASM-NOT: smx.contx
i8 min_volatile(const volatile i8 *a, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

// IR-LABEL: define{{.*}} @min_u8(
// IR-NOT: call i32 @llvm.riscv.smx.contx(
// IR: ret i8
// ASM-LABEL: min_u8:
// ASM-NOT: smx.contx
// ASM: ret
unsigned char min_u8(const unsigned char *a, unsigned n) {
  unsigned char min = 255;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

// IR-LABEL: define{{.*}} @min_with_store(
// IR-NOT: call i32 @llvm.riscv.smx.contx(
// IR: ret i8
// ASM-LABEL: min_with_store:
// ASM-NOT: smx.contx
// ASM: ret
i8 min_with_store(const i8 *a, i8 *out, unsigned n) {
  i8 min = 127;
  for (unsigned i = 0; i < n; ++i) {
    out[i] = a[i];
    if (min > a[i])
      min = a[i];
  }
  return min;
}
