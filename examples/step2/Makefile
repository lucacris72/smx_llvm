STEP2_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.DEFAULT_GOAL := all
.PHONY: all check clean

all:
	$(MAKE) -C "$(STEP2_DIR)/ex1"
	$(MAKE) -C "$(STEP2_DIR)/ex2"

check:
	$(MAKE) -C "$(STEP2_DIR)/ex1" check
	$(MAKE) -C "$(STEP2_DIR)/ex2" check

clean:
	$(MAKE) -C "$(STEP2_DIR)/ex1" clean
	$(MAKE) -C "$(STEP2_DIR)/ex2" clean
