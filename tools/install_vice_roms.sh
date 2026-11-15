#!/bin/sh
# Install the five ROM images the tests need into ~/.local/share/vice
# (C64/ and DRIVES/), where VICE looks for them. The Ubuntu/Debian `vice`
# package leaves the Commodore ROMs out (DFSG); the upstream VICE source
# release carries them in data/C64 and data/DRIVES.
# Usage: tools/install_vice_roms.sh [VICE-SOURCE-TARBALL]
#   Without an argument, downloads vice-3.7.1.tar.gz from SourceForge
#   (VICE_TARBALL_URL overrides the address). Every file is checked
#   against the SHA-256 below before it is installed; nothing else from
#   the tarball is used.
set -e
URL=${VICE_TARBALL_URL:-https://sourceforge.net/projects/vice-emu/files/releases/vice-3.7.1.tar.gz/download}
DEST=${XDG_DATA_HOME:-$HOME/.local/share}/vice
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
if [ $# -ge 1 ]; then
  tarball=$1
else
  tarball=$tmp/vice.tar.gz
  echo "install_vice_roms: downloading $URL"
  curl -fsSL -o "$tarball" "$URL"
fi
# SHA-256 of the ROMs (SHA-1 of the three C64 ROMs as catalogued for
# these part numbers: kernal 1d503e56..., basic 79015323..., chargen
# adc7c31e...)
cat > "$tmp/sums" <<'EOF'
83c60d47047d7beab8e5b7bf6f67f80daa088b7a6a27de0d7e016f6484042721  C64/kernal-901227-03.bin
89878cea0a268734696de11c4bae593eaaa506465d2029d619c0e0cbccdfa62d  C64/basic-901226-01.bin
fd0d53b8480e86163ac98998976c72cc58d5dd8eb824ed7b829774e74213b420  C64/chargen-901225-01.bin
d1d45afb46fd4e2b48d93ca367b889d75654a3b7acf73044e51f6d880c09369e  DRIVES/dos1541-325302-01+901229-05.bin
326c289c38753323d7e8167897447cf61ef35189d82eb8d75210ece949adda7c  DRIVES/dos1541ii-251968-03.bin
EOF
mkdir -p "$tmp/x"
files=$(awk '{ print "*/data/" $2 }' "$tmp/sums")
# shellcheck disable=SC2086
tar -xzf "$tarball" -C "$tmp/x" --wildcards $files
mkdir -p "$tmp/roms/C64" "$tmp/roms/DRIVES"
for f in $(awk '{ print $2 }' "$tmp/sums"); do
  src=$(find "$tmp/x" -path "*/data/$f" | head -1)
  [ -n "$src" ] || { echo "install_vice_roms: $f not in $tarball"; exit 1; }
  cp "$src" "$tmp/roms/$f"
done
(cd "$tmp/roms" && sha256sum -c --quiet "$tmp/sums") ||
  { echo "install_vice_roms: checksum mismatch, nothing installed"; exit 1; }
mkdir -p "$DEST/C64" "$DEST/DRIVES"
for f in $(awk '{ print $2 }' "$tmp/sums"); do cp "$tmp/roms/$f" "$DEST/$f"; done
echo "install_vice_roms: installed 5 ROMs into $DEST"
