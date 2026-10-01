STEP4_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(STEP4_DIR)/../..)
BUILD_DIR ?= $(STEP4_DIR)/build
LLVM_TOOLS ?= $(REPO_ROOT)/build/llvm-course/bin
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc
RISCV_OBJCOPY ?= /opt/riscv/bin/riscv32-unknown-elf-objcopy
RISCV_SYSROOT := $(shell $(RISCV_CC) -print-sysroot)
PYTHON ?= python3
CLANG := $(LLVM_TOOLS)/clang
OBJDUMP := $(LLVM_TOOLS)/llvm-objdump
STEP0 := $(REPO_ROOT)/examples/step0
RUNTIME := $(STEP0)/src
SOURCE := $(RUNTIME)/softmax_test.c
CORE := $(REPO_ROOT)/hardware/smx/example_tb/core
GNU_FLAGS := -march=rv32imc_zicsr -mabi=ilp32 -O2 -g
CLANG_FLAGS := --target=riscv32-unknown-elf --sysroot=$(RISCV_SYSROOT) \
	-march=rv32imc_zicsr_xsmx -mabi=ilp32 -O2 -g
VARIANTS := gcc-inline clang-inline clang-builtin
RUNTIME_OBJECTS := $(addprefix $(BUILD_DIR)/,crt0.o syscalls.o vectors.o mem_stall.o)
HEADERS := $(REPO_ROOT)/include/smx_intrin.h

.DEFAULT_GOAL := all
.SECONDARY:
.PHONY: all baseline analyze run test check clean

all: $(foreach v,$(VARIANTS),$(BUILD_DIR)/$(v).s $(BUILD_DIR)/$(v).dump \
	$(BUILD_DIR)/$(v).elf.dump $(BUILD_DIR)/$(v).runtime.hex) \
	$(BUILD_DIR)/clang-inline.ll $(BUILD_DIR)/clang-builtin.ll

$(BUILD_DIR):
	mkdir -p "$@"

define compile_variant
$(BUILD_DIR)/$(1).o: $(SOURCE) $(HEADERS) | $(BUILD_DIR)
	$(2) $(3) -I$(REPO_ROOT)/include -c "$$<" -o "$$@"
$(BUILD_DIR)/$(1).s: $(SOURCE) $(HEADERS) | $(BUILD_DIR)
	$(2) $(3) -I$(REPO_ROOT)/include -S "$$<" -o "$$@"
$(BUILD_DIR)/$(1).runtime.o: $(SOURCE) $(HEADERS) | $(BUILD_DIR)
	$(2) $(3) -I$(REPO_ROOT)/include -DSMX_REPORT_OUTPUT=1 -c "$$<" -o "$$@"
endef
$(eval $(call compile_variant,gcc-inline,$(RISCV_CC),$(GNU_FLAGS)))
$(eval $(call compile_variant,clang-inline,$(CLANG),$(CLANG_FLAGS)))
$(eval $(call compile_variant,clang-builtin,$(CLANG),$(CLANG_FLAGS) -DSMX_USE_LLVM_INTRINSICS=1))

$(BUILD_DIR)/clang-inline.ll: $(SOURCE) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/clang-builtin.ll: $(SOURCE) $(HEADERS) | $(BUILD_DIR)
	$(CLANG) $(CLANG_FLAGS) -I$(REPO_ROOT)/include -DSMX_USE_LLVM_INTRINSICS=1 \
		-S -emit-llvm "$<" -o "$@"

$(BUILD_DIR)/%.o: $(RUNTIME)/%.S | $(BUILD_DIR)
	$(RISCV_CC) $(GNU_FLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/syscalls.o: $(RUNTIME)/syscalls.c | $(BUILD_DIR)
	$(RISCV_CC) $(GNU_FLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/mem_stall.o: $(CORE)/mem_stall/mem_stall.c | $(BUILD_DIR)
	$(RISCV_CC) $(GNU_FLAGS) -c "$<" -o "$@"

$(BUILD_DIR)/%.elf: $(BUILD_DIR)/%.o $(RUNTIME_OBJECTS) $(RUNTIME)/link.ld
	$(RISCV_CC) $(GNU_FLAGS) -nostdlib -static -T $(RUNTIME)/link.ld \
		$(filter-out %.ld,$^) -Wl,--start-group -lm -lc -lgcc -Wl,--end-group -o "$@"

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(RISCV_OBJCOPY) -O verilog "$<" "$@"

$(BUILD_DIR)/%.dump: $(BUILD_DIR)/%.o
	$(OBJDUMP) --mattr=+xsmx -d "$<" > "$@"

$(BUILD_DIR)/%.elf.dump: $(BUILD_DIR)/%.elf
	$(OBJDUMP) --mattr=+xsmx -d "$<" > "$@"

baseline: | $(BUILD_DIR)
	$(MAKE) -C "$(STEP0)"
	$(OBJDUMP) --mattr=+xsmx -d "$(STEP0)/build/softmax_test.elf" \
		> "$(BUILD_DIR)/step0-original.dump"

analyze: all baseline
	$(PYTHON) "$(STEP4_DIR)/compare.py" --build-dir "$(BUILD_DIR)"

run: all baseline
	$(PYTHON) "$(STEP4_DIR)/compare.py" --build-dir "$(BUILD_DIR)" --simulate

test:
	$(PYTHON) -m unittest discover -s "$(STEP4_DIR)" -p 'test_*.py'

check: test run

clean:
	rm -f $(BUILD_DIR)/*.o $(BUILD_DIR)/*.s $(BUILD_DIR)/*.ll $(BUILD_DIR)/*.elf \
		$(BUILD_DIR)/*.hex $(BUILD_DIR)/*.dump $(BUILD_DIR)/*.diff \
		$(BUILD_DIR)/comparison.json $(BUILD_DIR)/comparison.md $(BUILD_DIR)/runtime-results.json
