EX1_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(EX1_DIR)/../../..)
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
BUILD_DIR ?= $(EX1_DIR)/build
CLANG := $(LLVM_TOOLS)/clang
OPT := $(LLVM_TOOLS)/opt
FILECHECK := $(LLVM_TOOLS)/FileCheck
LLVM_LIT := $(LLVM_TOOLS)/llvm-lit
FLAGS := --target=riscv32-unknown-elf -mabi=ilp32
SOURCE := $(EX1_DIR)/extrema.c
HEADER := $(EX1_DIR)/extrema.h
OUTPUTS := $(addprefix $(BUILD_DIR)/extrema.,before.ll pass.ll auto.ll auto.s scalar.s remarks.txt)

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:
export PYTHONDONTWRITEBYTECODE := 1
.PHONY: all check clean
.SILENT:

all: $(OUTPUTS)

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/extrema.before.ll: $(SOURCE) $(HEADER) $(CLANG) | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=rv32i_xsmx -O1 -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/extrema.pass.ll: $(BUILD_DIR)/extrema.before.ll $(OPT)
	$(OPT) -passes='loop-simplify,lcssa,smx-min-idiom' -verify-each -S "$<" -o "$@"

$(BUILD_DIR)/extrema.auto.ll: $(SOURCE) $(HEADER) $(CLANG) | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=rv32i_xsmx -O2 -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/extrema.auto.s: $(SOURCE) $(HEADER) $(CLANG) | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=rv32i_xsmx -O2 -S "$<" -o "$@"

$(BUILD_DIR)/extrema.scalar.s: $(SOURCE) $(HEADER) $(CLANG) | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=rv32i -O2 -S "$<" -o "$@"

$(BUILD_DIR)/extrema.remarks.txt: $(SOURCE) $(HEADER) $(CLANG) | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=rv32i_xsmx -O2 -Rpass=smx-min-idiom -S "$<" -o /dev/null 2> "$@"

check: all
	$(FILECHECK) $(EX1_DIR)/checks.txt --check-prefix=PASS < $(BUILD_DIR)/extrema.pass.ll
	$(FILECHECK) $(EX1_DIR)/checks.txt --check-prefix=AUTO < $(BUILD_DIR)/extrema.auto.ll
	$(FILECHECK) $(EX1_DIR)/checks.txt --check-prefix=ASM < $(BUILD_DIR)/extrema.auto.s
	$(FILECHECK) $(EX1_DIR)/checks.txt --check-prefix=SCALAR < $(BUILD_DIR)/extrema.scalar.s
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q \
		$(REPO_ROOT)/llvm-project/llvm/test/Transforms/SMXMinLoop \
		$(REPO_ROOT)/llvm-project/clang/test/CodeGen/RISCV/smx-extrema-loop.c
	printf 'PASS step5/ex1: extrema lowering, aligned loads and scalar baseline\n'

clean:
	rm -f $(OUTPUTS)
