//===-- RISCVInstrInfoSMX.td - SMX instructions ------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the SMX custom extension for scale-adaptive INT8 softmax.
//
//===----------------------------------------------------------------------===//

let Predicates = [HasSMX] in {
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in {
  class SMX_rr<bits<3> funct3, string opcodestr>
      : RVInstR<0b0000000, funct3, OPC_CUSTOM_0, (outs GPR:$rd),
                (ins GPR:$rs1, GPR:$rs2), opcodestr,
                "$rd, $rs1, $rs2">;

  class SMX_acc_rr<bits<3> funct3, string opcodestr>
      : RVInstR<0b0000000, funct3, OPC_CUSTOM_0, (outs GPR:$rd_wb),
                (ins GPR:$rd, GPR:$rs1, GPR:$rs2), opcodestr,
                "$rd, $rs1, $rs2"> {
    let Constraints = "$rd_wb = $rd";
  }

  def SMX_CONTX : SMX_acc_rr<0b000, "smx.contx">,
                  Sched<[WriteIALU, ReadIALU, ReadIALU]>;
  def SMX_ACC   : SMX_acc_rr<0b001, "smx.acc">,
                  Sched<[WriteIALU, ReadIALU, ReadIALU]>;
  def SMX_EXP   : SMX_rr<0b010, "smx.exp">,
                  Sched<[WriteIALU, ReadIALU, ReadIALU]>;
  def SMX_SIGM  : SMX_rr<0b011, "smx.sigm">,
                  Sched<[WriteIALU, ReadIALU, ReadIALU]>;
}

def : Pat<(int_riscv_smx_contx GPR:$rd, GPR:$rs1, GPR:$rs2),
          (SMX_CONTX GPR:$rd, GPR:$rs1, GPR:$rs2)>;
def : Pat<(int_riscv_smx_acc GPR:$rd, GPR:$rs1, GPR:$rs2),
          (SMX_ACC GPR:$rd, GPR:$rs1, GPR:$rs2)>;
def : PatGprGpr<int_riscv_smx_exp, SMX_EXP, i32, i32>;
def : PatGprGpr<int_riscv_smx_sigm, SMX_SIGM, i32, i32>;
} // Predicates = [HasSMX]
