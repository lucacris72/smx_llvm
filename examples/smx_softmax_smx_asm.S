/* Baseline RV32 SMX softmax kernel.
 *
 * ABI:
 *   a0 = input pointer
 *   a1 = output pointer
 *   a2 = element count, multiple of 8
 *
 * This version uses .insn so it can be assembled before LLVM/GAS learn the
 * SMX mnemonics.
 */

  .text
  .globl smx_softmax_i8_asm
  .type smx_softmax_i8_asm, @function

#include "../include/smx_encoding.h"

smx_softmax_i8_asm:
  li      a3, 0x00007F80
  mv      t0, a0
  mv      t1, a2

.Lsmx_contx:
  lw      t2, 0(t0)
  lw      t3, 4(t0)
  .insn r SMX_OPCODE, SMX_FUNCT3, SMX_FUNCT7_CONTX, a3, t2, t3
  addi    t0, t0, 8
  addi    t1, t1, -8
  bnez    t1, .Lsmx_contx

  li      a4, 0
  mv      t0, a0
  mv      t1, a2

.Lsmx_acc:
  lw      t2, 0(t0)
  .insn r SMX_OPCODE, SMX_FUNCT3, SMX_FUNCT7_ACC, a4, t2, a3
  addi    t0, t0, 4
  addi    t1, t1, -4
  bnez    t1, .Lsmx_acc

  mv      t0, a0
  mv      t5, a1
  mv      t1, a2

.Lsmx_output:
  lw      t2, 0(t0)
  .insn r SMX_OPCODE, SMX_FUNCT3, SMX_FUNCT7_EXP, a5, t2, a3
  .insn r SMX_OPCODE, SMX_FUNCT3, SMX_FUNCT7_SIGM, t3, a4, a5
  sw      t3, 0(t5)
  addi    t0, t0, 4
  addi    t5, t5, 4
  addi    t1, t1, -4
  bnez    t1, .Lsmx_output

  ret

  .size smx_softmax_i8_asm, .-smx_softmax_i8_asm

