# RUN: llvm-mc %s -triple=riscv32 -mattr=+xsmx -show-encoding \
# RUN:   | FileCheck %s --check-prefixes=ASM,ENC
# RUN: llvm-mc %s -triple=riscv32 -mattr=+xsmx -filetype=obj \
# RUN:   | llvm-objdump --mattr=+xsmx -d - \
# RUN:   | FileCheck %s --check-prefix=ASM

# ASM: smx.contx a0, a1, a2
# ENC: encoding: [0x0b,0x85,0xc5,0x00]
smx.contx a0, a1, a2

# ASM: smx.acc a0, a1, a2
# ENC: encoding: [0x0b,0x95,0xc5,0x00]
smx.acc a0, a1, a2

# ASM: smx.exp a0, a1, a2
# ENC: encoding: [0x0b,0xa5,0xc5,0x00]
smx.exp a0, a1, a2

# ASM: smx.sigm a0, a1, a2
# ENC: encoding: [0x0b,0xb5,0xc5,0x00]
smx.sigm a0, a1, a2
