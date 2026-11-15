# SPIKE (throwaway) -- ROM floats from cc65 and the C64 memory budget.
# `make` builds everything into out/; `./run.sh` builds and runs in VICE.
CL65   ?= cl65
CFG    := pl0-c64.cfg
OUT    := out
OPT    ?= -O
INC    := -I ../../src
WSRC   := weight/wmain.c weight/vm.c weight/editor.c weight/numw.c fp.s
W2SRC  := weight/wmain.c weight/vm2.c weight/editor.c weight/numw.c fp.s
WDEPS  := $(WSRC) weight/vm2.c weight/copy5.s weight/vmasm.s weight/weight.h $(CFG)
# proposed limits (see docs/architecture/spike-results.md)
PROPOSED := -DMAX_CODE=768 -DMAX_STACK=512 -DMAX_IDENTS=64 -DMAX_SOURCE=8192 \
            -DMAX_CONSTS=128 -DMAX_STRINGS=1024 -DCODE_IN_HIBSS -DSOURCE_IN_BASICBSS

.PHONY: all clean
all: $(OUT)/spike.prg $(OUT)/weight.prg $(OUT)/weight-Oirs.map $(OUT)/gfx.prg \
     $(OUT)/weight2.prg $(OUT)/weight2-Oirs.prg $(OUT)/weight3.prg $(OUT)/weight3-Oirs.prg $(OUT)/weight4.prg \
     $(OUT)/weight-draft-stock.map $(OUT)/weight-draft-pl0.txt

$(OUT):
	mkdir -p $@

# 1. ROM float proof, linked with the proposed config
$(OUT)/spike.prg: main.c fp.s $(CFG) | $(OUT)
	$(CL65) -t c64 $(OPT) -C $(CFG) -m $(OUT)/spike.map -Ln $(OUT)/spike.lbl -o $@ main.c fp.s

# 2a. weight stub at the DRAFT limits, stock cc65 layout (HIMEM $D000)
$(OUT)/weight-draft-stock.map: $(WDEPS) | $(OUT)
	$(CL65) -t c64 $(OPT) $(INC) -m $@ -o $(OUT)/weight-draft-stock.prg $(WSRC)

# 2b. weight stub at the DRAFT limits, proposed config: expected NOT to link
$(OUT)/weight-draft-pl0.txt: $(WDEPS) | $(OUT)
	-$(CL65) -t c64 $(OPT) $(INC) -C $(CFG) -o $(OUT)/weight-draft-pl0.prg $(WSRC) > $@ 2>&1
	@cat $@

# 3. weight stub at the PROPOSED limits and placement (runs the benchmark)
$(OUT)/weight.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 $(OPT) $(INC) -C $(CFG) $(PROPOSED) -m $(OUT)/weight.map -Ln $(OUT)/weight.lbl -o $@ $(WSRC)

# 3b. same with cc65's speed options, size only
$(OUT)/weight-Oirs.map: $(WDEPS) | $(OUT)
	$(CL65) -t c64 -Oirs $(INC) -C $(CFG) $(PROPOSED) -m $@ -o $(OUT)/weight-Oirs.prg $(WSRC)

# 3c. the cc65-friendly VM (vm2.c), -O and -Oirs; both are run by run.sh
$(OUT)/weight2.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 $(OPT) $(INC) -C $(CFG) $(PROPOSED) -DVM2 -m $(OUT)/weight2.map -Ln $(OUT)/weight2.lbl -o $@ $(W2SRC)
$(OUT)/weight2-Oirs.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 -Oirs $(INC) -C $(CFG) $(PROPOSED) -DVM2 -m $(OUT)/weight2-Oirs.map -Ln $(OUT)/weight2-Oirs.lbl -o $@ $(W2SRC)

# 3d. vm2.c with its 5-byte copy in assembly (copy5.s), -O
$(OUT)/weight3.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 $(OPT) $(INC) -C $(CFG) $(PROPOSED) -DVM2 -DASMCOPY -m $(OUT)/weight3.map -Ln $(OUT)/weight3.lbl -o $@ $(W2SRC) weight/copy5.s

$(OUT)/weight3-Oirs.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 -Oirs $(INC) -C $(CFG) $(PROPOSED) -DVM2 -DASMCOPY -m $(OUT)/weight3-Oirs.map -Ln $(OUT)/weight3-Oirs.lbl -o $@ $(W2SRC) weight/copy5.s

# 3e. weight3 + a minimal assembly dispatch loop for the LOD/STO benchmark
$(OUT)/weight4.prg: $(WDEPS) | $(OUT)
	$(CL65) -t c64 $(OPT) $(INC) -C $(CFG) $(PROPOSED) -DVM2 -DASMCOPY -DVMASM -m $(OUT)/weight4.map -Ln $(OUT)/weight4.lbl -o $@ $(W2SRC) weight/copy5.s weight/vmasm.s

# 4. bitmap in VIC bank 3
$(OUT)/gfx.prg: gfx/gfxtest.c gfx/gfx.s $(CFG) | $(OUT)
	$(CL65) -t c64 $(OPT) -C $(CFG) -m $(OUT)/gfx.map -Ln $(OUT)/gfx.lbl -o $@ gfx/gfxtest.c gfx/gfx.s

clean:
	rm -rf $(OUT) *.o weight/*.o gfx/*.o
