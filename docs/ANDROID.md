# Android client — experimental 0.2

The Android client uses BitSkim's shared transfer engine with a native Java interface. It is a development client, with no public Android release yet. A 64-bit Android 8.0/API 26 or later device is required; arm64 is the default build. No 32-bit Android target is provided.

## Build on Apple silicon macOS

Install Xcode command-line tools, Python 3.9+, Perl, make and JDK 21. The JDK is available through Homebrew:

```sh
brew install openjdk@21
python3 scripts/bootstrap-android.py --install-sdk
./scripts/build-android.sh
```

Run from the source root. The bootstrap installs Google's official Android tools/SDK/NDK and a pinned Gradle distribution. The first build compiles pinned OpenSSL 3.5.9. Downloads and custom-built dependencies live in `~/Development/tools/android`, outside Git; no repository credentials are needed. The current Android CLI replaces the deprecated `sdkmanager --licenses` workflow.

Output: `build/android/BitSkim-0.2-Android-debug.apk`. The app is signed with an external development key at `~/Development/tools/android/debug.keystore`. This is a development APK, not a production-signed store release. Keep that key to update existing local installations. Build outputs stay inside the checkout's ignored `build/android` directory.

The build checks version metadata, local network permission, required resources, APK signature, native architecture and 16 KiB page alignment. Use `BITSKIM_ANDROID_TOOLS` to choose another shared tools directory; `ANDROID_SDK_ROOT` and `JAVA_HOME` can select existing external installations. Automatic SDK bootstrap currently supports Apple silicon macOS. Other hosts need externally installed matching SDK/NDK, Gradle and JDK; they have not been validated.

## Install and run locally

Enable developer options and USB debugging on a physical Android device, connect it and approve its debugging prompt:

```sh
./scripts/build-android.sh --list-devices
./scripts/build-android.sh --run --device DEVICE_SERIAL
```

For a local Android test device:

```sh
python3 scripts/bootstrap-android.py --runtime
./scripts/build-android.sh --run --avd BitSkim_API_37
```

The run script creates only the named BitSkim test device, if missing, and retains app data. Its SDK image, runtime, device disk, Gradle cache and downloaded tools are stored in the shared tools directory. It does not reset or delete existing devices. The first start can take a few minutes.

To reinstall/start an existing build without recompiling:

```sh
python3 scripts/run-android.py --device DEVICE_SERIAL
```

Add `--test` to run debug-only native runtime checks before opening the app. These create private fixtures and verify review layout with 200% text, light/dark themes and keyboard cancellation, plus TLS, Unicode file delivery, CRC32 readback, received-file rename, document-tree access through a debug provider, empty directories, a 96-file folder send and safe staging cleanup. They do not establish physical-device networking or provider compatibility.

```sh
./scripts/build-android.sh --run --avd BitSkim_API_37 --test
```

The local test network may not deliver LAN broadcasts. For desktop-to-Android testing the runner forwards `127.0.0.1:46442` to Android's TCP 46440, unless that host port is already forwarded elsewhere. From Android, enter the Mac's reachable IPv4 address manually. Physical devices should use the same trusted LAN; USB debugging alone does not make the computer's LAN reachable.

## Build options

```sh
./scripts/build-android.sh --discovery off
./scripts/build-android.sh --abis arm64-v8a,x86_64
```

Discovery is enabled by default; `--discovery off` disables broadcast transport at compilation and removes discovery actions from the UI. Manual IPv4 connections remain available. x86_64 is an optional target, not yet runtime-validated. The default arm64 build is the validated development path.

## Using the client

- **Clients:** nearby clients grouped by installation, explicit address selection, manual IPv4, current-file progress, estimated time, outgoing batch totals, cancel and retry.
- **Files:** Android's standard document picker for files, and **Choose Folder** for a read-only document-tree grant, including enabled providers that support folder access. Selected files are copied into private staging on a worker thread; provider originals stay unchanged. Choose a client and review all relative paths, file sizes, the total and destination before explicitly sending. Send/Cancel stay outside the scrolling list; path rows wrap and identify files/folders to accessibility services. Folder trees include nested files and empty directories. Android share-sheet imports use the same staging and review flow.
- **Received:** browse folders, search the current folder, preview with an installed viewer, share, save a copy through Android Files, safely rename, or confirm permanent deletion. Existing incoming files use safe renamed destinations. Preview/share grants read access only to the selected received file.
- **Settings:** Mocha by default, the shared palettes or system appearance, encryption mode, verification-code pairing, saved queues/history, QR invitations and diagnostics.

Android 17 requires local network permission. The app requests it before listening or discovering; denying it leaves local file selection available. No account, broad storage permission or location permission is requested. Cloud locations appear only when their provider apps are installed and enabled.

Keep BitSkim foregrounded to receive a new offer. With “Continue active transfers while minimized” enabled and notification permission granted, an already approved active transfer continues in a connected-device foreground service. Its visible notification shows progress and Cancel; it stops after completion/cancellation. Denying permission or disabling the option requires keeping the app open. Idle/pending-approval receiving stops in the background; receiving is also paused while picking files or reviewing an outgoing transfer. File operations and network servicing run away from the UI thread. Incoming and outgoing collections are supported. Up to 32 roots can be reviewed, with 32,760 entries combined and 2 GiB minus one byte per file. Portable relative paths are limited to 255 UTF-8 bytes including directory slashes, with at most 16 components and directories below level 16. Links, special files, provider loops, duplicate names, virtual documents and incomplete/denied reads are rejected. Every staged file is hashed for review and checked again before queueing; a changed tree requires a new review. No always-on background receiving or production signing is provided yet. Trusted encrypted peers can explicitly resume an interrupted folder: completed files and retained partial prefixes are verified before continuing. Prefer encryption can fall back to plaintext with older clients; compare codes on both devices before trusting.

## Validation remaining

Physical Android devices, real Wi-Fi broadcast behavior, cloud-provider downloads/uploads, share-sheet imports, accessibility and Android versions below 17 require further runtime checks. An arm64 Android 17 development runtime has passed the native checks; this does not establish all UI workflows or all network paths. See [VALIDATION.md](VALIDATION.md).

## Saved work and connection QR

Queues and the last eight results survive restart. Outgoing work restores paused and needs explicit Retry; it never sends on launch. Selected private copies remain until sent or discarded. Folder staging is retained for every queued child, including after restart, and removed only when the whole queue is idle and empty. Provider originals are never modified. Cloud apps may restrict document-tree grants; download a local copy or select individual files when folder access is unavailable. Settings can clear saved history. Clients → Scan connection QR checks the receiving device’s ID and certificate fingerprint before the normal file review. Show my connection QR lets another phone send here; invitations expire after five minutes. Enable optional retro metadata in Settings on both devices to retain content-bound .bskmeta companions. See [TRANSFERS.md](TRANSFERS.md) for formats and limits.

The debug-only native background check requires Local Network and notification permission in a dedicated test install with no queued work. Run `adb shell am instrument -w org.bitskim.android/.BackgroundChecks`; it creates a private rate-limited fixture, minimizes the actual activity, verifies progress and notification Cancel, and removes the fixture. Device-specific power restrictions still need physical-device validation.

Provider imports preserve an available original regular-file modification date after copying into private staging; when a provider omits the date, its original timestamp cannot be recovered and the local staging date is used. This includes files in selected trees. End-to-end date preservation still requires the sender's metadata option and a compatible receiver. Authenticated cloud-provider/device validation remains pending. Updated encrypted trusted peers can explicitly recover existing received folders; incomplete checkpoint files are hidden from the received-file list.

## Build identity and retained partial files

About/Settings and Diagnostics display the source build fingerprint alongside version 0.2. It identifies the source contents, including uncommitted edits, independently of checkout path or Git history. Direct compiler invocations that omit the generated identity header are explicitly labeled as unidentified. Regenerate builds with the supplied scripts after source changes. Different target configurations may share a source ID; it is not a binary checksum.

Open **Retained partial files** from Settings/Preferences or Linux Transfer tools. Desktop clients let you choose a receive folder; mobile clients review Received. The bounded scan lists recognized checkpoint pairs and their disk usage. Review every listed entry, then confirm **Remove reviewed partials** to discard that resume progress. Completed files, unrecognized files, links and changed/locked checkpoints are kept. Older unmarked checkpoints and volumes that cannot store the local marker are excluded from cleanup; they remain usable for resume. Transfers or queued work prevent cleanup; any intervening transfer requires another scan. Closing the review stops further cleanup. A limit notice means only the listed files were reviewed; other files are kept.

During folder recovery, progress distinguishes checking existing content from transferring remaining data, with counts for verified, resumed, received and remaining files. Hashing bytes are shown as checked bytes, not network payload.
