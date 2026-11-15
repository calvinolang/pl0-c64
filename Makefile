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

.PHONY: test host examples c64 c64-tests c64-num c64-gfx c64-io clean

test: $(TEST_BIN) host
	@python3 tools/check_error_docs.py
	@python3 tools/test_vice_screen.py
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
# C is compiled in two steps (cc65 -> build/c64/*.s, ca65 -> *.o), never with
# `cl65 -c` in place: cl65 writes its intermediate .s next to the source,
# which would overwrite src/c64/num_c64.s and litter the tree.
CC65        ?= cc65
CA65        ?= ca65
CL65        ?= cl65
C64_BUILD   := $(BUILD)/c64
C64_CFG     := src/c64/pl0-c64.cfg
C64_CCFLAGS := -t c64 -O -Isrc -W +error
NUM_C64_OBJ := $(C64_BUILD)/num_c64.o $(C64_BUILD)/num_c64_s.o
C64_NUM_CYCLES ?= 100000000
C64_GFX_CYCLES ?= 100000000
C64_IO_CYCLES  ?= 200000000

# Text is ASCII on the C64 too (src/core/ascii.h): every C source is
# compiled with cc65's identity charmap. cc65 has no forced-include option,
# so make writes a one-line wrapper build/c64/X.ascii.c,
#   #include <ascii_charmap.h>
#   #include "<path of the source>"
# and compiles that (-I. so the path resolves; the source's own quoted
# includes still resolve next to it). Assembly sources include
# src/c64/ascii.inc instead. cc65 stdio (printf) parses its format letters
# in PETSCII, so C64 programs print through io_put_str, never printf.
# $(call c64_cc,source,object name)
c64_cc = mkdir -p $(C64_BUILD) && \
	printf '\043include <ascii_charmap.h>\n\043include "%s"\n' $(1) > $(C64_BUILD)/$(2).ascii.c && \
	$(CC65) $(C64_CCFLAGS) -I. -o $(C64_BUILD)/$(2).c.s $(C64_BUILD)/$(2).ascii.c && \
	$(CA65) -t c64 -o $(C64_BUILD)/$(2).o $(C64_BUILD)/$(2).c.s

# Pattern rules: src/c64/X.c -> X.o, src/c64/X.s -> X_s.o,
# src/core/X.c -> X.o, tests/c64/X.c -> X.o
$(C64_BUILD)/%.o: src/c64/%.c $(HDRS)
	$(call c64_cc,$<,$*)

$(C64_BUILD)/%_s.o: src/c64/%.s src/c64/ascii.inc
	@mkdir -p $(C64_BUILD)
	$(CA65) -t c64 -I src/c64 -o $@ $<

$(C64_BUILD)/%.o: src/core/%.c $(HDRS)
	$(call c64_cc,$<,$*)

$(C64_BUILD)/%.o: tests/c64/%.c tests/c64/*.h $(HDRS)
	$(call c64_cc,$<,$*)

# link a check program: $(call c64_link,name,objects)
c64_link = $(CL65) -t c64 -C $(C64_CFG) -m $(C64_BUILD)/$(1).map -Ln $(C64_BUILD)/$(1).lbl -o $(C64_BUILD)/$(1).prg $(2)

GFX_C64_OBJ := $(C64_BUILD)/gfx_c64.o $(C64_BUILD)/gfx_c64_s.o $(C64_BUILD)/clip.o
IO_C64_OBJ  := $(C64_BUILD)/io_c64.o $(C64_BUILD)/io_c64_s.o

# out.c prints through io_c64, whose io_read_num needs the number layer
OUT_C64_OBJ := $(C64_BUILD)/out.o $(IO_C64_OBJ) $(NUM_C64_OBJ)

$(C64_BUILD)/num_check.prg: $(C64_BUILD)/num_check.o $(OUT_C64_OBJ) $(C64_CFG)
	$(call c64_link,num_check,$(C64_BUILD)/num_check.o $(OUT_C64_OBJ))

$(C64_BUILD)/gfx_check.prg: $(C64_BUILD)/gfx_check.o $(GFX_C64_OBJ) $(OUT_C64_OBJ) $(C64_CFG)
	$(call c64_link,gfx_check,$(C64_BUILD)/gfx_check.o $(GFX_C64_OBJ) $(OUT_C64_OBJ))

$(C64_BUILD)/io_check.prg: $(C64_BUILD)/io_check.o $(IO_C64_OBJ) $(NUM_C64_OBJ) $(C64_CFG)
	$(call c64_link,io_check,$(C64_BUILD)/io_check.o $(IO_C64_OBJ) $(NUM_C64_OBJ))

# host reference bitmap for c64-gfx (same lines, host renderer + clipper)
$(BUILD)/gfx_ref: tests/c64/gfx_ref.c tests/c64/gfx_lines.h src/core/clip.c src/host/gfx_host.c $(HDRS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/c64/gfx_ref.c src/core/clip.c src/host/gfx_host.c

c64-tests: $(C64_BUILD)/num_check.prg $(C64_BUILD)/gfx_check.prg $(C64_BUILD)/io_check.prg

c64-num: $(C64_BUILD)/num_check.prg
	sh tests/vice_run.sh $(C64_BUILD)/num_check.prg PASS $(C64_NUM_CYCLES) FAIL

c64-gfx: $(C64_BUILD)/gfx_check.prg $(BUILD)/gfx_ref
	$(BUILD)/gfx_ref $(C64_BUILD)/gfx_ref
	sh tests/vice_run.sh --snap \
	  --check "python3 tools/check_bitmap.py --ref $(C64_BUILD)/gfx_ref --lbl $(C64_BUILD)/gfx_check.lbl" \
	  $(C64_BUILD)/gfx_check.prg PASS $(C64_GFX_CYCLES) FAIL

# keys for io_check.c, typed with the monitor `keybuf` command when the
# program shows the line feedN (\n = RETURN, \x14 = DEL, \x03 = RUN/STOP);
# feed9 forces the keyboard matrix: CIA1 port B bit 7 output low
c64-io: $(C64_BUILD)/io_check.prg
	sh tests/vice_run.sh --snap --check "python3 tools/check_io.py" \
	  --on feed1 'keybuf 12.5\n' --on feed2 'keybuf -3\n' \
	  --on feed3 'keybuf abc\n' --on feed4 'keybuf \n' \
	  --on feed5 'keybuf 1\x147\n' --on feed6 'keybuf -\n' \
	  --on feed7 'keybuf 4\x03' --on feed10 'keybuf 1111111111111111111111111\n' \
	  --on feed11 'keybuf 1e3\n' --on feed12 'keybuf -2.5E-2\n' \
	  --on feed13 'keybuf +3\n' --on feed14 'keybuf \x20\x207\x20\n' --on feed15 'keybuf -+3\n' \
	  --on feed8 'keybuf \x03' \
	  --on feed9 'bank io' --on feed9 '> dc03 80' --on feed9 '> dc01 7f' --on feed9 'bank ram' \
	  $(C64_BUILD)/io_check.prg PASS $(C64_IO_CYCLES) FAIL

clean:
	$(RM) -r $(BUILD)
