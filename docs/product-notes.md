# Requirements and product decisions

Recorded 1 November 2026.

## Requested scope

Build a local-network, peer-to-peer file sharing application with an AirDrop-like experience. First clients: AmigaOS 3.x Workbench and macOS on Apple silicon.

Later target platforms:

- MS-DOS.
- RISC OS.
- Atari TOS.
- Windows 3.1.
- Windows 95, 98 and ME.
- Windows 2000.
- Windows 10.
- Linux (mentioned in the overall goal; distribution/desktop targets undecided).

## Requested preferences

| Preference | Requested behavior |
| --- | --- |
| Client name | Editable; use the computer hostname by default |
| Automatic discovery / visibility | Make this client visible to other clients |
| Automatically accept files | Accept incoming offers without confirmation when enabled |
| Save location | Remember a download path, or ask where to save each time |
| Enable encryption | Future version, on platforms that support it |
| Force encryption | Future version; refuse transfers without encryption |

Automatically discover other clients on the network; broadcast was suggested. Also allow manual connection using an IP address. The user drops a file on a peer in the app; the recipient gets an accept/decline request, then chooses where to save. Initial transfers are unencrypted. Encryption may only be available on a subset of clients later.

## Recommended first-release behavior

These are proposals, not additional requirements already agreed with the user.

- Use an existing IPv4 LAN, without an account, server or Internet dependency. Ethernet and Wi-Fi can interoperate when the network permits it.
- Discover peers while the app is running. Background services and startup launch are later options.
- Provide both drag-and-drop and a Send File button/file requester. DOS should use file selection or a command line; it cannot promise a desktop drop interface.
- Initially send one ordinary file per offer. Defer directories, recursive transfer and metadata bundles.
- Show peer name, platform and IP address. Names may be duplicated and are not authenticated identities.
- Show incoming sender name/IP, filename, size, destination and clear unencrypted status.
- Default to confirmation for every offer. Allow explicit auto-accept to a configured folder. Never launch received files automatically.
- With auto-accept plus “ask where to save,” skip only the accept question; still ask for a destination. Fully unattended reception requires a configured, writable folder.
- Default to asking where to save. Remember a folder only when selected in preferences. If it becomes unavailable, prompt or fail clearly; do not silently choose another volume.
- Show progress, bytes transferred, cancellation and a useful completion/error state. Use speed/ETA only when enough data exists for a useful estimate.
- Never silently overwrite an existing file. Default to a unique local filename; manual replace can be added with explicit confirmation.
- Save into a temporary file and finalize only after the complete transfer passes its integrity check. Report success to the sender only after the receiver has finished writing and finalizing.

## Clarify discovery and receiving

Use distinct settings internally and preferably in the UI:

1. **Visible on network** controls announcements and responses to discovery probes.
2. **Allow incoming transfers** controls unsolicited incoming offers, including manual-IP offers.
3. **Automatically accept** controls whether allowed offers need confirmation.

Turning visibility off does not make a TCP listener unreachable. Make that distinction clear. A hidden client may still search for visible peers and initiate transfers. When incoming transfers are disabled, outbound transfers still work over their sender-initiated connection.

## Safety and interoperability improvements

- Bound filename/message lengths and pending offers; check disk capacity and file-size limits before accepting.
- Keep file bytes unchanged. No automatic newline, executable, archive or text-encoding conversion.
- Exchange UTF-8 display names; explicitly map or reject characters unavailable on the destination. Show any renamed destination before manual acceptance. Never use a lossy display string as peer identity.
- Transfer only a basename, never a sender-provided destination path. Reject path separators, traversal, device names and other platform-specific dangerous names, including Amiga volume syntax.
- Track a persistent installation ID separately from the user-visible name and current IP. It helps deduplication, but is not authentication.
- Provide a local status panel: IP/interface, discovery state, listening port, save directory, last error and firewall/permission guidance.
- Support manual IP plus optional port, and later saved peer bookmarks. An address alone cannot overcome firewall, NAT or Wi-Fi client isolation.
- Explain that plaintext offers/files can be observed or modified by others with network access. A checksum detects accidental corruption, not sender authenticity.

## Future work candidates

Queues, negotiated directory transfer and modern interrupted-transfer resume are implemented. Future work includes preservation of Amiga protection bits/comments, RISC OS file types and other platform metadata. Directory transport is specified in PROTOCOL.md; automatic archive unpacking remains out of scope. Amiga .info icons are separate files and should be offered explicitly or inside a folder collection.

Later consider receive-only mode, launch-at-startup, optional modern discovery via mDNS, IPv6 on capable clients, accessibility, localization and transfer history without retaining file contents.

Authenticated pairing and encrypted transports should precede “trusted devices auto-accept.” A name, IP, installation ID or plaintext pairing token cannot establish that trust securely.

## Open decisions before implementation

- Product name and license.
- Minimum Amiga CPU, RAM, OS revision and supported TCP/IP stack; provisional engineering target: 68000-compatible OS 3.1 build, validated first with Roadshow.
- Minimum macOS release and distribution method.
- Amiga GUI dependency policy: built-in Intuition/GadTools for broad OS 3.x reach versus optional MUI. Do not assume ReAction exists on every OS 3.x install.
- Exact old Windows editions/network stacks and minimum DOS hardware. Windows 3.1 and Windows for Workgroups 3.11 need separate validation.
- Filename conversion rules and maximum file size per target filesystem/API.
- Final protocol encoding, limits, checksum and development ports after a small cross-platform prototype.
