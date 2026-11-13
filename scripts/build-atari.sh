#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = 0.2 ] || exit 1
ATARI_CC=${ATARI_CC:-"$PWD/work/atari/sdk/opt/cross-mint/bin/m68k-atari-mint-gcc"}
STING_INCLUDE=${STING_INCLUDE:-"$PWD/work/atari/sting-include"}
command -v "$ATARI_CC" >/dev/null || { echo 'Run python3 scripts/bootstrap-atari.py or set ATARI_CC and STING_INCLUDE.' >&2; exit 1; }
for target in sting mintnet mintnet060; do
 python3 scripts/build_provenance.py begin "atari-$target"
 out="build/atari/$target"
 mkdir -p "$out"
 flags='-m68000'
 extra=''
 if [ "$target" = sting ]; then extra='-DBS_ATARI_STING'; fi
 if [ "$target" = mintnet060 ]; then flags='-m68020-60 -msoft-float'; extra=''; fi
 for unit in bitskim platform main; do
  src="clients/atari/$unit.c"
  if [ "$unit" = bitskim ]; then src=core/bitskim.c; fi
  "$ATARI_CC" $flags -std=gnu89 -Os -Wall -Wextra -Werror -DBS_ATARI $extra -Icore -I"$STING_INCLUDE" -c "$src" -o "$out/$unit.o"
 done
 objects="$out/bitskim.o $out/platform.o $out/main.o"
 if [ "$target" = sting ]; then
  "$ATARI_CC" $flags -std=gnu89 -Os -Wall -Wextra -Werror -DBS_ATARI $extra -Icore -I"$STING_INCLUDE" -c clients/atari/sting.c -o "$out/sting.o"
  objects="$objects $out/sting.o"
 fi
 "$ATARI_CC" $flags $objects -lgem -Wl,-Map,"$out/link.map" -o "$out/BITSKIM.PRG"
 python3 scripts/build_provenance.py finish "atari-$target" --tool "$ATARI_CC"
 printf 'Built experimental Atari %s: %s/BITSKIM.PRG\n' "$target" "$out"
done
