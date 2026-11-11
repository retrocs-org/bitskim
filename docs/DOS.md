# BitSkim 0.1 for DOS / mTCP

Experimental 16-bit DOS client using Michael B. Brutman's mTCP library and the
same BitSkim transfer engine/wire protocol as the other clients. This is a
foreground text program with an 80x25 transfer dashboard, not a TSR: networking stops when you quit it.

## Setup

Use DOS 3.3 or later, a configured Ethernet-compatible packet driver and enough
free conventional memory. The build generates 8086 instructions, but physical
8088/8086 hardware and specific MS-DOS releases have not been tested. Automated
checks cover local files, transfers, discovery and the text interface. Start
with roughly 400 KB free conventional memory; this is a
provisional recommendation, not a measured minimum.

1. Load the packet driver appropriate to your network card. Its interrupt must
   match PACKETINT in your mTCP configuration.
2. Set `MTCPCFG` to the existing mTCP config file. Use mTCP DHCP to obtain a lease
   first, or configure a valid static IP address, netmask and gateway. BitSkim
   does not configure the card, run DHCP or renew leases.
3. Extract the package into a writable directory and change to it.
4. Run `BITSKIM`. It listens on TCP and UDP 46440 by default.

Example only (adapt interrupt/card parameters and paths to your setup):

```
SET MTCPCFG=C:\MTCP\MTCP.CFG
CD C:\BITSKIM
BITSKIM
```

`MTCPSAMP.CFG` is a commented configuration example. Do not replace an existing
working mTCP configuration with it without adapting the values to your LAN.
See the [mTCP project](https://www.brutman.com/mTCP/) and its
[official source/developer information](https://github.com/mbbrutman/mTCP).
Only one foreground packet-driver application can own this instance of the
stack; mTCP is linked into BitSkim, rather than a resident service to start.

## Text interface

The default interface is a dark 80x25 dashboard with cyan accents, spacious
computer entries and one transfer area. VGA uses a muted navy/teal palette;
original palette registers are restored on exit. Non-VGA and monochrome
displays retain standard hardware colors. Select a nearby computer with arrows;
F5 or Enter opens a separate file browser. Tab also opens or closes the browser.
The browser supports Home/End and Page Up/Down, Space to tag files or folders,
Enter to browse directories or send a file, and F5 to queue tagged entries or
confirm the selected path. Backspace opens the parent directory. Esc returns to
the dashboard; after sending, the browser closes so progress stays visible.

| Key | Action |
| --- | --- |
| F1 | Help |
| F2 | Preferences: name, visibility, incoming transfers |
| F3 | Manual IPv4 connection, then file path |
| F4 | Refresh network discovery and local files |
| F5 | Open file browser; inside it, send tags or confirm the selected path |
| F6 | Approve waiting offer and choose a new DOS 8.3 path |
| F7 | Decline waiting offer |
| F8 | Cancel transfer |
| F9 | Queue controls |
| F10 / Esc | Quit from dashboard; Esc closes a browser or prompt first |

Prompts are modeless with respect to networking: offers and transfers keep
being processed while help/preferences/file prompts are open. A new offer
opens its decision popup over the current prompt, preserving your draft. Preferences
uses Tab to select a field, Space to toggle a checkbox and Enter to save.
Settings remain in the original launch directory even after browsing folders.

The transfer area shows the current filename and file number, readable KiB/MiB
amounts, recent speed, percentage and an estimated time remaining when data flows. Peer selection retains its stable ID through discovery changes;
a disappeared peer is never silently replaced by a different recipient.
The file list is bounded to 128 entries; browse a smaller folder or enter a
known file path in the send prompt if necessary. Paths remain ASCII/8.3.
The interface saves/restores an existing 80x25 text screen and cursor. Other
starting video modes are reset to their BIOS default on exit; physical CGA/MDA
and 8088 performance remain untested.

## Optional command interface

Use `BITSKIM /CLI` for the original scrolling command interface:

| Command | Behaviour |
| --- | --- |
| `LIST` | List current peers and their numbers |
| `REFRESH` | Search for nearby computers |
| `SEND 1 FILE.ZIP` | Send one regular file to the currently numbered peer |
| `SENDIP 192.168.1.20 FILE.ZIP` | Send by IPv4 address; an optional `:port` is accepted |
| `ACCEPT NEWFILE.ZIP` | Approve the incoming file and choose a new local filename |
| `DECLINE` | Decline the waiting offer |
| `CANCEL` | Cancel the current transfer |
| `STATUS` | Show state, bytes transferred/total/remaining and recent bytes/s |
| `NAME My DOS PC` | Change the ASCII client name |
| `VISIBLE ON` / `VISIBLE OFF` | Enable/disable discovery visibility |
| `RECEIVE ON` / `RECEIVE OFF` | Enable/disable incoming offers |
| `HELP` / `QUIT` | Help / stop the client |

Commands are case-insensitive; input paths cannot contain spaces. Peer numbers
are resolved to a stable peer ID when SEND is executed; identity is checked by
the normal protocol handshake. Review LIST again if peers have changed.

Incoming offers automatically open a centred popup with sender, filename and
size. Choose Accept or Decline with Tab/arrow keys and Enter, or use A/F6 and
D/F7. Accept opens a separate save-path prompt and still requires confirmation
before any file is created. Esc defers the decision; F6/F7 remain available.
If a preferences/help prompt was open, its draft is restored after the decision.
Cancellation by the sender closes the stale incoming popup automatically.
Network processing continues throughout.

Incoming offers require explicit approval and a destination. Choose an ASCII
DOS 8.3 filename even when the sender's original filename is longer or Unicode.
The client never silently truncates/remaps received filenames and refuses DOS device names.
Paths must use existing folders and be at most 127 characters. Temporary files
use short 8.3 names in the destination folder. Normal cancellation removes
partial files; a forced reboot or power loss may leave BSxxxxxx.Txx files.
Existing destination files are never overwritten.

Transfers stream through bounded buffers, with the same CRC-32 integrity check
and 2,147,483,647-byte file limit as the other clients. Large files still depend
on the DOS filesystem, available disk space and network reliability. There is
no encryption/resume, custom theme, mouse UI or TSR
background mode. Sender names are unverified. Use a trusted LAN.

`BITSKIM.CFG` in the launch directory stores the client name, identity,
visibility and receiving preference. Run from the same directory each time;
copying that config to another computer also copies its identity. Delete the
config on the new machine to generate a different identity. Settings writes
report a failure if the working directory is not writable.

Discovery is IPv4 UDP broadcast. Routers/NAT/guest networks may block it; use
SENDIP when that happens. DOS runtime's normal network is NAT, not a LAN
bridge. Host-to-guest receiving requires port forwarding. This project's tests
use an isolated temporary DOS runtime drive; no Amiga test system configuration is changed.

## Batch use and checks

```
BITSKIM /SEND 192.168.1.20:46440 FILE.ZIP
BITSKIM /RECEIVE NEWFILE.ZIP
BITSKIM /CHECK
```

/SEND sends one file and exits after completion/failure. /RECEIVE explicitly
pre-approves the next incoming file into the named path and waits for that
transfer; use only when ready to receive from a trusted sender. Both return 0
on a successful transfer and 1 on failure. Ctrl-C exits the interactive loop.
There is no automatic-accept preference for interactive operation.

/CHECK runs local CRC, exclusive-creation, no-overwrite rename and DOS-name
checks without starting mTCP. It creates/removes BSCHK1.TMP through BSCHK3.TMP
in the working directory; run it in an empty scratch directory.

## Build

Full source, including the pinned unmodified mTCP source, is included in the
source archive. The pinned revision is in third_party/mtcp/REVISION.

Use Python 3 and an installed Open Watcom C/C++ compiler for 16-bit DOS.
On Windows, from a checkout in a path without spaces:

```
python scripts/build-dos.py --native --watcom C:\WATCOM
```

The Windows-native invocation is provided but has not been exercised here.
The validated build uses Open Watcom 2.0 beta dated 2026-11-02 inside a private
DOS runtime drive:

```
python3 scripts/build-dos.py --watcom /path/to/watcom --runner /path/to/runner
```

The compiler directory must contain BINW/WCC.EXE, WPP.EXE, WASM.EXE, WLINK.EXE,
the standard headers and DOS libraries. The compiler is an external build tool
and is not in the source archive. DOS runtime must be runnable headlessly with SDL's
dummy video/audio drivers. All compiler units use the large model and 8086
code generation; the C++ adapter and mTCP also use two-byte structure alignment.
The core is compiled as C and the adapter/mTCP as C++. No BSD socket library
or DOS extender is required by the resulting BITSKIM.EXE.

Build output: build/dos/BITSKIM.EXE and BITSKIM.MAP. Then refresh the full source
archive and run `python3 scripts/package-dos.py`. The package includes SOURCE.ZIP
with the complete matching source. Do not omit it when redistributing the DOS
package. Compiler identity/hash and source/artifact fingerprints accompany it.

Runtime check (requires a separately acquired NE2000.COM and a DOS runtime build with
Slirp available):

```
python3 scripts/check-dos-runtime.py --runner /path/to/runner --packet-driver /path/to/NE2000.COM
```

This uses only work/dos-runtime, transfers generated binary test data between
DOS and the host C engine, compares bytes and runs the local file checks. The
packet driver and DOS runtime/Slirp are test tools, not included in the release.

## Licensing

BitSkim-owned sources retain their MIT licence. mTCP is copyright Michael B.
Brutman and GPLv3-or-later. The linked DOS executable is distributed under
GPLv3-or-later; COPYING.TXT contains that licence, SOURCE.ZIP supplies the
corresponding source, and THIRDPTY.TXT/WATCOM.TXT retain dependency notices.
The other platform binaries are not linked with mTCP and keep their existing
licensing. Do not describe the DOS binary as MIT-only.

Project: https://github.com/retrocs-org/bitskim
Website: https://bitskim.retrocs.org

## Multiple-file queue

Space tags files or folders in the file browser; F5 queues tagged entries for the selected recipient within the shared 32,760-entry ceiling. Paths are expanded before queueing, so changing the displayed directory cannot redirect a queued path. F9 offers retry/continue, pause-after-current, skip-idle and clear-waiting. Updated peers can approve a complete collection once; older peers receive individual offers. Failed/declined/cancelled files pause the queue for a decision. Queues are not saved on exit. TLS and retained-checkpoint resume are modern-client features; DOS remains plaintext/non-resumable.

## Compact transfer display

One progress bar tracks the current file and resets for each new transfer. The summary identifies the current filename, size progress, percentage and transfer speed. Outgoing FIFO runs show the file ordinal and number still waiting, excluding the active send. Negotiated incoming batches show their file position and remaining count; older offers have no remote total. The Mac groups queue actions in its compact ellipsis menu; Windows uses a native Queue menu; RISC OS uses one Queue button opening its native menu. DOS retains F9 controls and Atari its existing queue page, keeping these controls out of the normal progress area. Queue pause takes effect after the current file.

## Receiving a file batch

Updated senders/receivers stream a complete disk-backed list; paged/scrolling views let you browse every entry. Accept all chooses one destination folder for the remaining listed files; subsequent files follow without additional prompts. By default, existing files are preserved, with platform-appropriate shortened/mapped names and collision numbers where necessary. Cancelling, failing or changing a batch revokes this temporary approval. Older peers still use per-file approval. See [TRANSFERS.md](TRANSFERS.md) and [PROTOCOL.md](PROTOCOL.md).

All clients now use disk-backed queues with a portable 32,760-file count ceiling and one approval per complete collection. See [queue storage](TRANSFERS.md#queue-storage-and-practical-limits).

All current builds advertise a versioned BXC1 network capability profile in discovery and confirm directional features/limits in the live handshake. This build reports plaintext-only transport, its implemented filename policy/limits, 2 GiB-minus-one file limit, and available collection approval. Unsupported TLS, resume, directory transfer and active-file pause are not advertised. Capability exchange does not replace receiving approval. See PROTOCOL.md.

## Transfer tools

Open F1 then T (or DIAGNOSTICS / HISTORY in CLI mode) for diagnostics, recent session history and the Rename/Skip/Replace conflict policy. See [Transfer tools](TRANSFER-TOOLS.md) for filename mapping, verification and legacy-peer behavior. Native runtime validation is recorded separately.

Folder transfers and disk-space prompts are described in [Transfer tools](TRANSFER-TOOLS.md). Space tags folders; F5 sends the selected folder, while Enter browses it. Native DOS runtime validation includes nested and empty folders in both directions.
