#include "smx_softmax.h"

#include "smx_intrin.h"

#include <string.h>

static inline uint32_t smx_load_u32(const void *ptr) {
#if defined(__GNUC__) || defined(__clang__)
  ptr = __builtin_assume_aligned(ptr, 4);
#endif
  uint32_t value;
  memcpy(&value, ptr, sizeof(value));
  return value;
}

static inline void smx_store_u32(void *ptr, uint32_t value) {
#if defined(__GNUC__) || defined(__clang__)
  ptr = __builtin_assume_aligned(ptr, 4);
#endif
  memcpy(ptr, &value, sizeof(value));
}

void smx_softmax_i8(const int8_t *restrict in, int8_t *restrict out, size_t n) {
  if ((n & 7u) != 0)
    return;

  uint32_t ctx = SMX_CTX_INIT;
  for (size_t i = 0; i < n; i += 8) {
    uint32_t p0 = smx_load_u32(&in[i]);
    uint32_t p1 = smx_load_u32(&in[i + 4]);
    ctx = smx_contx(ctx, p0, p1);
  }

  uint32_t sum = 0;
  for (size_t i = 0; i < n; i += 4) {
    uint32_t packed = smx_load_u32(&in[i]);
    sum = smx_acc(sum, packed, ctx);
  }

  for (size_t i = 0; i < n; i += 4) {
    uint32_t packed = smx_load_u32(&in[i]);
    uint32_t expv = smx_exp(packed, ctx);
    uint32_t result = smx_sigm(sum, expv);
    smx_store_u32(&out[i], result);
  }
}
