#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.2" ] || { echo 'Version must remain 0.2.' >&2; exit 1; }
WINDOWS_CC=${WINDOWS_CC:-x86_64-w64-mingw32-gcc}
WINDOWS_RC=${WINDOWS_RC:-x86_64-w64-mingw32-windres}
[ -f work/openssl-windows/lib/libssl.a ] || scripts/build-openssl.sh windows
python3 scripts/build_provenance.py begin windows
mkdir -p build/windows
"$WINDOWS_RC" -I. clients/windows/BitSkim.rc -O coff -o build/windows/resources.o
"$WINDOWS_CC" -std=c99 -Os -Wall -Wextra -Werror -D_WIN32_WINNT=0x0a00 -DWINVER=0x0a00 -DBS_OPENSSL -Iwork/openssl-windows/include -Icore -municode -mwindows -Wl,--major-subsystem-version,6,--minor-subsystem-version,0 -static \
    core/bitskim.c core/platform.c clients/windows/main.c build/windows/resources.o \
    -o build/windows/BitSkim.exe work/openssl-windows/lib/libssl.a work/openssl-windows/lib/libcrypto.a -lcrypt32 -lws2_32 -liphlpapi -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid -lbcrypt -ldwmapi -luxtheme
cp work/openssl-src/openssl-3.5.9/LICENSE.txt build/windows/OpenSSL-LICENSE.txt
printf 'Built %s/build/windows/BitSkim.exe (Windows 10 x64)\n' "$PWD"
"$WINDOWS_CC" -std=c99 -Os -Wall -Wextra -Werror -D_WIN32_WINNT=0x0a00 -DBS_OPENSSL -Iwork/openssl-windows/include -Icore -static \
    core/bitskim.c core/platform.c clients/windows/core-check.c \
    -o build/windows/BitSkim-CoreCheck.exe work/openssl-windows/lib/libssl.a work/openssl-windows/lib/libcrypto.a -lcrypt32 -lws2_32 -liphlpapi -lbcrypt

python3 scripts/build_provenance.py finish windows --tool "$WINDOWS_CC"
