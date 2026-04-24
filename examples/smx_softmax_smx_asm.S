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

  .equ SMX_CUSTOM0, 0x0b
  .equ SMX_FUNCT7, 0
  .equ SMX_CONTX, 0
  .equ SMX_ACC,   1
  .equ SMX_EXP,   2
  .equ SMX_SIGM,  3

smx_softmax_i8_asm:
  li      a3, 0x00007F80
  mv      t0, a0
  mv      t1, a2

.Lsmx_contx:
  lw      t2, 0(t0)
  lw      t3, 4(t0)
  .insn r SMX_CUSTOM0, SMX_CONTX, SMX_FUNCT7, a3, t2, t3
  addi    t0, t0, 8
  addi    t1, t1, -8
  bnez    t1, .Lsmx_contx

  li      a4, 0
  mv      t0, a0
  mv      t1, a2

.Lsmx_acc:
  lw      t2, 0(t0)
  .insn r SMX_CUSTOM0, SMX_ACC, SMX_FUNCT7, a4, t2, a3
  addi    t0, t0, 4
  addi    t1, t1, -4
  bnez    t1, .Lsmx_acc

  mv      t0, a0
  mv      t5, a1
  mv      t1, a2

.Lsmx_output:
  lw      t2, 0(t0)
  .insn r SMX_CUSTOM0, SMX_EXP, SMX_FUNCT7, a5, t2, a3
  .insn r SMX_CUSTOM0, SMX_SIGM, SMX_FUNCT7, t3, a4, a5
  sw      t3, 0(t5)
  addi    t0, t0, 4
  addi    t5, t5, 4
  addi    t1, t1, -4
  bnez    t1, .Lsmx_output

  ret

  .size smx_softmax_i8_asm, .-smx_softmax_i8_asm

