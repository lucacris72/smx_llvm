STEP5_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.DEFAULT_GOAL := all
.PHONY: all check run clean
.SILENT:

all:
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex1"

check:
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex1" check
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex2" check

run:
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex2" run

clean:
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex1" clean
	$(MAKE) --no-print-directory -C "$(STEP5_DIR)/ex2" clean
