#!/bin/sh
# SPIKE: build everything and run it headless in VICE (x64sc -console).
# Outputs (all under out/):
#   spike.txt        ROM float results (decoded from logbuf)
#   rom-disasm.txt   disassembly of each ROM entry point used by fp.s
#   weight.txt       VM benchmark + RAM-under-BASIC check
#   gfx-bitmap.png   screenshot in bitmap mode, gfx-text.png after restore
#   gfx-regs.txt     VIC/CIA register dumps in both states
#   sizes.txt        segment summary of every map
set -e
cd "$(dirname "$0")"
make
OUT=$PWD/out
addr() { grep " \.$2\$" "$OUT/$1.lbl" | awk '{print substr($2,3)}'; }
hexadd() { printf '%04x' $((0x$1 + $2)); }

echo "== spike.prg (ROM floats)"
LB=$(addr spike _logbuf)
CYCLES=200000000 python3 vicemon.py out/spike.prg out/spike.lbl ._spike_done out/vice-spike \
    "bsave \"$OUT/spike-log.bin\" 0 $LB $(hexadd $LB 3071)" \
    "bank rom" \
    "d bba2 bba9" "d ba8c ba93" "d bbd4 bbdb" "d b850 b857" "d b867 b86e" \
    "d ba28 ba2f" "d bb0f bb16" "d bddd bde4" "d b391 b398" "d b1aa b1b1" \
    "d bcf3 bcfa" "d bc5b bc62" "d bc58 bc5f" "d bccc bcd3" "d bf71 bf78" \
    "d bfed bff4" "d b9ea b9f1" "d bfb4 bfbb" "d e264 e26b" "d e26b e272" \
    "d e30e e315" "d a437 a43a" "m 0300 0301" "r"
python3 petscii.py out/spike-log.bin > out/spike.txt
sed -n '/>>> bank rom/,$p' out/vice-spike/monitor.log > out/rom-disasm.txt
cp out/vice-spike/screen.png out/spike-screen.png
cat out/spike.txt

for w in weight weight2 weight2-Oirs weight3 weight3-Oirs weight4; do
    echo "== $w.prg (VM benchmark, RAM under BASIC)"
    LB=$(addr $w _logbuf)
    CYCLES=400000000 python3 vicemon.py out/$w.prg out/$w.lbl ._spike_done out/vice-$w \
        "bsave \"$OUT/$w-log.bin\" 0 $LB $(hexadd $LB 1023)"
    python3 petscii.py out/$w-log.bin > out/$w.txt
    cat out/$w.txt
done

echo "== gfx.prg (bitmap in VIC bank 3)"
CYCLES=100000000 python3 vicemon.py out/gfx.prg out/gfx.lbl ._shown_bitmap out/vice-gfx \
    "screenshot \"$OUT/gfx-bitmap.png\" 2" \
    "m dd00 dd00" "m d011 d011" "m d016 d016" "m d018 d018" "m 0001 0001" \
    "bank ram" "m e000 e027" "m cc00 cc0f" "bank cpu" \
    "break ._shown_text" "x" \
    "screenshot \"$OUT/gfx-text.png\" 2" \
    "m dd00 dd00" "m d011 d011" "m d016 d016" "m d018 d018" "bsave \"$OUT/gfx-textscreen.bin\" 0 0400 07e7"
sed -n '/>>> m dd00/,$p' out/vice-gfx/monitor.log > out/gfx-regs.txt
python3 screen2txt.py out/gfx-textscreen.bin > out/gfx-textscreen.txt
echo "text screen after gfx_close:"; grep -v '^ *$' out/gfx-textscreen.txt

for m in spike weight weight-Oirs weight2 weight2-Oirs weight3 weight3-Oirs weight4 gfx weight-draft-stock; do
    echo "--- $m.map"; sed -n '/^Segment list/,/^Exports/p' out/$m.map | grep -E '^[A-Z]+ '
done > out/sizes.txt
cat out/sizes.txt
echo "--- weight-draft-pl0 (draft limits, proposed config)"; cat out/weight-draft-pl0.txt
