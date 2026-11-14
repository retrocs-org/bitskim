# iPhone and iPad — experimental 0.2

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

This is a source-only iOS client in development, targeting iOS/iPadOS 16 or later on arm64. There is no App Store, TestFlight or publicly signed iOS download yet. It uses the existing C transfer engine and pinned OpenSSL 3.5.9, with a SwiftUI interface and a native Share extension. Physical-device UI, permission, Files-provider and Share-extension testing remain required before distribution.

## What it does

- Send up to 32 selected regular files per import, each up to 2 GiB minus one byte. Files are staged in private storage before queueing; imported folders are rejected explicitly.
- Receive files and complete folder manifests, including empty directories, into `Received` in the app's Documents directory. Existing files are preserved with collision-safe names.
- Browse received folders, copy files or folders with **Save to Files**, and use system sharing for files. The Documents directory is also available through Files.
- Prefer or require TLS, compare verification codes, remember device identities, show diagnostics/history and CRC32 readback results, retry queued sends and cancel transfers.
- Show the Wi-Fi/LAN address for manual incoming connections.
- Accept files/photos from other apps through the Share extension. The extension copies them to an App Group inbox. Open BitSkim, then select a computer or enter an address to send them. The extension does not silently open the containing app or send to a guessed recipient.

Shared-inbox files stay on the device until queued. Failed or cancelled import copies are cleaned without deleting the originals. An interrupted batch can leave already completed received files; explicit cancellation removes the current partial download. Temporary staging requires enough local storage for a copy of each selected file.

Keep BitSkim open to receive. On entering the background, it cancels active/queued transfers, closes its listeners and clears outgoing copies; completed received files remain. Opening it reconnects. Continuous background receiving and guaranteed completion while locked are not supported. There is no automatic acceptance of incoming files. The mobile UI uses verification-code device pairing; it does not yet expose shared-group PSK key entry.

## Files and cloud storage

BitSkim uses Apple's Files picker, so you can select documents from iCloud Drive and enabled providers such as Dropbox and OneDrive. Install the provider's app, sign in, then open **Files → Browse → … → Edit** and enable its location. See [Apple's setup guide](https://support.apple.com/en-ie/102238).

To send, select a computer (or enter its address), choose **Choose from Files**, and browse to the provider. Selected regular files are downloaded/coordinated by Files and copied into BitSkim's outgoing storage before sending. Imported folders are not supported.

To export received content, open **Received**, tap the item's **…** menu and choose **Save to Files**. Choose a local or enabled cloud location. Files and folders are exported as copies; cancelling leaves the received original untouched. BitSkim pauses new incoming offers while its import/export picker is open. Finish or cancel an active transfer before opening Files.

Providers control availability, authentication, downloads and cloud syncing. BitSkim does not need separate Dropbox or OneDrive tokens, and a successful handoff to Files does not confirm that a remote cloud upload has finished. Physical-device testing with authenticated providers remains pending.

## Manual builds

Use an Apple silicon Mac, full Xcode with iOS SDKs, XcodeGen, Python 3.9+, Perl, make, curl and a POSIX shell. Install tools outside the source checkout and put them in PATH. Initial TLS dependency downloads are checksum-verified; generated dependency/build files live under ignored `work/` and `build/`. No repository credentials or release-tool configuration is needed.

```sh
# Local iOS test build
./scripts/build-ios.sh iphonesimulator

# Unsigned arm64 device build
./scripts/build-ios.sh iphoneos --multicast=off
```

## Build and run locally

```sh
# Build, boot a suitable local device, install and launch
./scripts/build-ios.sh --run

# Show available iOS devices without building
./scripts/build-ios.sh --list-devices

# Choose an exact name or device ID from that list
./scripts/build-ios.sh --run --device "DEVICE_NAME_OR_ID"

# Optional discovery-enabled local run
./scripts/build-ios.sh --multicast=on --run
```

`--run` defaults to `iphonesimulator`, selects an already booted compatible iOS device when available, otherwise an available iPhone, and opens Apple's Device Hub (or Simulator on older Xcode versions). Names shared by several runtimes are ambiguous; use the listed device ID instead. Installing updates the app while preserving its existing data, and only the selected BitSkim process is restarted. No device is erased or uninstalled. The local test app is signed to run on this Mac; a physical iPhone still needs development signing. If only headless Xcode tools are installed, the launcher reports that Device Hub/Simulator must be opened from a full Xcode installation.

Local iOS builds listen on TCP **46441**, allowing the desktop Mac app to keep **46440**. From BitSkim on that Mac, use `127.0.0.1:46441` to send to the local iOS app. From the local iOS app, use `127.0.0.1:46440` to send to the Mac client. From a different computer, use the Mac's LAN IPv4 address and port 46441. Physical iPhone/iPad builds keep TCP 46440. Discovery still uses UDP 46440. `--run` is rejected for `iphoneos`; it never installs on a physical phone.

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

The build switch controls the compiler flag, discovery visibility and signing-entitlements file together. It defaults to **off**:

```sh
# No multicast entitlement; manual IPv4 connections only
./scripts/build-ios.sh iphoneos --multicast=off

# Enable BitSkim's broadcast discovery and request the multicast entitlement
./scripts/build-ios.sh iphoneos --multicast=on
```

The on build still needs Apple's multicast/broadcast entitlement approval and a matching provisioning profile before it can be signed and installed on a device. If approval is unavailable or withdrawn, build with `--multicast=off`; this removes the multicast entitlement and compiles out outgoing broadcast traffic, while retaining manual IPv4 send/receive, TLS and device trust. Local Network consent and App Group provisioning are still needed.

Signing settings can follow the switch:

```sh
./scripts/build-ios.sh iphoneos --multicast=on \
  CODE_SIGNING_ALLOWED=YES \
  DEVELOPMENT_TEAM=YOUR_TEAM_ID
```

Retain any bundle/App Group overrides used for your manual build. Do not override `BITSKIM_BROADCAST`, `BITSKIM_BROADCAST_DEFINE`, `BITSKIM_ENTITLEMENTS`, `CODE_SIGN_ENTITLEMENTS` or `GCC_PREPROCESSOR_DEFINITIONS` through this script; use the switch so the settings remain consistent. The Share extension only stages files and never requests the multicast entitlement. `--help` prints the available options. Direct Xcode project defaults remain manual-only.

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

The suite exercises the real iOS C engine and filesystem: automatic TLS, identity-checked advertised endpoint selection, a Unicode binary payload with verified receipt, an empty file, nested folders/empty directories, cancellation after data starts, collision-safe shared staging, symbolic-link rejection, and rejecting oversized files/folders without committing partial inbox entries. Host tests additionally exercise manual address validation and the no-broadcast transport. All seven iOS runtime tests passed on the local iOS 27.2 test runtime; both SDK builds compiled. Passing these checks is distinct from physical-device and visual app testing. Privacy manifests for app/extension file access and app settings/disk-space checks are included; distribution review still needs to assess the final linked dependencies.

Before distribution, test two physical devices or a phone and desktop: Local Network consent/denial, Wi-Fi discovery with the approved entitlement, manual receive/send in both directions, remembered identities across restarts, provider downloads, Photos sharing, Share-extension App Group access, low space, interruption, lock/background behavior and iPad layouts. Export compliance, App Store metadata, distribution signing and delivery are not configured by the development build.


Nearby computers are grouped by installation identity. When one computer advertises several network interfaces, its row lets you choose the exact address. The selected advertised endpoint is checked again when sending, and its HELLO identity must still match; grouping does not silently turn discovery-based sends into unchecked manual connections.

If iOS boot output reports `Data Migration Failed` but installation and launch succeed, the launcher reports those outcomes separately. It does not erase the device or reinstall from scratch. A boot command failure stops before installation; abnormal Files/settings behavior should be investigated in Device Hub rather than automatically resetting data.
