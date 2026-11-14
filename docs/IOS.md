# iPhone and iPad — experimental 0.2

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

This is a source-only iOS client in development, targeting iOS/iPadOS 16 or later on arm64. There is no App Store, TestFlight or publicly signed iOS download yet. It uses the existing C transfer engine and pinned OpenSSL 3.5.9, with a SwiftUI interface and a native Share extension. Physical-device UI, permission, Files-provider and Share-extension testing remain required before distribution.

## What it does

- Send up to 32 selected regular files per import, each up to 2 GiB minus one byte. Files are staged in private storage before queueing; imported folders are rejected explicitly.
- Receive files and complete folder manifests, including empty directories, into `Received` in the app's Documents directory. Existing files are preserved with collision-safe names.
- Browse received folders and use system sharing to save/export files. The Documents directory is also available through Files.
- Prefer or require TLS, compare verification codes, remember device identities, show diagnostics/history and CRC32 readback results, retry queued sends and cancel transfers.
- Show the Wi-Fi/LAN address for manual incoming connections.
- Accept files/photos from other apps through the Share extension. The extension copies them to an App Group inbox. Open BitSkim, then select a computer or enter an address to send them. The extension does not silently open the containing app or send to a guessed recipient.

Shared-inbox files stay on the device until queued. Failed or cancelled import copies are cleaned without deleting the originals. An interrupted batch can leave already completed received files; explicit cancellation removes the current partial download. Temporary staging requires enough local storage for a copy of each selected file.

Keep BitSkim open to receive. On entering the background, it cancels active/queued transfers, closes its listeners and clears outgoing copies; completed received files remain. Opening it reconnects. Continuous background receiving and guaranteed completion while locked are not supported. There is no automatic acceptance of incoming files. The mobile UI uses verification-code device pairing; it does not yet expose shared-group PSK key entry.

## Manual builds

Use an Apple silicon Mac, full Xcode with iOS SDKs, XcodeGen, Python 3.9+, Perl, make, curl and a POSIX shell. Install tools outside the source checkout and put them in PATH. Initial TLS dependency downloads are checksum-verified; generated dependency/build files live under ignored `work/` and `build/`. No repository credentials or release-tool configuration is needed.

```sh
# Local iOS test build
./scripts/build-ios.sh iphonesimulator

# Unsigned arm64 device build
./scripts/build-ios.sh iphoneos
```

The generated Xcode project is `build/ios/BitSkim.xcodeproj`. Products are under `build/ios-derived/Build/Products/Debug-<sdk>/BitSkim.app`. The Share extension is embedded in the app. The device product is deliberately unsigned and cannot be installed directly until provisioned. These scripts are separate from the desktop/retro release pipeline; no unsigned iOS artifact is uploaded by the release tool.

To install a development build, open the generated project in Xcode, select the BitSkim scheme and your connected iPhone/iPad, and set your development team for both targets. `BITSKIM_BUNDLE_ID` defaults to `org.bitskim.ios`; change it to an identifier available to your team. The extension derives its `.share` identifier from it. `BITSKIM_APP_GROUP` defaults to `group.$(BITSKIM_BUNDLE_ID)` and must match the App Group provisioned for both targets. Keep team-specific settings local or pass build settings on the command line. Regenerating the project replaces edits to the generated project file.

```sh
./scripts/build-ios.sh iphoneos \
  CODE_SIGNING_ALLOWED=YES \
  DEVELOPMENT_TEAM=YOUR_TEAM_ID \
  BITSKIM_BUNDLE_ID=org.example.bitskim \
  BITSKIM_APP_GROUP=group.org.example.bitskim
```

Xcode must already have a valid development signing identity and suitable profiles. The script does not create accounts, register devices, request entitlements or publish to Apple.

## Discovery and permissions

The default build disables outgoing broadcasts at compile time with `BS_NO_BROADCAST`, leaves discovery visibility off, and uses manual IPv4 connections. The receiving device's address is shown in Computers → Receive on this device. Enter that address on the sender. Allow BitSkim's Local Network access when prompted; a denied permission can block manual connections too.

For automatic discovery, obtain Apple's multicast/broadcast entitlement and a matching provisioning profile. Then build with:

```sh
./scripts/build-ios.sh iphoneos \
  CODE_SIGNING_ALLOWED=YES \
  DEVELOPMENT_TEAM=YOUR_TEAM_ID \
  BITSKIM_BROADCAST=YES \
  BITSKIM_BROADCAST_DEFINE= \
  BITSKIM_ENTITLEMENTS="$(pwd)/clients/ios/Discovery.entitlements"
```

Retain any bundle/App Group overrides used for your manual build. `BITSKIM_BROADCAST` controls the UI/visibility and `BITSKIM_BROADCAST_DEFINE` controls the transport; set both consistently. The Share extension only stages files and does not request the multicast entitlement. Do not remove the saved-identity checks to work around signing or network problems.

Apple references: [local-network privacy](https://developer.apple.com/documentation/technotes/tn3179-understanding-local-network-privacy), [broadcast entitlement](https://developer.apple.com/news/?id=0oi77447), [document-import access](https://developer.apple.com/documentation/swiftui/view/fileimporter(ispresented:allowedcontenttypes:oncompletion:)), [background execution](https://developer.apple.com/documentation/uikit/extending-your-app-s-background-execution-time).

## Validation

Run the iOS-specific engine/staging unit suite after generating the project. Select an installed local iOS test device using Xcode's destination list:

```sh
xcodebuild -project build/ios/BitSkim.xcodeproj \
  -scheme BitSkimCoreTests \
  -destination 'platform=iOS Simulator,name=YOUR_TEST_DEVICE' \
  -derivedDataPath build/ios-test-derived \
  CODE_SIGNING_ALLOWED=NO test
```

The suite exercises the real iOS C engine and filesystem: automatic TLS, a Unicode binary payload with verified receipt, an empty file, nested folders/empty directories, cancellation after data starts, collision-safe shared staging, symbolic-link rejection, and rejecting oversized files/folders without committing partial inbox entries. Host tests additionally exercise manual address validation and the no-broadcast transport. All six iOS runtime tests passed on the local iOS 27.2 test runtime; both SDK builds compiled. Passing these checks is distinct from physical-device and visual app testing. Privacy manifests for app/extension file access and app settings/disk-space checks are included; distribution review still needs to assess the final linked dependencies.

Before distribution, test two physical devices or a phone and desktop: Local Network consent/denial, Wi-Fi discovery with the approved entitlement, manual receive/send in both directions, remembered identities across restarts, provider downloads, Photos sharing, Share-extension App Group access, low space, interruption, lock/background behavior and iPad layouts. Export compliance, App Store metadata, distribution signing and delivery are not configured by the development build.
