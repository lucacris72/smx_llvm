#include "../ex1/extrema.h"
#include <stdio.h>

enum { VECTOR_SIZE = 256, REPEATS = 16, PATTERNS = 8 };
static int8_t data[VECTOR_SIZE + 16] __attribute__((aligned(4)));
static volatile uint32_t sink;

static void fill(int8_t *a, unsigned n, unsigned pattern) {
  for (unsigned i = 0; i < sizeof(data); ++i)
    data[i] = (i & 1u) ? INT8_MIN : INT8_MAX;
  for (unsigned i = 0; i < n; ++i) {
    switch (pattern) {
    case 0: a[i] = (int8_t)((int)((i * 37u + 91u) & 255u) - 128); break;
    case 1: a[i] = -12; break;
    case 2: a[i] = (i & 1u) ? INT8_MIN : INT8_MAX; break;
    case 3: a[i] = i == 0 ? INT8_MIN : i == 1 ? INT8_MAX : -7; break;
    case 4: a[i] = i + 2 == n ? INT8_MIN : i + 1 == n ? INT8_MAX : 5; break;
    case 5: a[i] = (int8_t)((int)(i % 255u) - 127); break;
    case 6: a[i] = INT8_MAX; break;
    default: a[i] = INT8_MIN; break;
    }
  }
}

static extrema_i8 reference(const int8_t *a, unsigned n) {
  extrema_i8 r = {INT8_MAX, INT8_MIN};
  for (unsigned i = 0; i < n; ++i) {
    if (a[i] < r.min) r.min = a[i];
    if (a[i] > r.max) r.max = a[i];
  }
  return r;
}

static extrema_i8 expected(unsigned kind, extrema_i8 r, int8_t lo, int8_t hi) {
  if (kind >= 3 && kind <= 5) {
    if (lo < r.min) r.min = lo;
    if (hi > r.max) r.max = hi;
  }
  if (kind == 0 || kind == 3 || kind == 6) r.max = INT8_MIN;
  if (kind == 1 || kind == 4 || kind == 7) r.min = INT8_MAX;
  return r;
}

static __attribute__((noinline)) extrema_i8 invoke(unsigned kind, const int8_t *a,
                                                  unsigned n, int8_t lo, int8_t hi) {
  switch (kind) {
  case 0: return (extrema_i8){min_i8(a, n), INT8_MIN};
  case 1: return (extrema_i8){INT8_MAX, max_i8(a, n)};
  case 2: return minmax_i8(a, n);
  case 3: return (extrema_i8){min_seed_i8(a, n, lo), INT8_MIN};
  case 4: return (extrema_i8){INT8_MAX, max_seed_i8(a, n, hi)};
  case 5: return minmax_seed_i8(a, n, lo, hi);
  case 6: return (extrema_i8){min_first_i8(a, n), INT8_MIN};
  case 7: return (extrema_i8){INT8_MAX, max_first_i8(a, n)};
  case 8: return minmax_first_i8(a, n);
  default: return minmax_aligned_i8(a, n);
  }
}

static uint32_t encode(extrema_i8 r) {
  return (uint8_t)r.min | ((uint32_t)(uint8_t)r.max << 8);
}

static uint32_t read_cycles(void) {
  uint32_t value;
  __asm__ volatile("csrr %0, mcycle" : "=r"(value) : : "memory");
  return value;
}

int main(void) {
  static const unsigned lengths[] = {0, 1, 2, 3, 4, 7, 8, 9, 10, 11, 12,
                                    15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 256};
  unsigned failures = 0, total = 0;
  for (unsigned pattern = 0; pattern < PATTERNS; ++pattern) {
    uint32_t hash = 2166136261u;
    unsigned cases = 0;
    int8_t lo = pattern % 3 == 0 ? 0 : pattern % 3 == 1 ? -40 : INT8_MAX;
    int8_t hi = pattern % 3 == 0 ? 0 : pattern % 3 == 1 ? 40 : INT8_MIN;
    for (unsigned offset = 0; offset < 4; ++offset) {
      int8_t *a = data + 8 + offset;
      for (unsigned j = 0; j < sizeof(lengths) / sizeof(lengths[0]); ++j) {
        unsigned n = lengths[j];
        fill(a, n, pattern);
        const int8_t *input = n ? a : 0;
        extrema_i8 ref = reference(input, n);
        for (unsigned kind = 0; kind < (offset ? 9u : 10u); ++kind) {
          extrema_i8 actual = invoke(kind, input, n, lo, hi);
          extrema_i8 want = expected(kind, ref, lo, hi);
          if (actual.min != want.min || actual.max != want.max) {
            if (failures < 8)
              printf("FAIL pattern=%u offset=%u n=%u kind=%u got=%d,%d expected=%d,%d\n",
                     pattern, offset, n, kind, actual.min, actual.max, want.min, want.max);
            ++failures;
          }
          hash = (hash ^ (uint8_t)actual.min) * 16777619u;
          hash = (hash ^ (uint8_t)actual.max) * 16777619u;
          ++cases;
        }
      }
    }
    total += cases;
    printf("RESULT pattern=%u cases=%u hash=%08x\n", pattern, cases, (unsigned)hash);
  }

  __asm__ volatile("csrci mcountinhibit, 1" : : : "memory");
  static const unsigned kinds[] = {0, 1, 2, 8, 9};
  static const unsigned timed_lengths[] = {63, 256};
  for (unsigned k = 0; k < sizeof(kinds) / sizeof(kinds[0]); ++k)
    for (unsigned offset = 0; offset < (kinds[k] == 9 ? 1u : 4u); ++offset)
      for (unsigned j = 0; j < sizeof(timed_lengths) / sizeof(timed_lengths[0]); ++j) {
        unsigned n = timed_lengths[j];
        int8_t *a = data + 8 + offset;
        fill(a, n, 0);
        sink = encode(invoke(kinds[k], a, n, 0, 0));
        uint32_t checksum = 0;
        uint32_t start = read_cycles();
        for (unsigned repeat = 0; repeat < REPEATS; ++repeat)
          checksum += encode(invoke(kinds[k], a, n, 0, 0));
        uint32_t cycles = read_cycles() - start;
        sink = checksum;
        printf("BENCH kind=%u offset=%u n=%u repeats=%u cycles=%u checksum=%u\n",
               kinds[k], offset, n, REPEATS, (unsigned)cycles, (unsigned)checksum);
      }
  if (!failures)
    printf("PASS extrema-benchmark cases=%u\n", total);
  return failures != 0;
}
