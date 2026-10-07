STEP4_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(STEP4_DIR)/../..)
BUILD_DIR ?= $(STEP4_DIR)/build
RESULTS_DIR ?= $(STEP4_DIR)/results
N ?= 63
CONFIG := $(BUILD_DIR)/config.h
WORK_DIR := $(BUILD_DIR)/bench
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_OBJCOPY ?= /opt/riscv/bin/riscv32-unknown-elf-objcopy
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)
PYTHON ?= python3
PLOT_PYTHON ?= $(BUILD_DIR)/venv/bin/python
CLANG := $(LLVM_TOOLS)/clang
OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
SOURCE := $(REPO_ROOT)/examples/step0/src/softmax_test.c
CORE := $(REPO_ROOT)/hardware/smx/example_tb/core
ARCH := rv32imc_zicsr
FLAGS := --target=riscv32-unknown-elf --sysroot=$(RISCV_SYSROOT) -mabi=ilp32
SMX_FLAGS := $(FLAGS) -march=$(ARCH)_xsmx -I$(REPO_ROOT)/include
HEADERS := $(REPO_ROOT)/include/smx_intrin.h
DRIVER := $(STEP4_DIR)/benchmark.py
SUPPORT := $(REPO_ROOT)/examples/sim_support.py
BUILD_DEPS := $(STEP4_DIR)/Makefile $(CLANG) $(HEADERS)
QUIET := $(PYTHON) "$(DRIVER)" exec --log "$(BUILD_DIR)/build.log" --
SIZES := 8 16 32 64 128 256 512 1024
IMAGES := $(foreach n,$(SIZES),$(WORK_DIR)/$(n)/benchmark.hex)
DUMPS := $(foreach n,$(SIZES),$(WORK_DIR)/$(n)/benchmark.dump)
RUNTIME := $(addprefix $(WORK_DIR)/runtime/,crt0.o syscalls.o vectors.o mem_stall.o)

MAKEFLAGS += --no-print-directory
export PYTHONDONTWRITEBYTECODE := 1
.SILENT:
.DEFAULT_GOAL := all
.SECONDARY:
.DELETE_ON_ERROR:
.PHONY: all run sweep setup plots test check clean FORCE

all: $(BUILD_DIR)/softmax.before.ll $(BUILD_DIR)/softmax.ll \
	$(BUILD_DIR)/softmax.s $(BUILD_DIR)/softmax.inline.s

$(BUILD_DIR):
	mkdir -p "$@"

$(CONFIG): FORCE | $(BUILD_DIR)
	$(PYTHON) "$(DRIVER)" configure --n "$(N)" --output "$@"

FORCE:

$(BUILD_DIR)/softmax.before.ll: $(SOURCE) $(BUILD_DEPS) $(CONFIG)
	$(QUIET) $(CLANG) $(SMX_FLAGS) -O2 -include "$(CONFIG)" \
		-DSMX_USE_LLVM_INTRINSICS=1 -Xclang -disable-llvm-passes -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/softmax.ll: $(SOURCE) $(BUILD_DEPS) $(CONFIG)
	$(QUIET) $(CLANG) $(SMX_FLAGS) -O2 -include "$(CONFIG)" \
		-DSMX_USE_LLVM_INTRINSICS=1 -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/softmax.s: $(SOURCE) $(BUILD_DEPS) $(CONFIG)
	$(QUIET) $(CLANG) $(SMX_FLAGS) -O2 -include "$(CONFIG)" -DSMX_USE_LLVM_INTRINSICS=1 -S "$<" -o "$@"

$(BUILD_DIR)/softmax.inline.s: $(SOURCE) $(BUILD_DEPS) $(CONFIG)
	$(QUIET) $(CLANG) $(SMX_FLAGS) -O2 -include "$(CONFIG)" -DSMX_USE_MNEMONICS=1 -S "$<" -o "$@"

$(WORK_DIR)/%/benchmark.o: $(STEP4_DIR)/benchmark.c $(BUILD_DEPS) | $(BUILD_DIR)
	mkdir -p "$(@D)"
	$(QUIET) $(CLANG) $(SMX_FLAGS) -O3 -g -DN=$* -c "$<" -o "$@"

$(WORK_DIR)/runtime/%.o: $(CORE)/custom/%.S $(RISCV_CC) $(STEP4_DIR)/Makefile | $(BUILD_DIR)
	mkdir -p "$(@D)"
	$(QUIET) $(RISCV_CC) -march=$(ARCH) -mabi=ilp32 -O3 -g -c "$<" -o "$@"

$(WORK_DIR)/runtime/syscalls.o: $(CORE)/custom/syscalls.c $(RISCV_CC) $(STEP4_DIR)/Makefile | $(BUILD_DIR)
	mkdir -p "$(@D)"
	$(QUIET) $(RISCV_CC) -march=$(ARCH) -mabi=ilp32 -O3 -g -c "$<" -o "$@"

$(WORK_DIR)/runtime/mem_stall.o: $(CORE)/mem_stall/mem_stall.c $(RISCV_CC) $(STEP4_DIR)/Makefile | $(BUILD_DIR)
	mkdir -p "$(@D)"
	$(QUIET) $(RISCV_CC) -march=$(ARCH) -mabi=ilp32 -O3 -g -c "$<" -o "$@"

$(WORK_DIR)/%/benchmark.elf: $(WORK_DIR)/%/benchmark.o $(RUNTIME) $(CORE)/custom/link.ld $(STEP4_DIR)/Makefile
	$(QUIET) $(RISCV_CC) -march=$(ARCH) -mabi=ilp32 -nostdlib -static \
		-T $(CORE)/custom/link.ld $(filter %.o,$^) \
		-Wl,--start-group -lm -lc -lgcc -Wl,--end-group -o "$@"

$(WORK_DIR)/%/benchmark.hex: $(WORK_DIR)/%/benchmark.elf $(RISCV_OBJCOPY)
	$(QUIET) $(RISCV_OBJCOPY) -O verilog "$<" "$@"

$(WORK_DIR)/%/benchmark.dump: $(WORK_DIR)/%/benchmark.o $(OBJDUMP)
	$(PYTHON) "$(DRIVER)" exec --log "$(BUILD_DIR)/build.log" --output "$@" -- \
		$(OBJDUMP) --mattr=+xsmx -d "$<"

$(BUILD_DIR)/venv/.ready: $(STEP4_DIR)/requirements.txt | $(BUILD_DIR)
	if ! test -x "$(BUILD_DIR)/venv/bin/python"; then \
		$(QUIET) $(PYTHON) -m venv "$(BUILD_DIR)/venv"; fi
	$(QUIET) "$(BUILD_DIR)/venv/bin/python" -m pip install --disable-pip-version-check -r "$<"
	touch "$@"

setup: $(BUILD_DIR)/venv/.ready

run: setup all $(IMAGES) $(DUMPS)
	$(PYTHON) "$(DRIVER)" run --build-dir "$(BUILD_DIR)" --results-dir "$(RESULTS_DIR)" --clang "$(CLANG)"
	$(PLOT_PYTHON) "$(STEP4_DIR)/plots.py" --results-dir "$(RESULTS_DIR)"

sweep: run

plots: setup
	$(PLOT_PYTHON) "$(STEP4_DIR)/plots.py" --results-dir "$(RESULTS_DIR)"

test: setup
	$(QUIET) $(PLOT_PYTHON) -m unittest discover -s "$(STEP4_DIR)" -p 'test_*.py'

check: test run

clean:
	rm -rf "$(BUILD_DIR)" "$(STEP4_DIR)/__pycache__"
