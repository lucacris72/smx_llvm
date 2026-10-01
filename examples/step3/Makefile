STEP3_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(STEP3_DIR)/../..)
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
BUILD_DIR ?= $(STEP3_DIR)/build

CLANG := $(LLVM_TOOLS)/clang
OPT := $(LLVM_TOOLS)/opt
LLVM_OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
FILECHECK := $(LLVM_TOOLS)/FileCheck
LLVM_LIT := $(LLVM_TOOLS)/llvm-lit

SOURCE := $(STEP3_DIR)/min_loops.c
CHECKS := $(STEP3_DIR)/checks.txt
BEFORE_IR := $(BUILD_DIR)/min_loops.before.ll
PASS_IR := $(BUILD_DIR)/min_loops.pass.ll
AUTO_IR := $(BUILD_DIR)/min_loops.auto.ll
AUTO_ASM := $(BUILD_DIR)/min_loops.auto.s
BASE_ASM := $(BUILD_DIR)/min_loops.no-xsmx.s
AUTO_OBJECT := $(BUILD_DIR)/min_loops.auto.o
AUTO_DUMP := $(BUILD_DIR)/min_loops.auto.dump
REMARKS := $(BUILD_DIR)/min_loops.remarks.txt

TARGET_FLAGS := --target=riscv32-unknown-elf -mabi=ilp32
XSMX_FLAGS := $(TARGET_FLAGS) -march=rv32i_xsmx
BASE_FLAGS := $(TARGET_FLAGS) -march=rv32i

.DEFAULT_GOAL := all
.PHONY: all check sim clean

all: $(BEFORE_IR) $(PASS_IR) $(AUTO_IR) $(AUTO_ASM) $(BASE_ASM) $(AUTO_DUMP) $(REMARKS)

$(BUILD_DIR):
	mkdir -p "$@"

$(BEFORE_IR): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(XSMX_FLAGS) -O1 -emit-llvm -S "$<" -o "$@"

$(PASS_IR): $(BEFORE_IR)
	$(OPT) -passes='loop-simplify,lcssa,smx-min-idiom' -verify-each -S "$<" -o "$@"

$(AUTO_IR): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(XSMX_FLAGS) -O2 -emit-llvm -S "$<" -o "$@"

$(AUTO_ASM): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(XSMX_FLAGS) -O2 -S "$<" -o "$@"

$(BASE_ASM): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(BASE_FLAGS) -O2 -S "$<" -o "$@"

$(AUTO_OBJECT): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(XSMX_FLAGS) -O2 -g -c "$<" -o "$@"

$(AUTO_DUMP): $(AUTO_OBJECT)
	$(LLVM_OBJDUMP) --source --line-numbers -d "$<" > "$@"

$(REMARKS): $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(XSMX_FLAGS) -O2 -Rpass=smx-min-idiom -S "$<" -o /dev/null 2> "$@"

check: all
	$(FILECHECK) --check-prefix=BEFORE $(CHECKS) < "$(BEFORE_IR)"
	$(FILECHECK) --check-prefix=PASS $(CHECKS) < "$(PASS_IR)"
	$(FILECHECK) --check-prefix=AUTO-IR $(CHECKS) < "$(AUTO_IR)"
	$(FILECHECK) --check-prefix=AUTO-ASM $(CHECKS) < "$(AUTO_ASM)"
	$(FILECHECK) --check-prefix=NO-XSMX $(CHECKS) < "$(BASE_ASM)"
	$(FILECHECK) --check-prefix=REMARK $(CHECKS) < "$(REMARKS)"
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q \
		"$(REPO_ROOT)/llvm-project/llvm/test/Transforms/SMXMinLoop/min-i8.ll" \
		"$(REPO_ROOT)/llvm-project/clang/test/CodeGen/RISCV/smx-min-loop.c"

sim:
	$(MAKE) -C "$(REPO_ROOT)" smx-min-core-run

clean:
	rm -f "$(BEFORE_IR)" "$(PASS_IR)" "$(AUTO_IR)" "$(AUTO_ASM)" \
		"$(BASE_ASM)" "$(AUTO_OBJECT)" "$(AUTO_DUMP)" "$(REMARKS)"
