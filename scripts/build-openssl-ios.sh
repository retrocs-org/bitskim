#!/bin/sh
# Static TLS builds are kept outside the source distribution.
set -eu
cd "$(dirname "$0")/.."
sdk=${1:-iphonesimulator}
case "$sdk" in
 iphoneos) target=ios64-xcrun ;;
 iphonesimulator) target=iossimulator-arm64-xcrun ;;
 *) echo 'Use iphoneos or iphonesimulator' >&2; exit 1 ;;
esac
version=3.5.9
archive="work/openssl-src/openssl-$version.tar.gz"
mkdir -p work/openssl-src "work/openssl-build/ios-$sdk"
if [ ! -f "$archive" ]; then curl -fL "https://github.com/openssl/openssl/releases/download/openssl-$version/openssl-$version.tar.gz" -o "$archive"; fi
printf '%s  %s\n' 603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a "$archive" | shasum -a 256 -c -
[ -d "work/openssl-src/openssl-$version" ] || tar -xzf "$archive" -C work/openssl-src
root=$PWD
cd "work/openssl-build/ios-$sdk"
export IPHONEOS_DEPLOYMENT_TARGET=16.0
perl "../../openssl-src/openssl-$version/Configure" "$target" no-shared no-tests no-docs no-module --prefix="$root/work/openssl-ios-$sdk" --libdir=lib
make -j6
make install_sw
