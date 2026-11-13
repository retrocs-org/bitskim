# BitSkim 0.2 — experimental Atari GEM clients

**Compiled, not Atari-runtime tested.** No Atari hardware or configured test system
is currently available for this project. Native GEM rendering, real STiNG /
MiNTNet behavior, NetUSBee drivers and real filesystem collision protection
still need testing. Host adapter tests are not a substitute for those checks.

## Choose a build

| Package | CPU / intended machine | Network stack |
|---|---|---|
| Atari-TOS-STinG-68000 | 68000 STe; also intended for Falcon | STiNG transport API 1.26 or newer |
| Atari-MiNTNet-68000 | FreeMiNT on 68000+, including Falcon 030 | Native MiNTNet BSD sockets |
| Atari-MiNTNet-68060 | Falcon 060 with appropriate FreeMiNT kernel | Native MiNTNet BSD sockets |

Each ZIP has `BITSKIM/BITSKIM.PRG`. Extract one variant to a writable directory
and double-click it. Use one instance/network variant at a time. No installer,
AUTO-folder changes, system configuration, stack or driver installation is
performed. `BITSKIM.CFG` is saved in the process's launch/current directory.
Preserve that file to retain the client name, installation ID and preferences;
remove/correct it manually if a saved setting prevents startup.

The intended display minimum is 320×200; the client uses the small system font
and native VDI palette. More memory than a minimal 1 MB configuration may be
needed; **2 MB or more is recommended for initial testing**, not a tested minimum.
The three builds are approximately 216–230 KiB executables before stack,
engine, GEM and networking allocations. Do not infer that the networking
stack's advertised minimum memory is sufficient for the complete application.

## TOS, NetUSBee and STiNG

Configure NetUSBee's **Ethernet** port and STiNG first, then verify ordinary IP
connectivity before launching BitSkim. The hardware provider's guide describes
installing STiNG, the matching EtherNEC/NetUSBee STX driver, selecting Ethernet,
setting addresses and rebooting: [NetUSBee STiNG setup](https://hardware.atari.org/manuals/netsting.htm).
Its sample 192.168.255.x addresses are examples; choose unused addresses and a
netmask appropriate for your own LAN. BitSkim does not configure DHCP or routes.

The application locates the `STiK` cookie, checks `STiKmagic`, obtains the
`TRANSPORT_TCPIP` table and requires version `01.26` or newer. It uses the
SDK's GCC short-argument call wrappers rather than compiling the whole app
with `-mshort`. Old STiK-only stacks or other socket test executions are not claimed.

TCP accepts transfer the established passive handle into a connection slot,
then immediately rearm a listener. Calls use STiNG's deferred mode to avoid
waiting in cooperative desktop callbacks. UDP sends use CAB addressing with
the listener's local port. Since a passive STiNG UDP handle becomes associated
with its first sender, each received datagram is copied with its actual source
address, freed through KRfree, and the passive handle is reopened. This can
lose queued simultaneous discovery replies; discovery is best effort and
repeated probes/manual IPv4 are available. Actual STiNG routing/broadcast
behavior still needs native testing.

## Falcon / MiNTNet

Install a FreeMiNT kernel suitable for the processor, AES (for example XaAES)
and a working **MiNTNet Ethernet interface driver** for the hardware. A NetUSBee
USB-controller module alone does not establish an Ethernet interface. Verify
the interface address, route and reachability with your usual MiNT tools.

Use the 68000 MiNTNet build as the conservative baseline. The 060 variant uses
`-m68020-60 -msoft-float` for application objects; use it with a correctly
configured Falcon 060 system. It does not require or talk through STiNG.
The adapter uses native nonblocking sockets and polls connection completion;
GEM's event/timer loop remains responsible for sleeping/cooperative scheduling.

## Sending, receiving and preferences

1. Start BitSkim on both computers on a reachable IPv4 network. UDP discovery
   and TCP transfers default to port **46440**. Firewalls must permit them.
2. Click a peer row, or open **Connect by IP** and enter an IPv4 address with optional
   `:port`. Confirm to select it as the destination for your next send.
   Arrow keys scroll peers. Selecting a peer clears the manual address.
3. Choose **Send File** to select a file and start sending it. Choose
   **Send Directory**, open a folder in the selector, and click OK to send it.
   Folder transfer requires a compatible recipient.
4. Incoming offers show the sender, file or batch, save path and Accept/Decline
   controls. For a directory or batch, click **Browse...**, open the destination
   folder, and click OK to select it. Then click **Accept all** to receive the offer.
5. **Prefs** has General, Network and About sections. General holds the client
   name, download folder, approval settings and completion sound. Network
   holds discovery visibility, incoming permission and transfer limits.
   Save persists changes, Use applies them for the session, and Revert
   restores the currently applied settings. F1 opens transfer tools.

The native AES selector is modal and may briefly delay discovery while open.
Under multitasking MiNT, transfers can continue with the window in the background.

**TOS build:** ASCII paths with 8.3 components only, at most 127 bytes in the
GUI. For an offered longer/Unicode filename, edit Save to an ASCII 8.3 name
before Accept. **MiNTNet builds:** ASCII paths, including long components when
the filesystem permits them, still with the 127-byte GUI limit. Use native
GEMDOS drive paths (`C:\DIR\FILE.TXT`, `U:\...`); Unix-root aliases are not
interpreted by the editable fields. File bytes are unchanged. Maximum offered
size is 2 GiB minus one byte; actual disk/filesystem limits remain untested.

Temporary leaves use 8.3 names. Creation is exclusive; finalization calls
GEMDOS Frename on TOS and has **no delete/replace fallback** (MiNTLib's POSIX
rename wrapper can delete a destination). MiNTNet first attempts no-replace
hard-link creation, then permits direct Frename only for positively identified
FAT/VFAT filesystems. If neither safe method is available, finalization fails
without claiming delivery. This deliberately excludes unknown legacy/XFS
rename behavior. These native syscall/filesystem semantics still need hardware
testing. Data is not opened automatically after receiving.

Atari builds are **plaintext/non-resumable**. On modern peers choose Disable or
Prefer encryption to communicate with them. Force encryption deliberately
refuses these clients. Sender names/installation IDs are unverified; use a
trusted LAN. Failed/declined/cancelled queued items require explicit retry or
skip; check the receiver before retrying unconfirmed delivery.

## Build and package

On macOS, with Python 3, tar, Xcode command-line tools and Rosetta for the
pinned x86_64 compiler:

```sh
python3 scripts/bootstrap-atari.py
./scripts/build-atari.sh
./scripts/release-amiga.sh
python3 scripts/package-atari.py
python3 scripts/sync-amiga-bin.py
```

The bootstrap installs only under `work/atari/`, verifies pinned SHA-256 values,
and downloads GCC 4.6.4, binutils 2.45, MiNTLib 0.60.1, GEMlib 0.44.0 and the
external STiNG SDK header at commit
`ab065dc94b01e9a5e29b584e2566a6552facd504`. It never modifies `/opt` or `/usr`.
The MiNTLib/GEMlib source archives and original library build recipes are
included for packaging/relinking. To use another suitable host toolchain:

```sh
ATARI_CC=/path/to/m68k-atari-mint-gcc \
STING_INCLUDE=/path/to/sting-sdk/include ./scripts/build-atari.sh
```

Use compatible MiNTLib/GEMlib archives and standard 32-bit int ABI; do not add
`-mshort`. SDK headers use GNU inline assembly; application units use GNU89.
The shared engine remains conservative C. Output directories:
`build/atari/sting/`, `mintnet/`, `mintnet060/`, each with BITSKIM.PRG and relink
objects. Packaging verifies source/artifact provenance and TOS PRG magic, and
creates three `outputs/BitSkim-0.2-Atari-*.zip` files with checksums.

The `RELINK` directory contains own-code objects, full BitSkim source, library
source snapshots, original library build recipes, notices and a relink script.
On a host shell run `sh RELINK.SH` with ATARI_CC set. LIBRARY_FLAGS may provide
`-L/replacement/libs -B/replacement/startup/` to use modified libraries. Keep
CPU/ABI options consistent; see the upstream library README/build recipes.
STiNG itself, its external SDK header, NetUSBee drivers and TOS ROMs are not
redistributed. The bootstrap obtains the SDK header for source builds.

## Validation and first hardware checks

- All three m68k PRGs compile/link with the real GEMlib, MiNTLib and STiNG SDK.
- Host tests execute the production STiNG adapter against a transport double:
  listener rearming, distinct UDP peers, backpressure, send size limits,
  negative NDB results, source ports, and explicit 8.3 rejection.
- A host AES/VDI double checks application startup/teardown and stack-before-
  hostname initialization. It does not test native AES drawing or timing.
- Production MiNTNet adapter + shared engine transfer binary data both ways
  over host sockets, with mocked GEMDOS traps, and test empty files/native path
  expansion. Native Frename behavior and the 200 Hz clock still need Atari tests.

First hardware session: check launch and text layout at the intended resolution;
name/config persistence; two-peer discovery; manual IP both directions;
accept/decline/timeout; a small binary file and checksum; a paused two-file queue;
cancel/disconnect cleanup; and collision refusal with an existing sentinel file.
Run the same checks separately for STiNG and MiNTNet/060. No hardware support
claim should be inferred from these builds.

Sources: [STiNG SDK/source](https://github.com/th-otto/STinG),
[Atari cross tools](https://tho-otto.m68k.eu/crossmint.php),
[FreeMiNT](https://freemint.github.io/).

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls while Atari starts sends directly from the main window, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

All current builds advertise a versioned BXC1 network capability profile in discovery and confirm directional features/limits in the live handshake. This build reports plaintext-only transport, its implemented filename policy/limits, 2 GiB-minus-one file limit, and available collection approval. Folder collections are negotiated with updated peers. Unsupported TLS, resume and active-file pause are not advertised. Capability exchange does not replace receiving approval. See PROTOCOL.md.

## Transfer tools

Open F1 for diagnostics, recent session history and the Rename/Skip/Replace conflict policy. See [Transfer tools](TRANSFER-TOOLS.md) for filename mapping, verification and legacy-peer behavior. Native runtime validation is recorded separately.

## Amiga feature parity and remaining platform differences

Atari and Amiga share discovery/manual IP, file and folder queues, collection
accept-all, disk-space checks, adapted filenames, Rename/Skip/Replace, checksum
and negotiated written-file verification, transfer telemetry, rate limits,
recent history and diagnostics. Both remain plaintext and lack active-file
pause and retained interrupted-transfer resume. Their local settings offer
name, remembered destination, auto-accept/ask-path, visibility, receiving and
completion sound. Atari uses the Classic GEM appearance.

Host UI state checks exercise persistence, Save/Use/Revert/Cancel, section
navigation, monochrome contrast and quit confirmation.
These are production-code tests against AES/VDI doubles, not Atari rendering
or native multitasking validation. All three target binaries compile/link;
real STe/Falcon testing is still required.

## Desktop drops and completion sound

Under a desktop that provides MiNT named pipes and the AES `AP_DRAGDROP`
protocol, drop files or folders onto the BitSkim window. BitSkim reads `ARGS`
filename lists without blocking the transfer loop; dropping on a peer row
selects that recipient. Review the displayed selection, then click Send File to
queue it. Esc discards the remaining staged selection. The recipient is fixed
when Send File is pressed. Native filename and path limits still apply. The exchange
accepts at most 64 KiB of arguments and times out after five seconds without
progress. No arbitrary dropped data is run or opened. See the
[Atari Compendium drag-and-drop protocol](https://bus-error.nokturnal.pl/atari_compendium/html/appendh.htm).

Plain TOS does not provide the live pipe protocol. On desktops that pass file
paths to a newly launched PRG, BitSkim can stage those launch arguments for
review and Add; the existing selector and typed paths remain available. The
General preferences page has an optional Completion sound. Native desktop
and audio testing is pending; host tests cover fragmented ARGS framing and
quoted filenames via a stream double.
