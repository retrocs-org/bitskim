# macOS build — BitSkim 0.2

## Target and host

The current target is **arm64 Apple silicon, macOS 13 or later**. Use an Apple silicon Mac with Xcode command-line tools (or Xcode), an installed macOS SDK, `clang`, `swiftc`, `codesign` and Python 3 on PATH. The script uses Clang's host architecture for the C objects and explicitly targets arm64 for Swift: an Intel-host build is not supported by this script as written. There is no Intel/universal binary target.

From the repository root:

```sh
./scripts/build-macos.sh
# equivalent: make macos
```

The script builds the shared C engine with a macOS 13 deployment minimum, compiles the SwiftUI/AppKit application in Swift 5 language mode for arm64, copies Info.plist and committed icons, then signs the bundle. Developer builds default to ad-hoc signing; set BITSKIM_SIGN_IDENTITY for Developer ID signing with hardened runtime and a trusted timestamp.

Output: `build/macos/BitSkim.app`. Swift/Clang module caches and object files remain under build/. No external Swift package is fetched. Generated icon files are committed, so normal builds need no Pillow.

```sh
codesign --verify --deep --strict build/macos/BitSkim.app
open build/macos/BitSkim.app
```

Opening the application starts its local discovery/transfer listener. Quit a running older copy before testing a new bundle. Local-network consent and firewall behavior require native runtime testing.

## DMG

Build the application first. The DMG script additionally needs macOS `hdiutil`, `xattr` and Python's **ds_store** module (package name **ds-store**). Use a local virtual environment if needed:

```sh
python3 -m venv work/dmg-venv
work/dmg-venv/bin/python -m pip install ds-store
work/dmg-venv/bin/python scripts/build-macos-dmg.py
```

Or run `python3 scripts/build-macos-dmg.py` with an interpreter where ds_store is already installed. The image layout comes from installers/macos/settings.py; it is not a dmgbuild configuration requiring the dmgbuild package.

Output: `outputs/BitSkim-0.2-macOS-arm64.dmg`. It contains BitSkim.app, an Applications shortcut and local documentation/licenses. The script creates the HFS image without mounting it, compresses it to UDZO and runs hdiutil verification. It never copies the app into /Applications.

For a portable ZIP, build all three primary clients and run `python3 scripts/package.py`; see [BUILDING.md](BUILDING.md) for its optional-target/provenance requirements.

## Signing and validation

Developer builds default to local ad-hoc signing. Public release builds use BITSKIM_SIGN_IDENTITY and scripts/notarize-macos.py with a saved keychain profile to obtain Apple acceptance and staple the app and DMG tickets. Successful codesign/hdiutil checks do not establish Gatekeeper behavior on another Mac or confirm Finder appearance and drag-copy operation. Recorded native app checks and remaining DMG checks are in [VALIDATION.md](VALIDATION.md). The working toolchain recorded for this project used Xcode 27.2; that observation is not an assertion that older/newer toolchains have been tested.

## Transfer extensions

The build statically links pinned OpenSSL 3.5.9, built for arm64/macOS 13 by `scripts/build-openssl.sh macos` on first use. This needs Perl, make, curl and initial network access. Pairing/encryption and retained-checkpoint settings are in Preferences. Select or drop multiple files; paused queues expose Retry, Skip and Clear. See [TRANSFERS.md](TRANSFERS.md).

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls and Atari its existing queue page, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

New configurations default to Prefer encryption. Disable always uses plaintext; Prefer negotiates automatic TLS over discovery or manual IP when possible; Force rejects plaintext. A blank optional pairing key enables unpaired TLS, which does not verify peer identity. Matching optional keys authenticate trusted group membership. See TRANSFERS.md.
