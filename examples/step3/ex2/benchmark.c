#include <stdint.h>
#include <stdio.h>

enum { VECTOR_SIZE = 256, REPEATS = 16 };
int8_t min_kernel(const int8_t *a, unsigned n);

static int8_t data[VECTOR_SIZE + 1] __attribute__((aligned(4)));
static volatile int sink;

static void fill_data(unsigned pattern) {
  for (unsigned i = 0; i < sizeof(data); ++i) {
    if (pattern == 0)
      data[i] = (int8_t)((int)((i * 37u + 91u) & 255u) - 128);
    else if (pattern == 1)
      data[i] = -12;
    else
      data[i] = (i & 1u) ? INT8_MIN : INT8_MAX;
  }
}

static int8_t reference_min(const int8_t *a, unsigned n) {
  int8_t min = INT8_MAX;
  for (unsigned i = 0; i < n; ++i)
    if (a[i] < min)
      min = a[i];
  return min;
}

static uint32_t read_cycles(void) {
  uint32_t value;
  __asm__ volatile("csrr %0, mcycle" : "=r"(value) : : "memory");
  return value;
}

int main(void) {
  static const unsigned lengths[] = {0, 1, 7, 8, 9, 15, 16, 31, 255, 256};
  unsigned cases = 0;
  unsigned failures = 0;
  for (unsigned pattern = 0; pattern < 3; ++pattern) {
    fill_data(pattern);
    for (unsigned offset = 0; offset < 2; ++offset)
      for (unsigned j = 0; j < sizeof(lengths) / sizeof(lengths[0]); ++j) {
        unsigned n = lengths[j];
        int actual = min_kernel(data + offset, n);
        int expected = reference_min(data + offset, n);
        printf("RESULT pattern=%u offset=%u n=%u min=%d\n", pattern, offset, n, actual);
        failures += actual != expected;
        ++cases;
      }
  }

  fill_data(0);
  __asm__ volatile("csrci mcountinhibit, 1" : : : "memory");
  for (unsigned offset = 0; offset < 2; ++offset)
    for (unsigned n = 255; n <= VECTOR_SIZE; ++n) {
      sink = min_kernel(data + offset, n);
      int checksum = 0;
      uint32_t start = read_cycles();
      for (unsigned repeat = 0; repeat < REPEATS; ++repeat)
        checksum += min_kernel(data + offset, n);
      uint32_t cycles = read_cycles() - start;
      sink = checksum;
      printf("BENCH offset=%u n=%u repeats=%u cycles=%u checksum=%d\n",
             offset, n, REPEATS, cycles, checksum);
    }
  if (!failures)
    printf("PASS min-benchmark cases=%u\n", cases);
  return failures != 0;
}
