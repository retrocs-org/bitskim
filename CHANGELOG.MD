# Changelog

## 0.2 — In development

- Mac groups multi-interface devices into one card and menu entry, asks for the selected address, retains identity checks and uses iPhone/iPad device icons.

- iOS groups multi-interface discoveries into one computer with explicit, identity-checked address selection; boot migration messages are distinguished from successful app launches without resetting device data.

- iOS receiving addresses keep port numbers ungrouped in all locales; local launch supports Device Hub and older Simulator app names.

- iOS `--run`, device selection and device listing for local build/install/launch, with a separate test listening port so the Mac desktop client can run alongside it.

- iOS build switch `--multicast=off|on` keeps discovery, compiler flags and entitlements consistent; manual IP connections remain available when disabled.

- Experimental source-only iPhone/iPad client with native file import/receive, shared-inbox extension, manual IPv4, optional broadcast discovery, TLS/device trust and transfer history. Device signing and physical-device validation remain pending.

- Windows loads saved device identities at startup so remembered pairing survives restarting.
- Mac incoming approvals appear on the main window after dismissing Transfer tools or connection sheets.
- Windows installer blocks upgrades while either 0.1 or 0.2 is running.

- Windows Transfer tools follows the selected theme, retains report scroll position and selection during live updates, and avoids repainting unchanged labels.

- Windows receivers no longer ask for a second confirmation when destination free space is known to be sufficient; insufficient and unknown-space handling remains explicit.

- Windows startup and Transfer tools layout fitted to high-DPI desktop work areas; scaled controls and reachable footer actions.

- Written-file CRC32 readback verification for incoming files from legacy peers, without changing their receipt protocol.
- Clearer verification results distinguishing successful readback, unconfirmed receiver verification and incomplete transfers.
- Discovery activity counters, search state, last peer-packet age and network troubleshooting guidance.
- Consistent peer compatibility hints for folders, filename limits, receiving state, verification and encryption.

## 0.1 — Initial release

- Direct file sharing between native macOS, Windows and AmigaOS clients, with experimental DOS, Atari TOS/MiNTNet, RISC OS and Windows Classic clients.
- Automatic discovery through local-network broadcasts and manual IPv4 connections.
- File and folder transfers, including nested and empty directories on capable clients.
- Incoming-transfer approval, destination selection, progress reporting and cancellation.
- Sequential transfer queues with pause-after-file, continue, retry, skip and clear controls.
- Rename, skip and replace conflict policies, destination filename adaptation and disk-space previews.
- Temporary receive files, CRC-32 integrity checks and negotiated written-file verification.
- TLS encryption, verification-code pairing and remembered device keys on modern macOS and Windows clients.
- Checked resume for retained interrupted single-file downloads on modern clients.
- Separate upload/download speed limits, local connection diagnostics and session transfer history.
- Shared color themes and preferences for identity, visibility, receiving and automatic acceptance.
- Mac menu-bar operation, Windows tray operation and Amiga Workbench iconification, with background transfers.
- Amiga preferences application, ARexx management/events and NovaDock integration.
- Native file-drop integration and file/folder selection where supported by the client.
- Platform installers and portable packages, plus coordinated build, signing, packaging and release-publication scripts.
