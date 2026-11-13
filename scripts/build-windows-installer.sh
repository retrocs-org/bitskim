#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.2" ] || exit 1
if [ -z "${MAKENSIS:-}" ] && ! command -v makensis >/dev/null 2>&1; then
 MAKENSIS="$PWD/work/nsis-toolchain/makensis/3.13/bin/makensis"
 NSISDIR="$PWD/work/nsis-toolchain/makensis/3.13/share/nsis"
 export NSISDIR
fi
python3 scripts/build_provenance.py begin setup
mkdir -p outputs
"${MAKENSIS:-makensis}" -V3 installers/windows/BitSkim.nsi

python3 scripts/build_provenance.py finish setup --tool "${MAKENSIS:-makensis}" --version-arg=-VERSION

python3 scripts/sync-builds.py
