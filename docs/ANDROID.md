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

Add `--test` to run debug-only native runtime checks before opening the app. These create their own private fixtures and verify TLS, Unicode file delivery, CRC32 readback and received-file rename. They do not establish physical-device networking or provider compatibility.

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
- **Files:** Android's standard document picker for regular files, including enabled cloud document providers. Selected files are copied into private staging on a worker thread; provider originals stay unchanged. Choose a client and review filenames, sizes and destination before explicitly sending. Android share-sheet imports use the same staging and review flow.
- **Received:** browse folders, search the current folder, preview with an installed viewer, share, save a copy through Android Files, safely rename, or confirm permanent deletion. Existing incoming files use safe renamed destinations. Preview/share grants read access only to the selected received file.
- **Settings:** Mocha by default, the shared palettes or system appearance, encryption mode, verification-code pairing and session diagnostics/history.

Android 17 requires local network permission. The app requests it before listening or discovering; denying it leaves local file selection available. No account, broad storage permission or location permission is requested. Cloud locations appear only when their provider apps are installed and enabled.

Keep BitSkim foregrounded. Leaving it stops transfers and closes its listener; receiving is paused while picking files or reviewing an outgoing transfer. File operations and network servicing run away from the UI thread. The client supports incoming collections, but outgoing selection currently accepts regular files only, up to 2 GiB minus one byte per file and 32 files per picker operation. No background receiving, folder picker, transfer resume or production signing is provided yet. Prefer encryption can fall back to plaintext with older clients; compare codes on both devices before trusting.

## Validation remaining

Physical Android devices, real Wi-Fi broadcast behavior, cloud-provider downloads/uploads, share-sheet imports, accessibility and Android versions below 17 require further runtime checks. An arm64 Android 17 development runtime has passed the native checks; this does not establish all UI workflows or all network paths. See [VALIDATION.md](VALIDATION.md).
