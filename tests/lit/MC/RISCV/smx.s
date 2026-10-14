# RUN: llvm-mc %s -triple=riscv32 -mattr=+xsmx -show-encoding \
# RUN:   | FileCheck %s --check-prefixes=ASM,ENC
# RUN: llvm-mc %s -triple=riscv32 -mattr=+xsmx -filetype=obj \
# RUN:   | llvm-objdump --mattr=+xsmx -d - \
# RUN:   | FileCheck %s --check-prefix=ASM

# ASM: smx.contx a0, a1, a2
# ENC: encoding: [0x2b,0xf5,0xc5,0xb4]
smx.contx a0, a1, a2

# ASM: smx.acc a0, a1, a2
# ENC: encoding: [0x2b,0xf5,0xc5,0xb6]
smx.acc a0, a1, a2

# ASM: smx.exp a0, a1, a2
# ENC: encoding: [0x2b,0xf5,0xc5,0xb8]
smx.exp a0, a1, a2

# ASM: smx.sigm a0, a1, a2
# ENC: encoding: [0x2b,0xf5,0xc5,0xba]
smx.sigm a0, a1, a2
