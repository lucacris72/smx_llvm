STEP1_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(STEP1_DIR)/../..)
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
BUILD_DIR ?= $(STEP1_DIR)/build

CLANG := $(LLVM_TOOLS)/clang
LLVM_MC := $(LLVM_TOOLS)/llvm-mc
LLVM_OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
LLVM_READELF := $(LLVM_TOOLS)/llvm-readelf
LLVM_LIT := $(LLVM_TOOLS)/llvm-lit

SOURCE := $(STEP1_DIR)/smx_instructions.S
OBJECT := $(BUILD_DIR)/smx_instructions.o
DUMP := $(BUILD_DIR)/smx_instructions.dump
ATTRIBUTES := $(BUILD_DIR)/smx_instructions.attributes
ENCODING := $(BUILD_DIR)/smx_instructions.encoding
STEP0_ELF := $(REPO_ROOT)/examples/step0/build/softmax_test.elf
STEP0_LLVM_DUMP := $(BUILD_DIR)/softmax_test.llvm.dump

.DEFAULT_GOAL := all
.PHONY: all compare-step0 step0-elf check clean

all: $(DUMP) $(ATTRIBUTES) $(ENCODING)

$(BUILD_DIR):
	mkdir -p "$@"

$(OBJECT): $(SOURCE) $(CLANG) | $(BUILD_DIR)
	$(CLANG) --target=riscv32-unknown-elf -march=rv32i_xsmx -c "$<" -o "$@"

$(DUMP): $(OBJECT) $(LLVM_OBJDUMP)
	$(LLVM_OBJDUMP) -d "$<" > "$@"

$(ATTRIBUTES): $(OBJECT) $(LLVM_READELF)
	$(LLVM_READELF) -A "$<" > "$@"

$(ENCODING): $(SOURCE) $(LLVM_MC) | $(BUILD_DIR)
	$(LLVM_MC) -triple=riscv32 -mattr=+xsmx -show-encoding "$<" > "$@"

step0-elf:
	$(MAKE) -C "$(REPO_ROOT)/examples/step0"

$(STEP0_LLVM_DUMP): step0-elf | $(BUILD_DIR)
	$(LLVM_OBJDUMP) --mattr=+xsmx -d "$(STEP0_ELF)" > "$@"

compare-step0: $(STEP0_LLVM_DUMP)

check:
	PATH="$(LLVM_TOOLS):$$PATH" $(LLVM_LIT) -q \
		"$(REPO_ROOT)/llvm-project/llvm/test/MC/RISCV/smx-valid.s" \
		"$(REPO_ROOT)/llvm-project/llvm/test/MC/RISCV/smx-invalid.s"

clean:
	rm -f "$(OBJECT)" "$(DUMP)" "$(ATTRIBUTES)" "$(ENCODING)" "$(STEP0_LLVM_DUMP)"
