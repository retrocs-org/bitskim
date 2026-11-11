#!/bin/sh
# Build a pinned LHa writer locally; never install or replace system commands.
set -eu
cd "$(dirname "$0")/.."
for tool in curl tar autoreconf make cc python3; do
    command -v "$tool" >/dev/null || { echo "Required build tool missing: $tool" >&2; exit 1; }
done
mkdir -p work/lha-tool
curl -fL https://codeload.github.com/jca02266/lha/tar.gz/16619b066b189ef289bb8b07b37d1c38d550da99 -o work/lha-tool-source.tar.gz
python3 - <<'PY'
import hashlib
from pathlib import Path
p=Path('work/lha-tool-source.tar.gz')
if hashlib.sha256(p.read_bytes()).hexdigest()!='171cb8bc09ee9610b09d7366ed5e5459972d790937fbcce385e7cb89d1910226':
    raise SystemExit('LHa source checksum mismatch; refusing to build')
PY
tar -xzf work/lha-tool-source.tar.gz -C work/lha-tool --strip-components=1
cd work/lha-tool
autoreconf -is
./configure --disable-multibyte-filename
make
printf 'Local writer ready: work/lha-tool/src/lha\n'
