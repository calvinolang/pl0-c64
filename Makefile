CC      ?= cc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O1 -g
CFLAGS  += -I src -I tests
BUILD   := build

CORE_SRC := $(wildcard src/core/*.c)
HOST_SRC := $(filter-out src/host/pl0host.c,$(wildcard src/host/*.c))
LIB_SRC  := $(CORE_SRC) $(HOST_SRC)
TEST_SRC := $(wildcard tests/test_*.c)
TEST_BIN := $(patsubst tests/%.c,$(BUILD)/%,$(TEST_SRC))
HDRS     := $(wildcard src/*/*.h tests/*.h)

.PHONY: test host examples c64 c64-tests c64-num clean

test: $(TEST_BIN) host
	@python3 tools/check_error_docs.py
	@fail=0; n=0; \
	for t in $(TEST_BIN); do \
	  n=$$((n+1)); echo "== $$t"; \
	  timeout 60 $$t; rc=$$?; \
	  if [ $$rc -eq 124 ]; then echo "TIMEOUT: $$t killed after 60 s"; fi; \
	  if [ $$rc -ne 0 ]; then fail=$$((fail+1)); fi; \
	done; \
	if [ $$fail -ne 0 ]; then echo "$$fail of $$n test file(s) FAILED"; exit 1; fi; \
	echo "$$n test file(s), all checks passed"
	@sh tests/run_examples.sh

$(BUILD)/test_%: tests/test_%.c $(LIB_SRC) $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPL0_TEST_HOOKS -o $@ $< $(LIB_SRC) -lm

host:
	@test -f src/host/pl0host.c || { echo "error: src/host/pl0host.c does not exist yet"; exit 1; }
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -DPL0_TEST_HOOKS -o $(BUILD)/pl0host src/host/pl0host.c $(LIB_SRC) -lm

examples: host
	@sh tests/run_examples.sh

c64:
	@echo "c64: the full C64 program is built from Task 14 on; see c64-tests / c64-num"

# ---- C64 (cc65): number layer on the BASIC ROM, checked in headless VICE.
# Objects go to build/c64 (cl65 -c -o), never next to the sources.
CL65        ?= cl65
C64_BUILD   := $(BUILD)/c64
C64_CFG     := src/c64/pl0-c64.cfg
C64_CFLAGS  := -t c64 -O -C $(C64_CFG) -Isrc -W +error
NUM_C64_OBJ := $(C64_BUILD)/num_c64.o $(C64_BUILD)/num_c64_s.o
C64_NUM_CYCLES ?= 100000000

$(C64_BUILD)/num_c64.o: src/c64/num_c64.c src/platform/num.h
	@mkdir -p $(C64_BUILD)
	$(CL65) $(C64_CFLAGS) -c -o $@ $<

$(C64_BUILD)/num_c64_s.o: src/c64/num_c64.s
	@mkdir -p $(C64_BUILD)
	$(CL65) -t c64 -c -o $@ $<

$(C64_BUILD)/num_check.o: tests/c64/num_check.c src/platform/num.h
	@mkdir -p $(C64_BUILD)
	$(CL65) $(C64_CFLAGS) -c -o $@ $<

$(C64_BUILD)/num_check.prg: $(C64_BUILD)/num_check.o $(NUM_C64_OBJ) $(C64_CFG)
	$(CL65) -t c64 -C $(C64_CFG) -m $(C64_BUILD)/num_check.map -Ln $(C64_BUILD)/num_check.lbl \
	  -o $@ $(C64_BUILD)/num_check.o $(NUM_C64_OBJ)

c64-tests: $(C64_BUILD)/num_check.prg

c64-num: c64-tests
	sh tests/vice_run.sh $(C64_BUILD)/num_check.prg PASS $(C64_NUM_CYCLES) FAIL

clean:
	$(RM) -r $(BUILD)
