#!/bin/sh
# Compile and package BitSkim 0.1 without building other platforms or publishing.
set -eu
cd "$(dirname "$0")/.."
mode=build
case "${1:-}" in
    --aminet-upload) shift; exec python3 scripts/aminet_release.py upload "$@" ;;
    --aminet-check) shift; exec python3 scripts/aminet_release.py check "$@" ;;
    --aminet-prepare) mode=prepare; shift ;;
    --help|-h)
        echo 'Build: ./scripts/release-amiga.sh'
        echo 'Prepare: ./scripts/release-amiga.sh --aminet-prepare --uploader "email (Name)" [--author "Name"]'
        echo 'Check: ./scripts/release-amiga.sh --aminet-check'
        echo 'Upload later: ./scripts/release-amiga.sh --aminet-upload --confirm-upload'
        exit 0 ;;
    "") ;;
    *) echo 'Unknown option. Use --help.' >&2; exit 1 ;;
esac
[ "$(cat VERSION)" = "0.1" ] || { echo 'Version must remain 0.1.' >&2; exit 1; }
if [ -z "${AMIGA_CC:-}" ] && [ -x work/toolchain/m68k-amigaos-gcc-16.2/bin/m68k-amigaos-gcc ]; then
    AMIGA_CC="$PWD/work/toolchain/m68k-amigaos-gcc-16.2/bin/m68k-amigaos-gcc"
    export AMIGA_CC
fi
command -v "${AMIGA_CC:-m68k-amigaos-gcc}" >/dev/null || { echo 'Set AMIGA_CC to your Amiga cross-compiler.' >&2; exit 1; }
if ! python3 scripts/amiga_release.py --check-writer; then
    [ -z "${LHA:-}" ] || { echo 'The configured LHA writer is invalid.' >&2; exit 1; }
    echo 'Building a local LHA writer (requires network access and host C/autotools).'
    ./scripts/bootstrap-lha.sh
fi
./scripts/build-amiga.sh
python3 scripts/amiga_release.py

python3 scripts/source_release.py
python3 scripts/aminet_release.py template
if [ -n "${AMIGA_DEV_BIN:-}" ] || [ -f work/amiga-dev-bin.txt ]; then
    python3 scripts/sync-amiga-bin.py
fi
if [ "$mode" = prepare ]; then
    python3 scripts/aminet_release.py prepare "$@"
fi

python3 scripts/sync-builds.py
