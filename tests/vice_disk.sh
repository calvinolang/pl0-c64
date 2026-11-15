#!/bin/sh
# The KERNAL disk primitives (src/c64/disk.c) in headless VICE:
# build/c64/disk_check.prg (tests/c64/disk_check.c) with a copy of
# build/pl0.d64 attached as device 8, true drive emulation (a 1541 running
# its own DOS: the real serial protocol, SEQ files and error channel).
# The screen must show every expected line; afterwards the file the
# program saved is read back from the image with c1541 and compared.
# Never run the disk programs with VICE's virtual device and no image
# attached: the virtual device then writes to the host's current directory.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 2
img=build/vice/disk_check.d64
mkdir -p build/vice
cp build/pl0.d64 $img || exit 1
sh tests/vice_run.sh --vice -drive8type --vice 1541 --vice -drive8truedrive \
  --vice +virtualdev8 --vice -8 --vice $img \
  build/c64/disk_check.prg DONE 400000000 FAIL > build/vice/disk_check.log 2>&1
rc=$?
fail=0
for want in 'dev 8' 'dir "PL0 TURTLE      " 25 2A' 'dir "PL0"              PRG' \
    'dir "INDEX"            SEQ' 'dir last "TREE"             SEQ' 'dir lines 19' 'dir close 0' 'idx SQUARE a square' 'idx close 0' \
    's1 0' 's2 63' 's3 0' 'l1 0' 'l1 same' 'l2 62' 'l3 253' 'l3 kept'; do
  grep -qxF "$want" build/vice/disk_check/screen.txt 2>/dev/null || { echo "vice_disk: missing line '$want'"; fail=1; }
done
printf '{ disk test }\n!1+2.\n' > build/vice/disktest.want
rm -f build/vice/disktest.got
c1541 $img -read disktest,s build/vice/disktest.got > /dev/null 2>&1
cmp -s build/vice/disktest.want build/vice/disktest.got ||
  { echo "vice_disk: DISKTEST on the image differs from what was saved"; fail=1; }
if [ $rc -ne 0 ] || [ $fail -ne 0 ]; then
  cat build/vice/disk_check.log
  echo "vice_disk: FAILED"
  exit 1
fi
echo "vice_disk: OK (dir, INDEX, save, exists, replace, load, not found, too big; bytes on the image)"
