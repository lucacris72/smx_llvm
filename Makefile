BUILD_DIR ?= build
MAKE_CMD ?= make
HOST_CC ?= cc
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
LLVM_BUILD ?= build/llvm-smx
LLVM_TOOLS := $(abspath $(LLVM_BUILD))/bin
LLVM_LIT ?= $(LLVM_TOOLS)/llvm-lit
LLVM_CLANG ?= $(LLVM_TOOLS)/clang

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra
RISCV_CFLAGS := -O2 -march=rv32im -mabi=ilp32
LLVM_RISCV_CFLAGS := -target riscv32 -march=rv32im_smx -mabi=ilp32 -O2
SMX_RTL_DIR := hardware/smx
SMX_RTL_UNIT_DIR := $(SMX_RTL_DIR)/SMX_dev/src
SMX_RTL_CORE_DIR := $(SMX_RTL_DIR)/example_tb/core
SMX_DOCKER := $(SMX_RTL_DIR)/docker/shell.sh
LLVM_SMX_LIT_TESTS := \
	tests/lit \
	llvm-project/llvm/test/MC/RISCV/smx-valid.s \
	llvm-project/llvm/test/MC/RISCV/smx-invalid.s \
	llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll \
	llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics.c \
	llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics-error.c

.PHONY: all check check-llvm smx-clang-softmax-asm clean smx-rtl-unit smx-core-hello smx-core-softmax smx-docker-unit smx-docker-core-softmax

all: \
	$(BUILD_DIR)/smx_softmax_i8.riscv.o \
	$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o

check: $(BUILD_DIR)/smx_softmax_i8.host.o all

check-llvm:
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q $(LLVM_SMX_LIT_TESTS)

smx-clang-softmax-asm: $(BUILD_DIR)/smx_softmax_i8.llvm.s

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/smx_softmax_i8.host.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(HOST_CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_i8.riscv.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(RISCV_CC) $(CPPFLAGS) $(CFLAGS) $(RISCV_CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_i8.llvm.s: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(LLVM_CLANG) $(CPPFLAGS) $(LLVM_RISCV_CFLAGS) -DSMX_USE_LLVM_INTRINSICS=1 -S $< -o $@

$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o: examples/smx_softmax_smx_asm.S | $(BUILD_DIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

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
