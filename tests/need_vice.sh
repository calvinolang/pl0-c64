#!/bin/sh
# Preflight of the on-target tests (make smoke, check-full): VICE (x64sc,
# c1541), the C64 and 1541 ROMs VICE needs, and cc65.
# Exit 0: all present. Exit 3: VICE or its ROMs missing and ALLOW_NO_VICE=1
# (the caller prints SKIPPED and succeeds). Exit 1: something is missing;
# the message says how to install it (Ubuntu).
missing_vice=
command -v x64sc > /dev/null 2>&1 || missing_vice="$missing_vice x64sc"
command -v c1541 > /dev/null 2>&1 || missing_vice="$missing_vice c1541"
if [ -n "$missing_vice" ]; then
  echo "VICE not installed (missing:$missing_vice)."
  echo "  Install it on Ubuntu with:  sudo apt-get install vice"
  echo "  (package 'vice', in universe; 3.7.1 is tested), then the ROMs: tools/install_vice_roms.sh"
  [ "$ALLOW_NO_VICE" = 1 ] && exit 3
  echo "  Or run with ALLOW_NO_VICE=1 to skip the on-target tests."
  exit 1
fi

# The Debian/Ubuntu package leaves out the Commodore ROMs (DFSG). VICE
# looks for them in these directories (C64/ and DRIVES/ below each).
roms="C64/kernal-901227-03.bin C64/basic-901226-01.bin C64/chargen-901225-01.bin
DRIVES/dos1541-325302-01+901229-05.bin DRIVES/dos1541ii-251968-03.bin"
dirs="${XDG_DATA_HOME:-$HOME/.local/share}/vice $HOME/.vice /usr/share/vice /usr/lib/vice
/usr/local/share/vice /usr/local/lib/vice /usr/lib/x86_64-linux-gnu/vice"
missing_rom=
for r in $roms; do
  found=
  for d in $dirs; do [ -f "$d/$r" ] && { found=1; break; }; done
  [ -n "$found" ] || missing_rom="$missing_rom $r"
done
if [ -n "$missing_rom" ]; then
  echo "VICE is installed but its ROMs are missing:$missing_rom"
  echo "  The Ubuntu/Debian 'vice' package ships without them. Install them with"
  echo "  tools/install_vice_roms.sh (from the VICE 3.7.1 source release, checked by SHA-256,"
  echo "  into ~/.local/share/vice)."
  [ "$ALLOW_NO_VICE" = 1 ] && exit 3
  echo "  Or run with ALLOW_NO_VICE=1 to skip the on-target tests."
  exit 1
fi

missing_cc65=
for t in cc65 ca65 cl65; do
  command -v $t > /dev/null 2>&1 || missing_cc65="$missing_cc65 $t"
done
if [ -n "$missing_cc65" ]; then
  echo "cc65 not installed (missing:$missing_cc65)."
  echo "  Install it on Ubuntu with:  sudo apt-get install cc65"
  exit 1
fi
exit 0
