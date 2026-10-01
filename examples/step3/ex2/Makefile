EX2_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(EX2_DIR)/../../..)
BUILD_DIR ?= $(EX2_DIR)/build
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_OBJCOPY ?= /opt/riscv/bin/riscv32-unknown-elf-objcopy
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)
PYTHON ?= python3
CLANG := $(LLVM_TOOLS)/clang
OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
RUNTIME := $(REPO_ROOT)/examples/step0/src
CORE := $(REPO_ROOT)/hardware/smx/example_tb/core
FLAGS := --target=riscv32-unknown-elf --sysroot=$(RISCV_SYSROOT) -mabi=ilp32 -O2 -g
ARCH := rv32imc_zicsr
RUNTIME_SOURCES := $(RUNTIME)/crt0.S $(RUNTIME)/syscalls.c $(RUNTIME)/vectors.S \
	$(CORE)/mem_stall/mem_stall.c

.DEFAULT_GOAL := all
.SECONDARY:
.PHONY: all check run test clean

all: $(BUILD_DIR)/plain.hex $(BUILD_DIR)/xsmx.hex \
	$(BUILD_DIR)/plain.dump $(BUILD_DIR)/xsmx.dump

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/benchmark.o: $(EX2_DIR)/benchmark.c | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=$(ARCH) -c "$<" -o "$@"

$(BUILD_DIR)/kernel-plain.o: $(EX2_DIR)/kernel.c | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=$(ARCH) -c "$<" -o "$@"

$(BUILD_DIR)/kernel-xsmx.o: $(EX2_DIR)/kernel.c | $(BUILD_DIR)
	$(CLANG) $(FLAGS) -march=$(ARCH)_xsmx -c "$<" -o "$@"

$(BUILD_DIR)/%.elf: $(BUILD_DIR)/kernel-%.o $(BUILD_DIR)/benchmark.o $(RUNTIME_SOURCES) $(RUNTIME)/link.ld
	$(RISCV_CC) -march=$(ARCH) -mabi=ilp32 -O2 -g -nostdlib -static \
		-T $(RUNTIME)/link.ld $(filter-out %.ld,$^) \
		-I$(RISCV_SYSROOT)/include -I$(CORE)/mem_stall \
		-Wl,--start-group -lm -lc -lgcc -Wl,--end-group -o "$@"

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(RISCV_OBJCOPY) -O verilog "$<" "$@"

$(BUILD_DIR)/%.dump: $(BUILD_DIR)/kernel-%.o
	$(OBJDUMP) --mattr=+xsmx -d "$<" > "$@"

test:
	$(PYTHON) -m unittest discover -s "$(EX2_DIR)" -p 'test_*.py'

run: all
	$(PYTHON) "$(EX2_DIR)/run.py" --build-dir "$(BUILD_DIR)"

check: test run

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.elf $(BUILD_DIR)/*.hex $(BUILD_DIR)/*.dump
