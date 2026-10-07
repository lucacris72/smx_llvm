#include <stddef.h>
#include <stdint.h>

#define SMX_USE_LLVM_INTRINSICS 1
#include "smx_intrin.h"
#if !SMX_RISCV_BUILTIN
#error "This benchmark requires Clang with SMX builtins"
#endif
#ifndef N
#define N 64u
#endif
_Static_assert(N >= 8 && N % 4 == 0, "N must contain complete packed words");

// Same input, quantization and three packed passes as the hardware benchmark.
static float in_data[N];
static int8_t in_q8[N] __attribute__((aligned(4)));
static uint32_t out_packed[N / 4];

static inline uint64_t read_mcycle64(void) {
    uint32_t lo, hi0, hi1;
    __asm__ volatile("" ::: "memory");
    do {
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi0));
        __asm__ volatile("csrr %0, mcycle" : "=r"(lo));
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi1));
    } while (hi0 != hi1);
    __asm__ volatile("" ::: "memory");
    return ((uint64_t)hi0 << 32) | lo;
}

static void quantize_vec(const float *in, int8_t *out, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        float val = in[i];
        if (val > 8.0f) val = 8.0f;
        if (val < -8.0f) val = -8.0f;
        out[i] = (int8_t)(val * 16.0f);
    }
}

void init_data(void) {
    for (size_t i = 0; i < N; ++i)
        in_data[i] = (float)((i * 37) % 64) / 4.0f - 8.0f;
    quantize_vec(in_data, in_q8, N);
}

static void softmax_smx(const int8_t *x, uint32_t *y, size_t n) {
    uint32_t ctx = SMX_CTX_INIT;
    const uint32_t *ptr = (const uint32_t *)x;
    size_t words = n / 4;
    size_t i = 0;
    for (; i < words - 1; i += 2)
        ctx = smx_contx(ctx, ptr[i], ptr[i + 1]);
    for (; i < words; ++i)
        ctx = smx_contx(ctx, ptr[i], ptr[i]);

    uint32_t sum = 0;
    i = 0;
    for (; i < words - 1; i += 2) {
        uint32_t p0 = ptr[i], p1 = ptr[i + 1];
        sum = smx_acc(sum, p0, ctx);
        sum = smx_acc(sum, p1, ctx);
    }
    for (; i < words; ++i)
        sum = smx_acc(sum, ptr[i], ctx);
    for (i = 0; i < words; ++i)
        y[i] = smx_sigm(sum, smx_exp(ptr[i], ctx));
}

static void print_char(char c) {
    *(volatile int *)0x10000000 = c;
}

static void print_str(const char *s) {
    while (*s) print_char(*s++);
}

static void print_dec(uint64_t value) {
    if (value == 0) {
        print_char('0');
        return;
    }
    char buf[24];
    size_t i = 0;
    while (value > 0) {
        buf[i++] = (value % 10) + '0';
        value /= 10;
    }
    while (i > 0) print_char(buf[--i]);
}

int main(void) {
    __asm__ volatile("csrci mcountinhibit, 5" ::: "memory");
    init_data();
    uint64_t start = read_mcycle64();
    softmax_smx(in_q8, out_packed, N);
    uint64_t end = read_mcycle64();
    print_str("SMX_TIMING n="); print_dec(N);
    print_str(" cycles="); print_dec(end - start); print_char('\n');
    for (size_t i = 0; i < N / 4; ++i) {
        print_str("SMX_OUTPUT index="); print_dec(i);
        print_str(" value="); print_dec(out_packed[i]); print_char('\n');
    }
    return 0;
}
