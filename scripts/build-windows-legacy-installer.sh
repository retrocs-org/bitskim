#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.2" ] || exit 1
if [ -z "${MAKENSIS:-}" ]; then
 if command -v makensis >/dev/null 2>&1; then MAKENSIS=makensis
 else
  MAKENSIS="$PWD/work/nsis-toolchain/makensis/3.13/bin/makensis"
  NSISDIR="$PWD/work/nsis-toolchain/makensis/3.13/share/nsis"
  export NSISDIR
 fi
fi
python3 scripts/build_provenance.py begin setup-legacy
mkdir -p outputs
"$MAKENSIS" -V3 installers/windows/BitSkim-Classic.nsi
python3 scripts/build_provenance.py finish setup-legacy --tool "$MAKENSIS" --version-arg=-VERSION
python3 - <<'PY'
from pathlib import Path
import hashlib
p=Path('outputs/BitSkim-0.2-Windows-Classic-x86-Setup.exe')
p.with_suffix('.exe.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
PY

python3 scripts/sync-builds.py
