# BitSkim 0.2

Direct local-network file sharing between modern desktops and classic computers, with native clients and no account or central server.

## Features

- Nearby discovery through UDP broadcasts, or manual IPv4 connections.
- Files and folder collections on capable clients, approval prompts, progress, cancellation, session queues/history and conflict choices.
- Written-file CRC32 verification and compatibility/discovery diagnostics.
- Mac and modern Windows prefer TLS, support verification-code pairing and remembered device identities. Retro connections use plaintext.
- Shared color palettes and native platform integration. Experimental iOS and Android clients are included as source only.

## Installation

Choose the package for your platform from the tagged release. macOS uses the signed/notarized Apple silicon DMG or ZIP; modern Windows uses its x64 setup or portable ZIP. Windows packages are unsigned. Unpack the Amiga archive and open Install-BitSkim; the Amiga requires AmigaOS 3.x and a compatible TCP/IP stack. DOS, Atari, RISC OS and Windows Classic packages have limited validation; consult their platform guides before installation.

Open BitSkim on both devices on a trusted local network, allow incoming connections and approve transfers on the receiving device. Individual files are limited to 2 GiB minus one byte. Filesystem metadata is not preserved. Mobile clients require development builds and are not public binary downloads. See docs/INSTALLATION.md, docs/BUILDING.md and docs/VALIDATION.md in the source for requirements and validation limits.

## Source and Licence

- Website: https://bitskim.retrocs.org
- Source: https://github.com/retrocs-org/bitskim
- Tagged release: https://github.com/retrocs-org/bitskim/releases/tag/0.2
- Licence: https://github.com/retrocs-org/bitskim/blob/0.2/LICENSE.md

BitSkim-owned code is MIT licensed. Third-party components retain their own terms. DOS binaries link GPL-licensed mTCP; RISC OS and Atari packages include their required corresponding-source/relink material and notices. See THIRD-PARTY-NOTICES.MD.

## Version Changelog

### 0.2

#### Features

- Experimental source-only Android client: native Clients/Files/Received/Settings tabs, standard document-provider selection, review before sending, foreground receiving, TLS/device trust, written-file verification, progress, session diagnostics and Mocha/shared themes.
- iOS review-before-send with recipient address, filenames, total size and explicit confirmation; changed files require another review.
- iOS current-file progress, recent speed, remaining-time estimates and overall outgoing batch progress.
- iOS received-file Quick Look previews, filename search, safe rename, confirmed deletion and multiple-item sharing/export.
- iOS persistent themes using the eight desktop palettes or system appearance; Settings with separate Theme and Diagnostics pages.
- iOS Clients and Files tabs let you browse enabled Files providers before choosing a recipient; opening Files launches Apple’s standard picker without forcing an app folder.
- iOS `--run`, device selection and device listing for local build/install/launch, with a separate test listening port so the Mac desktop client can run alongside it.
- iOS build switch `--multicast=off|on` keeps discovery, compiler flags and entitlements consistent; manual IP connections remain available when disabled.
- Experimental source-only iPhone/iPad client with native file import/receive, shared-inbox extension, manual IPv4, optional broadcast discovery, TLS/device trust and transfer history. Device signing and physical-device validation remain pending.
- Written-file CRC32 readback verification for incoming files from legacy peers, without changing their receipt protocol.
- Discovery activity counters, search state, last peer-packet age and network troubleshooting guidance.
- Consistent peer compatibility hints for folders, filename limits, receiving state, verification and encryption.

#### Enhancements

- Release automation enforces complete version notes, identical annotated tags and checked platform assets; publishes normal/latest repository releases; supports separate output directories, redacted logs and stable-name Aminet submissions from retained published archives.
- Android build/run/bootstrap commands keep downloaded tools, SDK/NDK, Gradle and custom OpenSSL builds in the external shared tools directory. Physical-device and cloud-provider validation remain pending.
- iOS selects regular files through copy-based Apple Files import, preserving provider originals.
- iOS Files-provider import guidance and explicit Save to Files for received files/folders, preserving originals and pausing incoming offers while browsing.
- Mac groups multi-interface devices into one card and menu entry, asks for the selected address, retains identity checks and uses iPhone/iPad device icons.
- iOS groups multi-interface discoveries into one computer with explicit, identity-checked address selection; boot migration messages are distinguished from successful app launches without resetting device data.
- Clearer verification results distinguishing successful readback, unconfirmed receiver verification and incomplete transfers.

#### Bugs

- Avoid repeated empty-offer notifications while idle, keeping iOS settings responsive.
- iOS receiving addresses keep port numbers ungrouped in all locales; local launch supports Device Hub and older Simulator app names.
- Windows loads saved device identities at startup so remembered pairing survives restarting.
- Mac incoming approvals appear on the main window after dismissing Transfer tools or connection sheets.
- Windows installer blocks upgrades while either 0.1 or 0.2 is running.
- Windows Transfer tools follows the selected theme, retains report scroll position and selection during live updates, and avoids repainting unchanged labels.
- Windows receivers no longer ask for a second confirmation when destination free space is known to be sufficient; insufficient and unknown-space handling remains explicit.
- Windows startup and Transfer tools layout fitted to high-DPI desktop work areas; scaled controls and reachable footer actions.
