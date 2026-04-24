BUILD_DIR ?= build
HOST_CC ?= cc
RISCV_CC ?= /opt/riscv/bin/riscv32-unknown-elf-gcc

CPPFLAGS := -Iinclude
CFLAGS := -std=c11 -Wall -Wextra
RISCV_CFLAGS := -O2 -march=rv32im -mabi=ilp32

.PHONY: all check clean

all: \
	$(BUILD_DIR)/smx_softmax_i8.riscv.o \
	$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o

check: $(BUILD_DIR)/smx_softmax_i8.host.o all

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/smx_softmax_i8.host.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(HOST_CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_i8.riscv.o: src/smx_softmax_i8.c include/smx_intrin.h include/smx_softmax.h | $(BUILD_DIR)
	$(RISCV_CC) $(CPPFLAGS) $(CFLAGS) $(RISCV_CFLAGS) -c $< -o $@

$(BUILD_DIR)/smx_softmax_smx_asm.riscv.o: examples/smx_softmax_smx_asm.S | $(BUILD_DIR)
	$(RISCV_CC) $(RISCV_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
