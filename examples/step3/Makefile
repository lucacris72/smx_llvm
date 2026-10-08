STEP3_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.DEFAULT_GOAL := all
.PHONY: all ex1 ex2 check check-rtl check-all sim clean

all: ex1

ex1:
	$(MAKE) -C "$(STEP3_DIR)/ex1"

ex2:
	$(MAKE) -C "$(STEP3_DIR)/ex2"

check:
	$(MAKE) -C "$(STEP3_DIR)/ex1" check

check-rtl:
	$(MAKE) -C "$(STEP3_DIR)/ex2" check

check-all: check check-rtl

sim:
	$(MAKE) -C "$(STEP3_DIR)/ex1" sim

clean:
	$(MAKE) -C "$(STEP3_DIR)/ex1" clean
	$(MAKE) -C "$(STEP3_DIR)/ex2" clean
