# Shared ISA checks; keep the course's xsmx feature and RV32 target.
LLVM_RISCV_CFLAGS := --target=riscv32-unknown-elf -march=rv32imc_zicsr_xsmx \
	--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2

.PHONY: isa-check isa-sync
isa-check:
	python3 $(SMX_RTL_DIR)/util/gen_smx_isa.py --check --llvm-root $(CURDIR)
isa-sync:
	python3 $(SMX_RTL_DIR)/util/gen_smx_isa.py --llvm-root $(CURDIR)

check check-llvm: isa-check

.PHONY: check-encoding check-rtl-isa
check-encoding: isa-check
	python3 tests/check_smx_encoding.py --smx-root $(SMX_RTL_DIR) --llvm-tools $(LLVM_TOOLS)

$(BUILD_DIR)/smx_isa_smoke.o: tests/rtl/smx_isa_smoke.c include/smx_intrin.h include/smx_encoding.h $(LLVM_CLANG) | $(BUILD_DIR)
	$(LLVM_CLANG) $(CPPFLAGS) $(LLVM_RISCV_CFLAGS) -DSMX_USE_LLVM_INTRINSICS=1 -c $< -o $@

$(BUILD_DIR)/smx_isa_smoke.elf: $(BUILD_DIR)/smx_isa_smoke.o $(SMX_RTL_CORE_DIR)/custom/crt0.S $(SMX_RTL_CORE_DIR)/custom/syscalls.c $(SMX_RTL_CORE_DIR)/custom/vectors.S $(SMX_RTL_CORE_DIR)/mem_stall/mem_stall.c $(SMX_RTL_CORE_DIR)/custom/link.ld
	$(RISCV_CC) -march=rv32imc_zicsr -mabi=ilp32 -nostdlib -static -O2 \
	  -I$(SMX_RTL_CORE_DIR)/mem_stall -T $(SMX_RTL_CORE_DIR)/custom/link.ld \
	  $(filter %.o %.S %.c,$^) -Wl,--start-group -lm -lc -lgcc -Wl,--end-group -o $@

$(BUILD_DIR)/smx_isa_smoke.hex: $(BUILD_DIR)/smx_isa_smoke.elf
	$(patsubst %gcc,%objcopy,$(RISCV_CC)) -O verilog $< $@

check-rtl-isa: isa-check $(BUILD_DIR)/smx_isa_smoke.hex
	$(MAKE_CMD) -C $(SMX_RTL_CORE_DIR) SMX=1 PULP_XPULP=1 PULP_CLUSTER=1 FIRMWARE=$(abspath $(BUILD_DIR)/smx_isa_smoke.hex) verilator-run
