#include "smx_intrin.h"

#if !SMX_RISCV_BUILTIN
#error "step2/ex1 requires the Xsmx Clang builtins"
#endif

uint32_t smx_round(uint32_t ctx, uint32_t sum, uint32_t word0,
                   uint32_t word1) {
  uint32_t next_ctx = smx_contx(ctx, word0, word1);
  uint32_t next_sum = smx_acc(sum, word0, next_ctx);
  uint32_t exp_word = smx_exp(word1, next_ctx);
  return smx_sigm(next_sum, exp_word);
}
