#include "smx_min.h"
#include <stdint.h>
#include <stdio.h>

__attribute__((noinline, optnone))
static int8_t scalar_min_i8(const int8_t *a, unsigned n) {
  int8_t min = INT8_MAX;
  for (unsigned i = 0; i < n; ++i)
    if (min > a[i])
      min = a[i];
  return min;
}

int main(void) {
  int8_t data[48];
  unsigned failures = 0;
  for (unsigned pattern = 0; pattern < 3; ++pattern) {
    for (unsigned i = 0; i < sizeof(data); ++i) {
      if (pattern == 0)
        data[i] = (int8_t)(i * 37u + 91u);
      else if (pattern == 1)
        data[i] = -12;
      else
        data[i] = (i & 1u) ? INT8_MIN : INT8_MAX;
    }
    for (unsigned offset = 0; offset < 2; ++offset)
      for (unsigned n = 0; n <= 40; ++n) {
        int8_t expected = scalar_min_i8(data + offset, n);
        int8_t actual = smx_min_i8(data + offset, n);
        if (expected != actual) {
          printf("FAIL pattern=%u offset=%u n=%u expected=%d actual=%d\n",
                 pattern, offset, n, expected, actual);
          ++failures;
        }
      }
  }
  if (!failures)
    puts("PASS smx-min-core");
  return failures != 0;
}
