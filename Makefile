BUILD_DIR ?= build
MAKE_CMD ?= make
HOST_CC ?= cc
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)
LLVM_BUILD ?= build/llvm-course
LLVM_TOOLS := $(abspath $(LLVM_BUILD))/bin
LLVM_CLANG := $(LLVM_TOOLS)/clang
LLVM_LIT := $(LLVM_TOOLS)/llvm-lit
SMX_CORE_DIR := hardware/smx/example_tb/core

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra
RISCV_CFLAGS := -O2 -march=rv32im -mabi=ilp32
SMX_RTL_DIR := hardware/smx
SMX_RTL_UNIT_DIR := $(SMX_RTL_DIR)/SMX_dev/src
SMX_RTL_CORE_DIR := $(SMX_RTL_DIR)/example_tb/core
SMX_DOCKER := $(SMX_RTL_DIR)/docker/shell.sh

.PHONY: all check check-llvm smx-clang-softmax-asm smx-min-core-elf smx-min-core-run clean smx-rtl-unit smx-core-hello smx-core-softmax smx-docker-unit smx-docker-core-softmax

all: \
	$(BUILD_DIR)/smx_softmax_i8.riscv.o \
	$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o

check: $(BUILD_DIR)/smx_softmax_i8.host.o all

check-llvm:
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q \
		llvm-project/llvm/test/MC/RISCV/smx-valid.s \
		llvm-project/llvm/test/MC/RISCV/smx-invalid.s \
		llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll \
		llvm-project/llvm/test/Transforms/SMXMinLoop \
		llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics.c \
		llvm-project/clang/test/CodeGen/RISCV/smx-min-loop.c \
		llvm-project/clang/test/CodeGen/RISCV/smx-extrema-loop.c

smx-clang-softmax-asm: $(BUILD_DIR)/smx_softmax_i8.llvm.s

$(BUILD_DIR)/smx_softmax_i8.llvm.s: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(LLVM_CLANG) --target=riscv32-unknown-elf -march=rv32imc_zicsr_xsmx \
		--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2 \
		-DSMX_USE_LLVM_INTRINSICS=1 -Iinclude -S $< -o $@

$(BUILD_DIR)/smx_min_i8.llvm.o: examples/smx_min_i8.c include/smx_min.h | $(BUILD_DIR)
	$(LLVM_CLANG) --target=riscv32-unknown-elf -march=rv32imc_zicsr_xsmx \
		--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2 -g -Iinclude -c $< -o $@

$(BUILD_DIR)/smx_min_core.llvm.o: examples/smx_min_core.c include/smx_min.h | $(BUILD_DIR)
	$(LLVM_CLANG) --target=riscv32-unknown-elf -march=rv32imc_zicsr_xsmx \
		--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2 -g -Iinclude -c $< -o $@

$(BUILD_DIR)/smx_min_core.elf: $(BUILD_DIR)/smx_min_i8.llvm.o $(BUILD_DIR)/smx_min_core.llvm.o
	$(RISCV_CC) -march=rv32imc_zicsr -mabi=ilp32 -g -nostdlib -static \
		-T $(SMX_CORE_DIR)/custom/link.ld \
		$(SMX_CORE_DIR)/custom/crt0.S $^ \
		$(SMX_CORE_DIR)/mem_stall/mem_stall.c \
		$(SMX_CORE_DIR)/custom/syscalls.c \
		$(SMX_CORE_DIR)/custom/vectors.S \
		-I $(RISCV_SYSROOT)/include \
		-I $(SMX_CORE_DIR)/mem_stall \
		-Wl,--start-group -lm -lc -lgcc -Wl,--end-group -o $@

$(BUILD_DIR)/smx_min_core.hex: $(BUILD_DIR)/smx_min_core.elf
	/opt/riscv/bin/riscv32-unknown-elf-objcopy -O verilog $< $@

smx-min-core-elf: $(BUILD_DIR)/smx_min_core.elf

smx-min-core-run: $(BUILD_DIR)/smx_min_core.hex
	$(MAKE) -C $(SMX_CORE_DIR) FIRMWARE=$(abspath $<) verilator-run

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/smx_softmax_i8.host.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(HOST_CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_i8.riscv.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(RISCV_CC) $(CPPFLAGS) $(CFLAGS) $(RISCV_CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o: examples/smx_softmax_smx_asm.S | $(BUILD_DIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -c $< -o $@

clean:
	rm -f $(BUILD_DIR)/smx_*.o $(BUILD_DIR)/smx_*.s \
		$(BUILD_DIR)/smx_*.elf $(BUILD_DIR)/smx_*.hex

smx-rtl-unit:
	$(MAKE_CMD) -C $(SMX_RTL_UNIT_DIR) TOP=tb_smx_fu_lut sim

smx-core-hello:
	$(MAKE_CMD) -C $(SMX_RTL_CORE_DIR) hello-world-run

smx-core-softmax:
	$(MAKE_CMD) -C $(SMX_RTL_CORE_DIR) softmax-comparison-run

smx-docker-unit:
	$(SMX_DOCKER) bash -lc 'cd SMX_dev/src && make TOP=tb_smx_fu_lut sim'

smx-docker-core-softmax:
	$(SMX_DOCKER) bash -lc 'cd example_tb/core && make softmax-comparison-run'
