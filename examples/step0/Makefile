STEP0_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
CORE_DIR := $(STEP0_DIR)/../../hardware/smx/example_tb/core
SRC_DIR := $(STEP0_DIR)/src
BUILD_DIR ?= $(STEP0_DIR)/build

RISCV ?= /opt/riscv
RISCV_PREFIX := $(RISCV)/bin/riscv32-unknown-elf-
RISCV_CC := $(RISCV_PREFIX)gcc
RISCV_OBJCOPY := $(RISCV_PREFIX)objcopy
RISCV_OBJDUMP := $(RISCV_PREFIX)objdump
RISCV_FLAGS ?= -march=rv32imc_zicsr -mabi=ilp32
LLVM_TOOLS ?= $(abspath $(STEP0_DIR)/../../build/llvm-course/bin)
CLANG := $(LLVM_TOOLS)/clang

ELF := $(BUILD_DIR)/softmax_test.elf
HEX := $(BUILD_DIR)/softmax_test.hex
DUMP := $(BUILD_DIR)/softmax_test.dump
SOURCE_DUMP := $(BUILD_DIR)/softmax_test.source.dump
IR := $(BUILD_DIR)/softmax_test.ll
HEADERS := $(STEP0_DIR)/../../include/smx_intrin.h $(STEP0_DIR)/../../include/smx_encoding.h
SOURCES := $(SRC_DIR)/softmax_test.c $(SRC_DIR)/crt0.S \
           $(SRC_DIR)/syscalls.c $(SRC_DIR)/vectors.S \
           $(CORE_DIR)/mem_stall/mem_stall.c

.DEFAULT_GOAL := all
.PHONY: all dumps ir sim softmax-test-run help clean

all: $(ELF) dumps

dumps: $(DUMP) $(SOURCE_DUMP)

ir: $(IR)

$(IR): $(SRC_DIR)/softmax_test.c $(HEADERS) $(CLANG) $(STEP0_DIR)/Makefile | $(BUILD_DIR)
	@$(CLANG) --target=riscv32-unknown-elf --sysroot=$(shell $(RISCV_CC) -print-sysroot) \
		$(RISCV_FLAGS) -O2 -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR):
	mkdir -p "$@"

$(ELF): $(SOURCES) $(HEADERS) $(SRC_DIR)/link.ld | $(BUILD_DIR)
	$(RISCV_CC) $(RISCV_FLAGS) -O2 -g -nostdlib -static \
		-T $(SRC_DIR)/link.ld -o "$@" $(SOURCES) \
		-Wl,--start-group -lm -lc -lgcc -Wl,--end-group

$(DUMP): $(ELF)
	$(RISCV_OBJDUMP) -d "$<" > "$@"

$(SOURCE_DUMP): $(ELF)
	$(RISCV_OBJDUMP) -dS "$<" > "$@"

$(HEX): $(ELF)
	$(RISCV_OBJCOPY) -O verilog "$<" "$@"

sim: all $(HEX)
	$(MAKE) -C "$(CORE_DIR)" FIRMWARE="$(HEX)" verilator-run

softmax-test-run: sim

help:
	@echo "make              Build the ELF and both disassembly dumps"
	@echo "make sim          Build and run the Verilator core simulation"
	@echo "make ir           Emit optional Clang IR with the original volatile asm"
	@echo "make clean        Remove step0-generated files"

clean:
	rm -f "$(ELF)" "$(HEX)" "$(DUMP)" "$(SOURCE_DUMP)" "$(IR)"
