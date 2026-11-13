#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = "0.2" ] || { echo 'Version must remain 0.2 until explicitly changed.' >&2; exit 1; }
[ -f work/openssl-macos/lib/libssl.a ] || scripts/build-openssl.sh macos
python3 scripts/build_provenance.py begin macos
mkdir -p build/macos/BitSkim.app/Contents/MacOS build/macos/BitSkim.app/Contents/Resources build/module-cache
export CLANG_MODULE_CACHE_PATH="$PWD/build/module-cache"
clang -std=c89 -Os -Wall -Wextra -mmacosx-version-min=13.0 -DBS_OPENSSL -Iwork/openssl-macos/include -Icore -c core/bitskim.c -o build/macos/bitskim.o
clang -std=c89 -Os -Wall -Wextra -mmacosx-version-min=13.0 -DBS_OPENSSL -Iwork/openssl-macos/include -Icore -c core/platform.c -o build/macos/platform.o
swiftc -parse-as-library -swift-version 5 -O -target arm64-apple-macos13.0 -module-cache-path build/module-cache -Icore -import-objc-header clients/macos/Bridge.h clients/macos/Themes.swift clients/macos/BitSkim.swift clients/macos/AppController.swift build/macos/bitskim.o build/macos/platform.o work/openssl-macos/lib/libssl.a work/openssl-macos/lib/libcrypto.a -o build/macos/BitSkim.app/Contents/MacOS/BitSkim
cp work/openssl-src/openssl-3.5.9/LICENSE.txt build/macos/BitSkim.app/Contents/Resources/OpenSSL-LICENSE.txt
cp clients/macos/Info.plist build/macos/BitSkim.app/Contents/Info.plist
if [ -f assets/icons/BitSkim.icns ]; then cp assets/icons/BitSkim.icns build/macos/BitSkim.app/Contents/Resources/BitSkimRetro.icns; fi
cp assets/icons/BitSkim-menu.png build/macos/BitSkim.app/Contents/Resources/
identity=${BITSKIM_SIGN_IDENTITY:--}
if [ "$identity" = - ]; then
    codesign --force --deep --sign - build/macos/BitSkim.app
else
    codesign --force --options runtime --timestamp --sign "$identity" build/macos/BitSkim.app
fi
codesign --verify --deep --strict build/macos/BitSkim.app
printf 'Built %s/build/macos/BitSkim.app (signed; notarization is a separate release step)\n' "$PWD"

python3 scripts/build_provenance.py finish macos --tool "swiftc"
