# BitSkim 0.2

Direct local-network file sharing between modern desktops and classic computers, with native clients and no account or central server.

## Features

- Absolute negotiation and file-list deadlines stop partial-frame trickle attacks without shortening recipient approval or active transfer verification.
- Android provider staging preserves an available original file modification date, including files inside selected folders.
- macOS pairing keys use Keychain; modern Windows keys use user-scoped DPAPI. Legacy plaintext settings migrate only after verified storage, and storage errors fail closed.
- Stable typed error codes and an explicit written-file verification API; remote error wording cannot choose local recovery actions.
- Negotiated collection connections reuse the same socket for successive files, retain per-file readback verification, and fall back to separate connections for older clients.
- Explicit recovery of interrupted folders from a trusted encrypted peer: re-approve the manifest, verify completed files and partial prefixes, preserve mismatches and unlisted files, then receive remaining data.
- Experimental Windows 3.x development client with native Win16/Winsock 1.1 build and import checks. Classic Mac OS 8/9 compiler/SDK preparation is documented; its network client is not ready.
- Opt-in file modification-date preservation, negotiated per transfer with content-bound records, verified temporary-file restoration and explicit success/failure receipts; disabled and older peers retain the original transfer protocol.
- Contextual recovery guidance for transfer errors, safe current-transfer retry actions and connection/folder shortcuts while retaining raw diagnostic errors.
- One maintained compatibility record generates the README, validation summary and public support table, distinguishing completed checks from unverified platforms.
- Nearby discovery through UDP broadcasts, or manual IPv4 connections.
- Files and folder collections on capable clients, approval prompts, progress, cancellation, session queues/history and conflict choices.
- Written-file CRC32 verification and compatibility/discovery diagnostics.
- Mac, modern Windows and Linux prefer TLS, support verification-code pairing and remembered device identities. Retro connections use plaintext.
- Shared color palettes and native platform integration. Experimental iOS and Android clients are included as source only.

## Installation

Choose the package for your platform from the tagged release. macOS uses the signed/notarized Apple silicon DMG or ZIP; modern Windows uses its x64 setup or portable ZIP. Windows packages are unsigned. Unpack the Amiga archive and open Install; the Amiga requires AmigaOS 3.x and a compatible TCP/IP stack. DOS, Atari, RISC OS and Windows Classic packages have limited validation; consult their platform guides before installation.

Linux desktop+CLI tar.gz archives target arm64/x86-64 with distribution-provided GTK 4.12+, GLib/GIO and OpenSSL 3; see docs/LINUX.md for installation and limits.

Open BitSkim on both devices on a trusted local network, allow incoming connections and approve transfers on the receiving device. Individual files are limited to 2 GiB minus one byte. Opt-in retro metadata is preserved by supported clients. Mobile clients require development builds and are not public binary downloads. See docs/INSTALLATION.md, docs/BUILDING.md and docs/VALIDATION.md in the source for requirements and validation limits.

## Source and Licence

- Website: https://bitskim.retrocs.org
- Source: https://github.com/retrocs-org/bitskim
- Tagged release: https://github.com/retrocs-org/bitskim/releases/tag/0.2
- Licence: https://github.com/retrocs-org/bitskim/blob/0.2/LICENSE.md

BitSkim-owned code is MIT licensed. Third-party components retain their own terms. DOS binaries link GPL-licensed mTCP; RISC OS and Atari packages include their required corresponding-source/relink material and notices. See THIRD-PARTY-NOTICES.MD.

## Version Changelog

### 0.2

#### Features

- Absolute negotiation and file-list deadlines stop partial-frame trickle attacks without shortening recipient approval or active transfer verification.
- Android provider staging preserves an available original file modification date, including files inside selected folders.
- macOS pairing keys use Keychain; modern Windows keys use user-scoped DPAPI. Legacy plaintext settings migrate only after verified storage, and storage errors fail closed.
- Stable typed error codes and an explicit written-file verification API; remote error wording cannot choose local recovery actions.
- Negotiated collection connections reuse the same socket for successive files, retain per-file readback verification, and fall back to separate connections for older clients.
- Explicit recovery of interrupted folders from a trusted encrypted peer: re-approve the manifest, verify completed files and partial prefixes, preserve mismatches and unlisted files, then receive remaining data.
- Experimental Windows 3.x development client with native Win16/Winsock 1.1 build and import checks. Classic Mac OS 8/9 compiler/SDK preparation is documented; its network client is not ready.
- Opt-in file modification-date preservation, negotiated per transfer with content-bound records, verified temporary-file restoration and explicit success/failure receipts; disabled and older peers retain the original transfer protocol.
- Contextual recovery guidance for transfer errors, safe current-transfer retry actions and connection/folder shortcuts while retaining raw diagnostic errors.
- One maintained compatibility record generates the README, validation summary and public support table, distinguishing completed checks from unverified platforms.
- iOS and Android native folder selection with read-only provider staging, nested/empty directory preservation, full content review and explicit send confirmation.
- Linux desktop completion/failure notifications with grouped results, verified-receipt counts, Open folder and a saved per-profile notification preference.
- Linux GTK desktop and separate headless CLI for arm64/x86-64, with discovery, file/folder approval, TLS pairing, disk verification, saved queues/history, shared themes, diagnostics and QR invitations.
- Saved outgoing queues and eight recent results; restored work stays paused until explicit Continue, with mobile staging retained across restart.
- Android approved active transfers continue while minimized through a visible foreground notification with Cancel. Idle receiving still needs the app open.
- Five-minute QR connection invitations on modern desktop/mobile clients, mobile scanning, TLS fingerprint/device checks and receiver-validated tokens before file offers.
- Opt-in Amiga attributes/comments and RISC OS filetypes, native restoration, content-bound companions for onward transfers and adjacent .info inclusion in queued collections.
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

- About/Settings and diagnostics show a short source build fingerprint, stable across checkout paths and available without Git metadata.
- Folder recovery distinguishes checking existing content from transferring remaining data, with verified/resumed/received/remaining counts and checked-byte progress.
- Explicit retained-partial review shows recognized files and allocated space, requires confirmation, blocks concurrent transfers and preserves completed, changed, locked, linked and unrecognized files.
- Mobile send reviews keep Send/Cancel visible while folder contents scroll, wrap full paths, support large text and provide explicit accessibility labels and keyboard cancellation.
- Isolated native Mac view/startup checks and Windows installer wizard acceptance tools support review of packaged applications without changing existing user profiles.
- Mobile folder reviews use bounded manifests and background hashing; changed content requires a new review, and provider copy failures leave originals untouched.
- Standalone Linux build/install commands, checked desktop/CLI tar.gz packages and optional coordinated Linux builds; public download links appear automatically when Linux assets are released.
- Local builds refresh an unpacked BitSkim installer drawer in the shared builds directory, with matching classic drawer/installer icons and archives kept in its packages subdirectory.
- Release automation enforces complete version notes, identical annotated tags and checked platform assets; publishes normal/latest repository releases; supports separate output directories, redacted logs and stable-name Aminet submissions from retained published archives.
- Android build/run/bootstrap commands keep downloaded tools, SDK/NDK, Gradle and custom OpenSSL builds in the external shared tools directory. Physical-device and cloud-provider validation remain pending.
- iOS selects regular files through copy-based Apple Files import, preserving provider originals.
- iOS Files-provider import guidance and explicit Save to Files for received files/folders, preserving originals and pausing incoming offers while browsing.
- Mac groups multi-interface devices into one card and menu entry, asks for the selected address, retains identity checks and uses iPhone/iPad device icons.
- iOS groups multi-interface discoveries into one computer with explicit, identity-checked address selection; boot migration messages are distinguished from successful app launches without resetting device data.
- Clearer verification results distinguishing successful readback, unconfirmed receiver verification and incomplete transfers.

#### Bugs

- Fully verified existing files now advance folder recovery exactly once, including jobs containing only completed files.
- Corrected outdated batch-resume and website-launch wording in apps, guides and public website copy.
- Queued preconnection failures receive a fresh failed receipt, preserving history identity and allowing result notifications instead of reusing the previous successful transfer.
- Amiga LHA packaging normalizes level-0 path separators and header checksums for consistent nested extraction across LHa and OLHa readers.
- Mac disk images preserve the signed app metadata and validate the mounted/copied app; HFS hybrid metadata no longer invalidates the installed bundle signature.
- Full 0.2 releases require all intended Linux desktop/CLI archives for both architectures; missing packages are named and cannot silently reduce the publication inventory.
- Release inventory and iOS folder/background documentation now match the current application behavior.
- Mobile staging cleanup waits for the whole queue instead of deleting folder children after the first completed item; restored queues retain their owned roots.
- Android status reports bound queued path previews so large folder queues cannot overflow the display report.
- Aminet preparation honors the configured LHA extraction reader for archive verification.
- Avoid repeated empty-offer notifications while idle, keeping iOS settings responsive.
- iOS receiving addresses keep port numbers ungrouped in all locales; local launch supports Device Hub and older Simulator app names.
- Windows loads saved device identities at startup so remembered pairing survives restarting.
- Mac incoming approvals appear on the main window after dismissing Transfer tools or connection sheets.
- Windows installer blocks upgrades while either 0.1 or 0.2 is running.
- Windows Transfer tools follows the selected theme, retains report scroll position and selection during live updates, and avoids repainting unchanged labels.
- Windows receivers no longer ask for a second confirmation when destination free space is known to be sufficient; insufficient and unknown-space handling remains explicit.
- Windows startup and Transfer tools layout fitted to high-DPI desktop work areas; scaled controls and reachable footer actions.
