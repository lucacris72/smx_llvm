#include "smx_intrin.h"

#if defined(SMX_USE_LLVM_INTRINSICS)
#if !SMX_RISCV_BUILTIN
#error "ex2 builtin variant requires the Xsmx Clang builtins"
#endif
#elif !defined(SMX_USE_MNEMONICS)
#error "ex2 inline variant requires named volatile assembly"
#endif

uint32_t discarded(uint32_t ctx, uint32_t word0, uint32_t word1) {
  smx_contx(ctx, word0, word1);
  return ctx;
}

uint32_t repeated(uint32_t packed, uint32_t ctx) {
  uint32_t first = smx_exp(packed, ctx);
  uint32_t second = smx_exp(packed, ctx);
  return first ^ second;
}

uint32_t consumed(uint32_t ctx, uint32_t word0, uint32_t word1) {
  return smx_contx(ctx, word0, word1);
}
