#ifndef SMX_INTRIN_H
#define SMX_INTRIN_H

#include <stdint.h>

#define SMX_CTX_INIT 0x00007F80u

#include "smx_encoding.h"

#if defined(__riscv)
#define SMX_RISCV_INLINE 1
#else
#define SMX_RISCV_INLINE 0
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#if defined(SMX_USE_LLVM_INTRINSICS) &&                                    \
    __has_builtin(__builtin_riscv_smx_contx)
#define SMX_RISCV_BUILTIN 1
#else
#define SMX_RISCV_BUILTIN 0
#endif

static inline uint32_t smx_contx(uint32_t ctx, uint32_t p0, uint32_t p1) {
#if SMX_RISCV_BUILTIN
  return __builtin_riscv_smx_contx(ctx, p0, p1);
#elif SMX_RISCV_INLINE
#if defined(SMX_USE_MNEMONICS)
  __asm__ volatile("smx.contx %0, %1, %2"
                   : "+r"(ctx)
                   : "r"(p0), "r"(p1));
#else
  __asm__ volatile(".insn r %3, %4, %5, %0, %1, %2"
                   : "+r"(ctx)
                   : "r"(p0), "r"(p1), "i"(SMX_OPCODE),
                     "i"(SMX_FUNCT3), "i"(SMX_FUNCT7_CONTX));
#endif
  return ctx;
#else
  (void)p0;
  (void)p1;
  return ctx;
#endif
}

static inline uint32_t smx_acc(uint32_t sum, uint32_t packed, uint32_t ctx) {
#if SMX_RISCV_BUILTIN
  return __builtin_riscv_smx_acc(sum, packed, ctx);
#elif SMX_RISCV_INLINE
#if defined(SMX_USE_MNEMONICS)
  __asm__ volatile("smx.acc %0, %1, %2"
                   : "+r"(sum)
                   : "r"(packed), "r"(ctx));
#else
  __asm__ volatile(".insn r %3, %4, %5, %0, %1, %2"
                   : "+r"(sum)
                   : "r"(packed), "r"(ctx), "i"(SMX_OPCODE),
                     "i"(SMX_FUNCT3), "i"(SMX_FUNCT7_ACC));
#endif
  return sum;
#else
  (void)packed;
  (void)ctx;
  return sum;
#endif
}

static inline uint32_t smx_exp(uint32_t packed, uint32_t ctx) {
#if SMX_RISCV_BUILTIN
  return __builtin_riscv_smx_exp(packed, ctx);
#elif SMX_RISCV_INLINE
  uint32_t expv;
#if defined(SMX_USE_MNEMONICS)
  __asm__ volatile("smx.exp %0, %1, %2"
                   : "=r"(expv)
                   : "r"(packed), "r"(ctx));
#else
  __asm__ volatile(".insn r %3, %4, %5, %0, %1, %2"
                   : "=r"(expv)
                   : "r"(packed), "r"(ctx), "i"(SMX_OPCODE),
                     "i"(SMX_FUNCT3), "i"(SMX_FUNCT7_EXP));
#endif
  return expv;
#else
  (void)ctx;
  return packed;
#endif
}

static inline uint32_t smx_sigm(uint32_t sum, uint32_t expv) {
#if SMX_RISCV_BUILTIN
  return __builtin_riscv_smx_sigm(sum, expv);
#elif SMX_RISCV_INLINE
  uint32_t out;
#if defined(SMX_USE_MNEMONICS)
  __asm__ volatile("smx.sigm %0, %1, %2"
                   : "=r"(out)
                   : "r"(sum), "r"(expv));
#else
  __asm__ volatile(".insn r %3, %4, %5, %0, %1, %2"
                   : "=r"(out)
                   : "r"(sum), "r"(expv), "i"(SMX_OPCODE),
                     "i"(SMX_FUNCT3), "i"(SMX_FUNCT7_SIGM));
#endif
  return out;
#else
  (void)sum;
  return expv;
#endif
}

#endif
