#include "smx_intrin.h"
extern int puts(const char *);

int main(void) {
  uint32_t ctx = smx_contx(SMX_CTX_INIT, 0x01020304, 0xfffefdfc);
  ctx = smx_contx(ctx, 0, 0);
  if ((ctx & 0xffff) != 0xfc04) return 1;
  uint32_t e = smx_exp(0, ctx);
  uint32_t lanes = (e&255) + ((e>>8)&255) + ((e>>16)&255) + (e>>24);
  uint32_t acc = smx_acc(17, 0, ctx);
  acc = smx_acc(acc, 0, ctx);
  if (acc != 17 + 2*lanes || smx_sigm(acc, e) == 0) return 2;
  puts("LLVM SMX ISA v2 -> canonical RTL PASS");
  return 0;
}
