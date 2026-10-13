# RUN: llvm-mc %s -triple=riscv32 -mattr=+smx -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -triple=riscv64 -mattr=+smx -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=riscv32 -mattr=+smx < %s \
# RUN:     | llvm-objdump --mattr=+smx --no-print-imm-hex -d -r - \
# RUN:     | FileCheck --check-prefix=CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=riscv64 -mattr=+smx < %s \
# RUN:     | llvm-objdump --mattr=+smx --no-print-imm-hex -d -r - \
# RUN:     | FileCheck --check-prefix=CHECK-ASM-AND-OBJ %s

# CHECK-ASM-AND-OBJ: smx.contx a0, a1, a2
# CHECK-ASM: encoding: [0x2b,0xf5,0xc5,0xb4]
smx.contx a0, a1, a2

# CHECK-ASM-AND-OBJ: smx.acc a0, a1, a2
# CHECK-ASM: encoding: [0x2b,0xf5,0xc5,0xb6]
smx.acc a0, a1, a2

# CHECK-ASM-AND-OBJ: smx.exp a0, a1, a2
# CHECK-ASM: encoding: [0x2b,0xf5,0xc5,0xb8]
smx.exp a0, a1, a2

# CHECK-ASM-AND-OBJ: smx.sigm a0, a1, a2
# CHECK-ASM: encoding: [0x2b,0xf5,0xc5,0xba]
smx.sigm a0, a1, a2

