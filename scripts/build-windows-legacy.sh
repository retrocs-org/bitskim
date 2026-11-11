#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
CC=${LEGACY_WINDOWS_CC:-i686-w64-mingw32-gcc}
RC=${LEGACY_WINDOWS_RC:-i686-w64-mingw32-windres}
[ "$(cat VERSION)" = "0.1" ] || exit 1
python3 scripts/build_provenance.py begin windows-legacy
mkdir -p build/windows-legacy
"$RC" -I. clients/windows-legacy/BitSkim.rc -O coff -o build/windows-legacy/resources.o
"$CC" -std=c99 -Os -march=i486 -Wall -Wextra -Werror -Wno-misleading-indentation \
 -DBS_WINDOWS_LEGACY -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -D__USE_MINGW_ANSI_STDIO=0 -Icore \
 -D__MSVCRT_VERSION__=0x0600 -D_USE_32BIT_TIME_T -mwindows -nostdlib -Wl,-e,_legacy_entry -Wl,--major-subsystem-version,4,--minor-subsystem-version,0 \
 core/bitskim.c core/platform.c clients/windows-legacy/main.c clients/windows-legacy/entry.c build/windows-legacy/resources.o \
 -o build/windows-legacy/BitSkim.exe -lwsock32 -lcomdlg32 -lshell32 -lgdi32 -luser32 -lkernel32 -lmsvcrt-os -lgcc
python3 scripts/check-windows-legacy.py build/windows-legacy/BitSkim.exe
python3 scripts/build_provenance.py finish windows-legacy --tool "$CC"
