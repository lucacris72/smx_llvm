#ifndef STEP5_EXTREMA_H
#define STEP5_EXTREMA_H

#include <stdint.h>

typedef struct {
  int8_t min;
  int8_t max;
} extrema_i8;

int8_t min_i8(const int8_t *a, unsigned n);
int8_t max_i8(const int8_t *a, unsigned n);
extrema_i8 minmax_i8(const int8_t *a, unsigned n);
int8_t min_seed_i8(const int8_t *a, unsigned n, int8_t seed);
int8_t max_seed_i8(const int8_t *a, unsigned n, int8_t seed);
extrema_i8 minmax_seed_i8(const int8_t *a, unsigned n, int8_t lo, int8_t hi);
int8_t min_first_i8(const int8_t *a, unsigned n);
int8_t max_first_i8(const int8_t *a, unsigned n);
extrema_i8 minmax_first_i8(const int8_t *a, unsigned n);
extrema_i8 minmax_aligned_i8(const int8_t *a, unsigned n);

#endif
