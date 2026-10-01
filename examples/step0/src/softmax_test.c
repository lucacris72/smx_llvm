#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

#ifndef N
#define N 63u
#endif

#ifndef REPEAT
#define REPEAT 1u
#endif

// -------------------- SMX Custom Instructions --------------------
// func3:
// 000: SMX.CONTX
// 001: SMX.ACC
// 010: SMX.EXP
// 011: SMX.SIGM

#define SMX_CONTX(rd, rs1, rs2) \
    __asm__ volatile(".insn r 0x0B, 0, 0, %0, %1, %2" : "+r"(rd) : "r"(rs1), "r"(rs2))

#define SMX_ACC(rd, rs1, rs2) \
    __asm__ volatile(".insn r 0x0B, 1, 0, %0, %1, %2" : "+r"(rd) : "r"(rs1), "r"(rs2))

#define SMX_EXP(rd, rs1, rs2) \
    __asm__ volatile(".insn r 0x0B, 2, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2))

#define SMX_SIGM(rd, rs1, rs2) \
    __asm__ volatile(".insn r 0x0B, 3, 0, %0, %1, %2" : "=r"(rd) : "r"(rs1), "r"(rs2))


// -------------------- Baseline FP32 Softmax --------------------
// Clampa il delta per evitare overflow della exp in soft-float
#ifndef DELTA_MIN
#define DELTA_MIN (-8.0f)   // sufficiente per softmax numericamente robusta
#endif
#ifndef DELTA_MAX
#define DELTA_MAX ( 0.0f)
#endif

static void softmax_f32(const float *x, float *y, size_t n) {
    // 1) trova max
    float m = x[0];
    for (size_t i = 1; i < n; ++i) if (x[i] > m) m = x[i];

    // 2) exp(x - m) clampata, somma
    float sum = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        float d = x[i] - m;
        // Clamp the delta to keep the soft-float reference numerically stable.
        // This is a simple way to prevent overflow in the exponential function.
        if (d < DELTA_MIN) d = DELTA_MIN;
        if (d > DELTA_MAX) d = DELTA_MAX;

        // Uso diretto di expf da math.h
        float e = expf(d);
        y[i] = e;
        sum += e;
    }

    // 3) normalizza
    float inv = 1.0f / sum;
    for (size_t i = 0; i < n; ++i) y[i] *= inv;
}

// -------------------- Optimized SMX Softmax --------------------
// Quantizza float -> int8
static void quantize_vec(const float *in, int8_t *out, size_t n) {
    // Semplice scalatura fissa per il test: supponiamo input in [-8, 8]
    // Mappa [-8, 8] -> [-128, 127]
    for (size_t i = 0; i < n; ++i) {
        float val = in[i];
        if (val > 8.0f) val = 8.0f;
        if (val < -8.0f) val = -8.0f;
        out[i] = (int8_t)(val * 16.0f);
    }
}

// Zero-Copy approach: assumes input is 4-byte aligned
// Modified to output packed results (uint32_t) directly to avoid unpacking overhead during bench logic.
static void softmax_smx(const int8_t *x, uint32_t *y_packed, size_t n) {
    // 1) SMX.MAX Pass
    // We process 8 bytes (2 words) per instruction
    uint32_t current_max = 0x00007F80; // Init: Min=127, Max=-128

    const uint32_t *ptr = (const uint32_t *)x;
    size_t n_words = n / 4;

    // Loop 2 words (8 bytes) at a time for efficiency
    size_t i = 0;
    for (; i < n_words - 1; i += 2) {
        uint32_t rs1 = ptr[i];
        uint32_t rs2 = ptr[i+1];
        // Compare 8 bytes in one go
        SMX_CONTX(current_max, rs1, rs2);
    }
    // Handle odd word if any
    for (; i < n_words; i++) {
        uint32_t rs1 = ptr[i];
        SMX_CONTX(current_max, rs1, rs1);
    }

    // Now current_max contains {Shift, Min, Max} ready for next steps

    // 2) SMX.ACC (Sum of Exps)
    // Hardware ACC takes 1 word at a time in rs1
    uint32_t sum_acc = 0;

    // Using unrolling for consistency, though hardware limitation is 1 word/instr
    i = 0;
    for (; i < n_words - 1; i += 2) {
        uint32_t rs1 = ptr[i];
        uint32_t rs2_val = ptr[i+1];
        SMX_ACC(sum_acc, rs1, current_max);
        SMX_ACC(sum_acc, rs2_val, current_max);
    }
    for (; i < n_words; i++) {
        SMX_ACC(sum_acc, ptr[i], current_max);
    }

    // 3) SMX.EXP + SMX.SIGM (Probabilities)
    i = 0;
    for (; i < n_words; i++) {
        uint32_t chunk = ptr[i];
        uint32_t exps_packed;
        uint32_t probs_packed;

        SMX_EXP(exps_packed, chunk, current_max);
        SMX_SIGM(probs_packed, sum_acc, exps_packed);

        // Store packed result directly
        y_packed[i] = probs_packed;
    }
}


// -------------------- Inputs --------------------
static float in_data[N];
static float out_fp32[N];
// Ensure alignment for 32-bit access
static int8_t in_q8[N] __attribute__((aligned(4)));
static uint32_t out_packed[N/4]; // Output buffer for SMX
static float out_smx[N]; // Converted back from u8 for validation

void init_data() {
    for (int i = 0; i < N; ++i) {
        // Random-ish pattern
        in_data[i] = (float)((i * 37) % 64) / 4.0f - 8.0f;
    }
    quantize_vec(in_data, in_q8, N);
}

int find_argmax(const float *v, size_t n) {
    int idx = 0;
    float mx = v[0];
    for (size_t i = 1; i < n; ++i) {
        if (v[i] > mx) {
            mx = v[i];
            idx = i;
        }
    }
    return idx;
}

// -------------------- Minimal I/O --------------------
static volatile int* const stdout_reg = (int*)0x10000000;

static void print_char(char c) {
    *stdout_reg = c;
}

static void print_str(const char* s) {
    while (*s) print_char(*s++);
}

static void print_dec(uint64_t val) {
    if (val == 0) {
        print_char('0');
        return;
    }
    char buf[24];
    int i = 0;
    while (val > 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    while (i > 0) {
        print_char(buf[--i]);
    }
}

static void print_signed(int32_t val) {
    if (val < 0) {
        print_char('-');
        print_dec((uint64_t)(-val));
    } else {
        print_dec((uint64_t)val);
    }
}


int main(void) {
    print_str("Softmax Test (N=");
    print_dec((uint64_t)N);
    print_str(")\n");

    init_data();

    // --- Run FP32 ---
    softmax_f32(in_data, out_fp32, N);

    // --- Run SMX ---

    softmax_smx(in_q8, out_packed, N);

    // --- Post-Processing for Validation ---
    // Unpack results to float array
    for (size_t i = 0; i < N/4; ++i) {
        uint32_t p_val = out_packed[i];
        uint8_t *p = (uint8_t*)&p_val;
        out_smx[i*4 + 0] = (float)p[0] / 255.0f;
        out_smx[i*4 + 1] = (float)p[1] / 255.0f;
        out_smx[i*4 + 2] = (float)p[2] / 255.0f;
        out_smx[i*4 + 3] = (float)p[3] / 255.0f;
    }

    // --- Validation ---
    int idx_fp = find_argmax(out_fp32, N);
    int idx_smx = find_argmax(out_smx, N);

    int match = (idx_fp == idx_smx);

    print_str("Top-1 FP32: "); print_signed(idx_fp);
    print_str(", SMX: "); print_signed(idx_smx);
    print_str(" -> Match: "); print_str(match ? "YES" : "NO"); print_char('\n');

    return 0; // Success
}
