EX2_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(EX2_DIR)/../../..)
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
BUILD_DIR ?= $(EX2_DIR)/build
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)

CLANG := $(LLVM_TOOLS)/clang
FILECHECK := $(LLVM_TOOLS)/FileCheck
SOURCE := $(EX2_DIR)/optimization.c
HEADER := $(REPO_ROOT)/include/smx_intrin.h
CHECKS := $(EX2_DIR)/checks.txt

BUILTIN_IR := $(BUILD_DIR)/builtin.ll
BUILTIN_ASM := $(BUILD_DIR)/builtin.s
VOLATILE_IR := $(BUILD_DIR)/volatile.ll
VOLATILE_ASM := $(BUILD_DIR)/volatile.s

CLANG_FLAGS := --target=riscv32-unknown-elf -march=rv32i_xsmx \
	--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2 -I$(REPO_ROOT)/include

.DEFAULT_GOAL := all
.PHONY: all check clean

all: $(BUILTIN_IR) $(BUILTIN_ASM) $(VOLATILE_IR) $(VOLATILE_ASM)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILTIN_IR): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -DSMX_USE_LLVM_INTRINSICS=1 -emit-llvm -S "$<" -o "$@"

$(BUILTIN_ASM): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -DSMX_USE_LLVM_INTRINSICS=1 -S "$<" -o "$@"

$(VOLATILE_IR): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -DSMX_USE_MNEMONICS=1 -emit-llvm -S "$<" -o "$@"

$(VOLATILE_ASM): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -DSMX_USE_MNEMONICS=1 -S "$<" -o "$@"

check: all
	$(FILECHECK) --check-prefix=BUILTIN-IR $(CHECKS) < "$(BUILTIN_IR)"
	$(FILECHECK) --check-prefix=VOLATILE-IR $(CHECKS) < "$(VOLATILE_IR)"
	$(FILECHECK) --check-prefix=BUILTIN $(CHECKS) < "$(BUILTIN_ASM)"
	$(FILECHECK) --check-prefix=VOLATILE $(CHECKS) < "$(VOLATILE_ASM)"

clean:
	rm -f "$(BUILTIN_IR)" "$(BUILTIN_ASM)" "$(VOLATILE_IR)" "$(VOLATILE_ASM)"
