#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.1" ] || { echo 'Version must remain 0.1.' >&2; exit 1; }
AMIGA_CC=${AMIGA_CC:-m68k-amigaos-gcc}
python3 scripts/build-amiga-guide.py
python3 scripts/build_provenance.py begin amiga
mkdir -p build/amiga
"$AMIGA_CC" -std=gnu89 -Os -m68000 -mcrt=nix20 -D__amigaos__ -Wall -Wextra -Wno-pointer-sign -Wno-discarded-qualifiers -Icore core/bitskim.c core/platform.c clients/amiga/main.c -o build/amiga/BitSkim -lamiga
"$AMIGA_CC" -std=gnu89 -Os -m68000 -mcrt=nix20 -D__amigaos__ -DBITSKIM_PREFS -Wall -Wextra -Wno-pointer-sign -Wno-discarded-qualifiers -Icore core/bitskim.c core/platform.c clients/amiga/main.c -o build/amiga/BitSkimPrefs -lamiga
if [ -f assets/icons/BitSkim.info ]; then
    cp assets/icons/BitSkim.info build/amiga/BitSkim.info
    cp assets/icons/BitSkimPrefs.info build/amiga/BitSkimPrefs.info
    cp assets/icons/BitSkimDrawer.info build/amiga/BitSkimDrawer.info
fi
printf 'Built %s/build/amiga/BitSkim (68000 Hunk executable)\n' "$PWD"
"$AMIGA_CC" -std=gnu89 -Os -m68000 -mcrt=nix20 -Wall -Wextra installers/amiga/start-installer.c -o build/amiga/StartInstaller -lamiga
"$AMIGA_CC" -std=gnu89 -Os -m68000 -mcrt=nix20 -Wall -Wextra installers/amiga/start-bitskim.c -o build/amiga/StartBitSkim -lamiga
"$AMIGA_CC" -std=gnu89 -Os -m68000 -mcrt=nix20 -Wall -Wextra installers/amiga/stop-bitskim.c -o build/amiga/StopBitSkim -lamiga
python3 scripts/build-amiga-installer.py

python3 scripts/build_provenance.py finish amiga --tool "$AMIGA_CC"
