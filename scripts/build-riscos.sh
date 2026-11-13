#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.2" ] || { echo 'Version must remain 0.2.' >&2; exit 1; }
RISCOS_CC=${RISCOS_CC:-arm-riscos-aof-gcc}
for target in arm2 strongarm; do
    python3 scripts/build_provenance.py begin "riscos-$target"
    out="build/riscos/$target"
    mkdir -p "$out/!BitSkim"
    cp -R 'clients/riscos/app/!BitSkim/.' "$out/!BitSkim/"
    for unit in bitskim platform riscos-file; do
        "$RISCOS_CC" -mapcs-26 -mcpu="$target" -munixlib -Os -D__riscos__ -Wall -Werror -Icore -c "core/$unit.c" -o "$out/$unit.o"
    done
    "$RISCOS_CC" -mapcs-26 -mcpu="$target" -munixlib -Os -D__riscos__ -Wall -Werror -Icore -c clients/riscos/main.c -o "$out/main.o"
    "$RISCOS_CC" -mapcs-26 -mcpu="$target" -munixlib "$out/main.o" "$out/bitskim.o" "$out/platform.o" "$out/riscos-file.o" -o "$out/!BitSkim/!RunImage"
    # GCCSDK may append the Absolute filetype suffix on a Unix host.
    if [ -f "$out/!BitSkim/!RunImage,ff8" ]; then mv "$out/!BitSkim/!RunImage,ff8" "$out/!BitSkim/!RunImage"; fi
    [ -s "$out/!BitSkim/!RunImage" ] || { echo "Missing $target Absolute image" >&2; exit 1; }
    python3 scripts/build_provenance.py finish "riscos-$target" --tool "$RISCOS_CC"
    printf 'Built RISC OS 3.x %s APCS-26 development image\n' "$target"
done
