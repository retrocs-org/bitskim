#!/bin/sh
# Pinned TLS dependency. Source and build output remain outside release source.
set -eu
cd "$(dirname "$0")/.."
target=${1:-macos}
case "$target" in macos|windows) ;; *) echo 'Use macos or windows' >&2; exit 1;; esac
version=3.5.9
archive="work/openssl-src/openssl-$version.tar.gz"
mkdir -p work/openssl-src "work/openssl-build/$target"
if [ ! -f "$archive" ]; then curl -fL "https://github.com/openssl/openssl/releases/download/openssl-$version/openssl-$version.tar.gz" -o "$archive"; fi
printf '%s  %s\n' 603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a "$archive" | shasum -a 256 -c -
[ -d "work/openssl-src/openssl-$version" ] || tar -xzf "$archive" -C work/openssl-src
root=$PWD
cd "work/openssl-build/$target"
if [ "$target" = macos ]; then
    export MACOSX_DEPLOYMENT_TARGET=13.0
    perl "../../openssl-src/openssl-$version/Configure" darwin64-arm64-cc no-shared no-tests no-docs no-module --prefix="$root/work/openssl-macos" --libdir=lib
else
    perl "../../openssl-src/openssl-$version/Configure" mingw64 no-shared no-tests no-docs no-module --cross-compile-prefix=x86_64-w64-mingw32- --prefix="$root/work/openssl-windows" --libdir=lib
fi
make -j6
make install_sw
