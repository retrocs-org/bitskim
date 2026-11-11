#!/bin/sh
# Build a pinned LHa writer in the shared tools directory.
set -eu
cd "$(dirname "$0")/.."
for tool in curl tar autoreconf make cc python3; do
    command -v "$tool" >/dev/null || { echo "Required build tool missing: $tool" >&2; exit 1; }
done
LHA_PREFIX=${LHA_PREFIX:-$HOME/Development/tools/amiga/lha}
mkdir -p "$LHA_PREFIX/build"
cd "$LHA_PREFIX"
export LHA_PREFIX
curl -fL https://codeload.github.com/jca02266/lha/tar.gz/16619b066b189ef289bb8b07b37d1c38d550da99 -o lha-source.tar.gz
python3 - <<'PY'
import hashlib
from pathlib import Path
p=Path('lha-source.tar.gz')
if hashlib.sha256(p.read_bytes()).hexdigest()!='171cb8bc09ee9610b09d7366ed5e5459972d790937fbcce385e7cb89d1910226':
    raise SystemExit('LHa source checksum mismatch; refusing to build')
PY
tar -xzf lha-source.tar.gz -C build --strip-components=1
cd build
autoreconf -is
./configure --prefix="$LHA_PREFIX" --disable-multibyte-filename
make
make install
printf 'Shared writer ready: %s/bin/lha\n' "$LHA_PREFIX"
