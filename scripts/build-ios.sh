#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
[ "$(cat VERSION)" = 0.2 ] || { echo 'Version must remain 0.2.' >&2; exit 1; }
usage() {
    echo 'Usage: scripts/build-ios.sh [iphoneos|iphonesimulator] [--multicast=off|on] [--run] [--device NAME_OR_ID] [--list-devices] [Xcode build settings...]'
    echo 'Default: multicast off; manual IPv4 sending and receiving remain available.'
}
sdk=iphonesimulator
case "${1:-}" in
 iphoneos|iphonesimulator) sdk=$1; shift ;;
 --*|-h|'') ;;
 *) case "$1" in *=*) ;; *) usage >&2; exit 1;; esac ;;
esac
multicast=off
run=no
list_devices=no
device=
while [ "$#" -gt 0 ]; do
    case "$1" in
      --multicast=on) multicast=on; shift ;;
      --multicast=off) multicast=off; shift ;;
      --run) run=yes; shift ;;
      --list-devices) list_devices=yes; shift ;;
      --device=*) device=${1#--device=}; [ -n "$device" ] || { echo 'Provide a device name or ID.' >&2; exit 1; }; shift ;;
      --device) [ "$#" -ge 2 ] && [ -n "$2" ] || { echo 'Provide a device name or ID.' >&2; exit 1; }; device=$2; shift 2 ;;
      --help|-h) usage; exit 0 ;;
      --*) echo "Unknown build option: $1" >&2; usage >&2; exit 1 ;;
      *) break ;;
    esac
done
if [ "$sdk" != iphonesimulator ] && { [ "$run" = yes ] || [ "$list_devices" = yes ] || [ -n "$device" ]; }; then
    echo '--run, --device and --list-devices require iphonesimulator, not iphoneos.' >&2; exit 1
fi
if [ "$list_devices" = yes ]; then python3 scripts/run-ios.py --list-devices; exit 0; fi
if [ -n "$device" ] && [ "$run" != yes ]; then echo 'Use --device together with --run.' >&2; exit 1; fi
for setting in "$@"; do
    case "$setting" in
      BITSKIM_BROADCAST=*|BITSKIM_BROADCAST_DEFINE=*|BITSKIM_ENTITLEMENTS=*|CODE_SIGN_ENTITLEMENTS=*|GCC_PREPROCESSOR_DEFINITIONS=*)
        echo 'Use --multicast=on or --multicast=off instead of overriding discovery settings.' >&2; exit 1 ;;
      --*) echo 'Place script options before Xcode build settings.' >&2; exit 1 ;;
    esac
done
if [ "$multicast" = on ]; then
    broadcast=YES; broadcast_define=; entitlements="$PWD/clients/ios/Discovery.entitlements"
else
    broadcast=NO; broadcast_define=BS_NO_BROADCAST; entitlements="$PWD/clients/ios/Manual.entitlements"
fi
echo "Multicast/broadcast discovery: $multicast"
[ -f "work/openssl-ios-$sdk/lib/libssl.a" ] || ./scripts/build-openssl-ios.sh "$sdk"
command -v xcodegen >/dev/null || { echo 'Install XcodeGen, or set PATH to an existing installation.' >&2; exit 1; }
mkdir -p build/ios
xcodegen generate --spec clients/ios/project.yml --project build/ios
# Device builds stay unsigned; --run signs the local test app to run on this Mac.
signing=NO
if [ "$run" = yes ]; then signing=YES; fi
xcodebuild -project build/ios/BitSkim.xcodeproj -scheme BitSkim -configuration Debug -sdk "$sdk" -derivedDataPath build/ios-derived ARCHS=arm64 "CODE_SIGNING_ALLOWED=$signing" "$@" "BITSKIM_BROADCAST=$broadcast" "BITSKIM_BROADCAST_DEFINE=$broadcast_define" "BITSKIM_ENTITLEMENTS=$entitlements" clean build
python3 scripts/check-ios-build.py "$sdk" "$multicast"
printf 'Built experimental iOS client: build/ios-derived/Build/Products/Debug-%s/BitSkim.app\n' "$sdk"
if [ "$run" = yes ]; then
    if [ -n "$device" ]; then python3 scripts/run-ios.py --device "$device"
    else python3 scripts/run-ios.py
    fi
fi
