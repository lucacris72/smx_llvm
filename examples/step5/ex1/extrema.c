#include "extrema.h"

int8_t min_i8(const int8_t *a, unsigned n) {
  int8_t min = INT8_MAX;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] < min)
      min = a[i];
  return min;
}

int8_t max_i8(const int8_t *a, unsigned n) {
  int8_t max = INT8_MIN;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > max)
      max = a[i];
  return max;
}

extrema_i8 minmax_i8(const int8_t *a, unsigned n) {
  int8_t min = INT8_MAX, max = INT8_MIN;
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < min)
      min = a[i];
    if (a[i] > max)
      max = a[i];
  }
  return (extrema_i8){min, max};
}

int8_t min_seed_i8(const int8_t *a, unsigned n, int8_t seed) {
  int8_t min = seed;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] < min)
      min = a[i];
  return min;
}

int8_t max_seed_i8(const int8_t *a, unsigned n, int8_t seed) {
  int8_t max = seed;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] > max)
      max = a[i];
  return max;
}

extrema_i8 minmax_seed_i8(const int8_t *a, unsigned n, int8_t lo, int8_t hi) {
  int8_t min = lo, max = hi;
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < min)
      min = a[i];
    if (a[i] > max)
      max = a[i];
  }
  return (extrema_i8){min, max};
}

int8_t min_first_i8(const int8_t *a, unsigned n) {
  if (!n)
    return INT8_MAX;
  int8_t min = a[0];
  for (unsigned i = 1; i < n; ++i)
    if (a[i] < min)
      min = a[i];
  return min;
}

int8_t max_first_i8(const int8_t *a, unsigned n) {
  if (!n)
    return INT8_MIN;
  int8_t max = a[0];
  for (unsigned i = 1; i < n; ++i)
    if (a[i] > max)
      max = a[i];
  return max;
}

extrema_i8 minmax_first_i8(const int8_t *a, unsigned n) {
  if (!n)
    return (extrema_i8){INT8_MAX, INT8_MIN};
  int8_t min = a[0], max = a[0];
  for (unsigned i = 1; i < n; ++i) {
    if (a[i] < min)
      min = a[i];
    if (a[i] > max)
      max = a[i];
  }
  return (extrema_i8){min, max};
}

extrema_i8 minmax_aligned_i8(const int8_t *a, unsigned n) {
  a = __builtin_assume_aligned(a, 4);
  int8_t min = INT8_MAX, max = INT8_MIN;
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < min)
      min = a[i];
    if (a[i] > max)
      max = a[i];
  }
  return (extrema_i8){min, max};
}
