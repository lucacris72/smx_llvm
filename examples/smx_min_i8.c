#include "smx_min.h"

int8_t smx_min_i8(const int8_t *a, size_t n) {
  int8_t min = INT8_MAX;
  for (size_t i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}
