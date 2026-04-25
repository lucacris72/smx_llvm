// RUN: %clang_cc1 -triple riscv32 -verify %s -o -
// RUN: %clang_cc1 -triple riscv64 -target-feature +smx -verify=rv64 %s -o -

typedef unsigned int uint32_t;

#if __riscv_xlen == 32
uint32_t missing_feature(uint32_t ctx, uint32_t x, uint32_t y) {
  return __builtin_riscv_smx_contx(ctx, x, y); // expected-error {{builtin requires at least one of the following extensions: 'Smx'}}
}
#endif

#if __riscv_xlen == 64
uint32_t wrong_xlen(uint32_t ctx, uint32_t x, uint32_t y) {
  return __builtin_riscv_smx_contx(ctx, x, y); // rv64-error {{builtin requires: 'RV32'}}
}
#endif
