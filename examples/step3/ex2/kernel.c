#include <stdint.h>

int8_t min_kernel(const int8_t *a, unsigned n) {
  int8_t min = INT8_MAX;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}
