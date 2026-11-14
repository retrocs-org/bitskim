#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = 0.2 ] || { echo 'Version must remain 0.2.' >&2; exit 1; }
sdk=${1:-iphonesimulator}
if [ "$#" -gt 0 ]; then shift; fi
case "$sdk" in iphoneos|iphonesimulator) ;; *) echo 'Use iphoneos or iphonesimulator' >&2; exit 1;; esac
[ -f "work/openssl-ios-$sdk/lib/libssl.a" ] || ./scripts/build-openssl-ios.sh "$sdk"
command -v xcodegen >/dev/null || { echo 'Install XcodeGen, or set PATH to an existing installation.' >&2; exit 1; }
mkdir -p build/ios
xcodegen generate --spec clients/ios/project.yml --project build/ios
# Device builds are unsigned until a developer selects a team/provisioning profile.
xcodebuild -project build/ios/BitSkim.xcodeproj -scheme BitSkim -configuration Debug -sdk "$sdk" -derivedDataPath build/ios-derived ARCHS=arm64 CODE_SIGNING_ALLOWED=NO "$@" clean build
python3 scripts/check-ios-build.py "$sdk"
printf 'Built experimental iOS client: build/ios-derived/Build/Products/Debug-%s/BitSkim.app\n' "$sdk"
