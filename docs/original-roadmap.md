# Delivery plan

Proposal dated 1 November 2026. Complete each milestone before adding platforms.

## 1. Resolve the oldest first-client constraints

Choose an Amiga hardware/OS/stack baseline and GUI dependency policy. Verify the compiler can produce the intended CPU executable. Make a small Amiga socket program exchange bounded messages with a macOS command-line peer over manual IP in both directions. Measure RAM, responsiveness and basic throughput on real hardware when available; test execution alone does not establish hardware compatibility.

Exit: both sides can connect, send and receive without blocking the UI/event loop; toolchain and stack dependencies are recorded. Keep discovery out of this first spike so network reachability is easy to diagnose.

## 2. Freeze the minimal protocol

Write the actual wire specification, state transitions, limits and golden byte vectors. Implement the shared parser, serializer and streaming transfer core. Add a reference CLI using the same core for diagnostics.

Exit: cross-endian vectors, fragmented TCP input, partial writes, invalid lengths, zero-byte files, size limit rejection and checksum failures behave consistently. A transfer larger than the configured memory buffer uses bounded memory.

## 3. Build the macOS and Amiga user flow

Create native peer lists, manual IP entry, Send File, incoming offer dialog, file/directory requester, progress and cancel. Add drop-on-peer support on macOS and validate Workbench AppWindow behavior on Amiga. Persist name, visibility, receiving policy, auto-accept and destination mode. Show useful hostname fallback when no hostname is configured.

Exit: Amiga→Mac and Mac→Amiga work. Confirmation and auto-accept each honor the save-location policy. Both clients remain responsive. Receiver shows completion only after file finalization; sender handles missing final acknowledgment honestly.

## 4. Add discovery and release hardening

Add bounded UDP probes/replies and peer expiry. Test multiple interfaces, duplicate names/IDs, application restart, IP changes, sleep/wake, visibility disabled, incoming transfers disabled and port conflicts. Document local-network permissions/firewall steps and manual-IP diagnostics.

Exit: discovery finds peers on a supported LAN, and failures explain the next useful action. Decline and timeout send no file payload. Disk full, missing destination, existing file, invalid name, network disconnect and cancellation do not produce a falsely successful destination file. No silent overwrites or executable auto-launch.

## First-release verification matrix

| Area | Required checks |
| --- | --- |
| Interoperability | Both transfer directions; real Amiga baseline; actual packaged Apple silicon app |
| Content | Empty, small binary, random data, non-ASCII names, unsupported names, maximum supported size boundary |
| Integrity | Compare original/final bytes independently; deliberately altered/truncated data fails |
| Consent | Accept, decline, timeout; all auto-accept/save-location combinations |
| Storage | Full volume, read-only folder, removed volume, name collision, unsafe filename, no executable launch |
| Networking | Manual IP, discovery, blocked UDP with reachable TCP, unreachable TCP, duplicate peers, partial I/O |
| Responsiveness | Slow sender/receiver, no incoming data, cancel, bounded RAM and prompt limits |
| Parsing | Golden cross-endian vectors, malformed packets, oversized fields, unknown versions, fuzzing on modern host |
| Privacy | Visibility off stops advertisements/replies; incoming-off rejects manual offers; permission denial gives actionable status |

## Subsequent releases

1. Add Linux and Windows 10 using the established protocol and conformance vectors.
2. Investigate DOS and Win16 early, before expanding the protocol: they may expose integer, memory or stack assumptions. Add separate adapters/build targets rather than weakening the wire contract ad hoc.
3. Add Windows 95/98/ME/2000, RISC OS and Atari clients as individually tested ports. Order should follow available test hardware and contributors.
4. Add queues, directories/metadata and resume with explicit protocol extensions.
5. Add authenticated encryption on capable platforms, including pairing and downgrade tests. Test every combination of plaintext-only, encryption-preferred and encryption-required policies.

No calendar estimate until the first real Amiga↔Mac spike establishes the development and hardware constraints.
