# Benchmark Results

FP32, SW Int8 and SMX baseline are fixed historical data.
Only SMX LLVM is measured: one call, -O3, RV32IMC_Zicsr_Xsmx, mcycle64.

| N | SMX historical | SMX LLVM | Historical / LLVM | FP32 historical / LLVM | SW historical / LLVM |
|---:|---:|---:|---:|---:|---:|
| 8 | 29 | 30 | 0.97x | 387.43x | 9.67x |
| 16 | 41 | 55 | 0.75x | 505.16x | 10.33x |
| 32 | 67 | 63 | 1.06x | 837.52x | 22.48x |
| 64 | 112 | 107 | 1.05x | 1030.62x | 26.64x |
| 128 | 491 | 253 | 1.94x | 871.03x | 22.59x |
| 256 | 1235 | 1057 | 1.17x | 416.31x | 10.98x |
| 512 | 2450 | 2323 | 1.05x | 378.77x | 9.60x |
| 1024 | 4882 | 4758 | 1.03x | 369.81x | 9.80x |

PASS: every packed output matches the host LUT model; Top-1 is correct for all sizes.
Initialization/quantization, printing and validation are outside the timer.
Counter overhead is included. FP32 historical data uses soft-float.
This is a comparison to historical results, not an isolated attribution to LLVM builtins.
