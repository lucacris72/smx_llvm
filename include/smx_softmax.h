#ifndef SMX_SOFTMAX_H
#define SMX_SOFTMAX_H

#include <stddef.h>
#include <stdint.h>

void smx_softmax_i8(const int8_t *restrict in, int8_t *restrict out, size_t n);

#endif
