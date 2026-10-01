EX1_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(EX1_DIR)/../../..)
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
BUILD_DIR ?= $(EX1_DIR)/build
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)

CLANG := $(LLVM_TOOLS)/clang
LLC := $(LLVM_TOOLS)/llc
LLVM_OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
LLVM_READELF := $(LLVM_TOOLS)/llvm-readelf
LLVM_DWARFDUMP := $(LLVM_TOOLS)/llvm-dwarfdump
LLVM_LIT := $(LLVM_TOOLS)/llvm-lit

SOURCE := $(EX1_DIR)/smx_explicit.c
HEADER := $(REPO_ROOT)/include/smx_intrin.h
IR := $(BUILD_DIR)/smx_explicit.ll
ASM := $(BUILD_DIR)/smx_explicit.s
OBJECT := $(BUILD_DIR)/smx_explicit.o
DUMP := $(BUILD_DIR)/smx_explicit.dump
ATTRIBUTES := $(BUILD_DIR)/smx_explicit.attributes
DWARF := $(BUILD_DIR)/smx_explicit.debug-line

CLANG_FLAGS := --target=riscv32-unknown-elf -march=rv32i_xsmx \
	--sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O1 \
	-DSMX_USE_LLVM_INTRINSICS=1 -I$(REPO_ROOT)/include

.DEFAULT_GOAL := all
.PHONY: all check clean

all: $(IR) $(ASM) $(DUMP) $(ATTRIBUTES) $(DWARF)

$(BUILD_DIR):
	mkdir -p "$@"

$(IR): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -emit-llvm -S "$<" -o "$@"

$(ASM): $(IR)
	$(LLC) -mtriple=riscv32 -mattr=+xsmx -verify-machineinstrs "$<" -o "$@"

$(OBJECT): $(SOURCE) $(HEADER) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -g -c "$<" -o "$@"

$(DUMP): $(OBJECT)
	$(LLVM_OBJDUMP) --source --line-numbers -d "$<" > "$@"

$(ATTRIBUTES): $(OBJECT)
	$(LLVM_READELF) -A "$<" > "$@"

$(DWARF): $(OBJECT)
	$(LLVM_DWARFDUMP) --debug-line "$<" > "$@"

check:
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q \
		"$(REPO_ROOT)/llvm-project/clang/test/CodeGen/RISCV/smx-intrinsics.c" \
		"$(REPO_ROOT)/llvm-project/llvm/test/CodeGen/RISCV/smx-intrinsics.ll"

clean:
	rm -f "$(IR)" "$(ASM)" "$(OBJECT)" "$(DUMP)" "$(ATTRIBUTES)" "$(DWARF)"
