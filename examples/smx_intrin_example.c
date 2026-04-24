#include "smx_intrin.h"
#include "smx_softmax.h"

#include <stdint.h>

uint32_t smx_example_contx(uint32_t ctx, uint32_t x, uint32_t y) {
  return smx_contx(ctx, x, y);
}

void smx_example_softmax(const int8_t *in, int8_t *out, unsigned n) {
  smx_softmax_i8(in, out, n);
}

