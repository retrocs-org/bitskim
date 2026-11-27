# iPhone and iPad — experimental 0.2

[Website](https://bitskim.retrocs.org) · [Source](https://github.com/retrocs-org/bitskim)

This is a source-only iOS client in development, targeting iOS/iPadOS 16 or later on arm64. There is no App Store, TestFlight or publicly signed iOS download yet. It uses the existing C transfer engine and pinned OpenSSL 3.5.9, with a SwiftUI interface and a native Share extension. Physical-device UI, permission, Files-provider and Share-extension testing remain required before distribution.

## What it does

- Send files or provider folders, including nested files and empty directories, after reviewing the complete selection. Up to 32 roots and 32,760 entries can be reviewed; individual files are limited to 2 GiB minus one byte. Selected content is copied into private staging before queueing.
- Receive files and complete folder manifests, including empty directories, into `Received` in the app's Documents directory. Existing files are preserved with collision-safe names.
- Browse received folders, copy files or folders with **Save to Files**, and use system sharing for files. The Documents directory is also available through Files.
- Prefer or require TLS, compare verification codes, remember device identities, show diagnostics/history and CRC32 readback results, retry queued sends and cancel transfers.
- Show the Wi-Fi/LAN address for manual incoming connections.
- Accept files/photos from other apps through the Share extension. The extension copies them to an App Group inbox. Open BitSkim, select a client or enter an address, then review the files and confirm Send. The extension does not silently open the containing app or send to a guessed recipient.

Pending shared-inbox selections stay on the device until sent or discarded. Once queued, a staged folder remains available until every queued child is finished or explicitly cleared. Failed or cancelled import copies are cleaned without deleting the originals. An interrupted batch can leave already completed received files; explicit cancellation removes the current partial download. Temporary staging requires enough local storage for a copy of each selected file.

Keep BitSkim open to receive. On entering the background, it checkpoints the outgoing queue and recent history, stops network servicing and closes its listeners. Pending and queued private copies and completed received files remain. Opening it reconnects with restored outgoing work paused; choose Retry explicitly to continue. An interrupted transfer may need to restart its current file. Continuous background receiving and guaranteed completion while locked are not supported. There is no automatic acceptance of incoming files. The mobile UI uses verification-code device pairing; it does not yet expose shared-group PSK key entry.

## Review and send

Select documents in **Files**, then choose a recipient in **Clients**. Selecting a client first also opens the standard file picker when no files are ready. Both routes open **Review transfer**, showing the recipient name/address, filenames, individual sizes, total size and encryption mode. Send and Cancel stay in the navigation bar while the contents scroll, including with the largest accessibility text sizes. Full relative paths wrap, and VoiceOver announces file/folder type, path and size. Escape cancels; Command-S sends. Only the explicit **Send** button starts networking. Cancelling the review leaves the staged selections available for another recipient; it does not send or delete the originals. Staged file contents and folder structure are checked again at confirmation, and changed selections require another review.

During a transfer, **Clients → Transfer** shows the current filename, item position, transferred bytes, recent speed and an estimated time remaining once data is moving. Reviewed outgoing batches also show an overall progress bar and total bytes. Incoming collections use the engine's overall summary; their time estimate is for the current item. Estimates can change with network speed and do not replace the final written-file verification result. Paused queues can be retried or cancelled explicitly.

## Manage received files

Open **Received** to browse folders and search filenames in the current folder. Tap a regular file to preview it using Quick Look, then **Done** to return. Unsupported preview formats remain available to share or export.

An item's **…** menu offers **Preview**, **Share**, **Save to Files**, **Rename** and **Delete**, as applicable. Rename includes the extension, rejects invalid names and never overwrites another file. Delete requires confirmation and permanently removes selected files or selected folders with their contents; it cannot be undone.

Use **Select** to choose several items, optionally **Select all** for the visible search results, then **Selected file actions** to share, export or delete them together. File actions are blocked during transfers, and incoming offers pause while a preview, share sheet or document picker is open. Operations are scoped to BitSkim's Received folder, reject symbolic-link escapes and preserve unrelated files.

## Appearance and diagnostics

The tabs are **Clients**, **Files**, **Received** and **Settings**. Open **Settings → Appearance → Theme** to choose Follow system or any of the eight desktop palettes from a dedicated selection page. Catppuccin Mocha is the default on new installations; existing saved choices are retained. Changes apply immediately to BitSkim screens, including received folders and incoming approvals, and are saved on this device. System document pickers retain Apple's standard interface.

**Settings → Diagnostics** contains pairing controls, recent transfer verification results and the connection report. Incoming approvals also retain their pairing controls.

## Files and cloud storage

BitSkim uses Apple's Files picker, so you can select documents from iCloud Drive and enabled providers such as Dropbox and OneDrive. Install the provider's app, sign in, then open **Files → Browse → … → Edit** and enable its location. See [Apple's setup guide](https://support.apple.com/en-ie/102238).

To select files before choosing a recipient, open the **Files** tab to launch Apple's standard file picker, then browse to a local or enabled cloud location, and select your documents. BitSkim stages them in its local inbox; open **Clients** and choose a recipient to send. Pending selections survive reopening the app. You can also select a client (or enter its address) first and choose **Choose from Files**. Files supplies copies of selected regular files; BitSkim stages them locally before sending, preserving the originals. Use **Files → Choose Folder** to open Apple's standard folder picker. When the provider supports folder access, BitSkim reads the selected tree through that grant, copies it locally and preserves nested/empty directories. No provider files are renamed or deleted. Open **Clients**, choose the destination and review every relative path, file size and total before **Send selected items**. Preparation and the final content check run away from the UI thread. Cancelling review sends nothing.

A review accepts up to 32 selected roots and 32,760 files/directories combined, including the top-level folders. Each file is at most 2 GiB minus one byte; relative paths use at most 255 UTF-8 bytes including a directory's trailing slash. Trees are bounded to 16 path components, with directories below level 16. Links, aliases, special files, unreadable entries and unsupported names stop preparation. If a provider does not offer a readable folder grant, download a local copy or select individual files. Pending copies and paused queued children survive restart; cleanup waits until all queued items finish or are explicitly cleared.

To export received content, open **Received**, tap the item's **…** menu and choose **Save to Files**. Choose a local or enabled cloud location. Files and folders are exported as copies; cancelling leaves the received original untouched. The importer lets Apple manage its initial location and navigation instead of forcing BitSkim’s Received folder. Use **Browse Files** to reopen it after dismissal. BitSkim pauses new incoming offers while its import/export picker is open. Finish or cancel an active transfer before opening Files.

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

The suite exercises the real iOS C engine and filesystem: automatic TLS, identity-checked advertised endpoint selection, a Unicode binary payload with verified receipt, an empty file, nested folders/empty directories, cancellation after data starts, collision-safe shared staging, symbolic-link rejection, and rejecting oversized files/folders without committing partial inbox entries. Host tests additionally exercise manual address validation and the no-broadcast transport. The 24-test iOS suite passed on the local iOS 27.2 test runtime, including provider-style folder staging, content/structure change detection, queued staging ownership and a reviewed TLS folder send; both SDK builds compiled. Passing these checks is distinct from physical-device and visual app testing. Privacy manifests for app/extension file access and app settings/disk-space checks are included; distribution review still needs to assess the final linked dependencies.

Before distribution, test two physical devices or a phone and desktop: Local Network consent/denial, Wi-Fi discovery with the approved entitlement, manual receive/send in both directions, remembered identities across restarts, provider downloads, Photos sharing, Share-extension App Group access, low space, interruption, lock/background behavior and iPad layouts. Export compliance, App Store metadata, distribution signing and delivery are not configured by the development build.


Nearby computers are grouped by installation identity. When one computer advertises several network interfaces, its row lets you choose the exact address. The selected advertised endpoint is checked again when sending, and its HELLO identity must still match; grouping does not silently turn discovery-based sends into unchecked manual connections.

If iOS boot output reports `Data Migration Failed` but installation and launch succeed, the launcher reports those outcomes separately. It does not erase the device or reinstall from scratch. A boot command failure stops before installation; abnormal Files/settings behavior should be investigated in Device Hub rather than automatically resetting data.

## Local Files navigation check

The installed iOS 27.2 development runtime currently reproduces an app-container navigation loop in both BitSkim’s standard picker and Apple’s standalone Files app, including on a separate clean test device. Files logs report a missing cached domain and inability to resolve the local container. Restarting the Files service and device did not repair it. This remains an unresolved runtime validation issue; the app folder is present on disk. Do not erase app data to work around it. Validate local-folder and authenticated cloud-provider navigation on a physical iPhone or another healthy runtime before distribution.

## Saved work, QR connections and metadata

Queues and eight recent results persist privately. Restarted outgoing work stays paused until Continue; staged files are retained. iOS can suspend or stop the app in the background, so keep it open for active transfers. Settings → Clear saved transfer history leaves files and queued work intact. Clients → Scan connection QR opens the camera scanner, with a paste field if no camera is available. Show my connection QR displays a five-minute invitation for a selected LAN address. File selection and review remain explicit; QR does not remember trust automatically or approve incoming files. Both devices need certificate encryption. Optional retro metadata in Settings retains .bskmeta companions for forwarding to Amiga/RISC OS. See [TRANSFERS.md](TRANSFERS.md) for limits. Camera/provider/signing behavior still requires physical-device review.


## Review accessibility checks

After project generation, run the isolated UI review against an installed iOS test device:

```sh
xcodebuild -project build/ios/BitSkim.xcodeproj -scheme BitSkimReviewUITests \
  -sdk iphonesimulator -destination 'id=YOUR_DEVICE_ID' \
  -derivedDataPath build/ios-accessibility CODE_SIGNING_ALLOWED=YES test
```

The debug-only harness renders the production review with synthetic long paths and a long collection. It starts no listener and reads/sends no files. Tests cover the largest Dynamic Type size, Mocha/Latte, pinned actions after scrolling, disabled Send while checking and cancellation. They retain screenshots in the test result. These checks do not replace physical-device VoiceOver and provider testing.

Updated trusted encrypted peers support an explicit **Resume existing folder** choice on incoming folder offers. It verifies completed content and partial prefixes before continuing; normal acceptance still uses a separate safe root. Retained checkpoint files are hidden in Received. Connection loss retains the current checkpoint; deliberate cancellation removes it.

## Build identity and retained partial files

About/Settings and Diagnostics display the source build fingerprint alongside version 0.2. It identifies the source contents, including uncommitted edits, independently of checkout path or Git history. Direct compiler invocations that omit the generated identity header are explicitly labeled as unidentified. Regenerate builds with the supplied scripts after source changes. Different target configurations may share a source ID; it is not a binary checksum.

Open **Retained partial files** from Settings/Preferences or Linux Transfer tools. Desktop clients let you choose a receive folder; mobile clients review Received. The bounded scan lists recognized checkpoint pairs and their disk usage. Review every listed entry, then confirm **Remove reviewed partials** to discard that resume progress. Completed files, unrecognized files, links and changed/locked checkpoints are kept. Older unmarked checkpoints and volumes that cannot store the local marker are excluded from cleanup; they remain usable for resume. Transfers or queued work prevent cleanup; any intervening transfer requires another scan. Closing the review stops further cleanup. A limit notice means only the listed files were reviewed; other files are kept.

During folder recovery, progress distinguishes checking existing content from transferring remaining data, with counts for verified, resumed, received and remaining files. Hashing bytes are shown as checked bytes, not network payload.
