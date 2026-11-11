# Amiga preferences and ARexx — BitSkim 0.1

Project repository: [retrocs-org/bitskim](https://github.com/retrocs-org/bitskim). Website: [bitskim.retrocs.org](https://bitskim.retrocs.org).

## Separate preferences program

Keep `BitSkimPrefs` and `BitSkimPrefs.info` beside `BitSkim` and its icon. Open the editor from Workbench, Shell (`Bin:BitSkim/BitSkimPrefs`) or the client's Preferences button. It does not need a TCP/IP stack and does not open a network listener. The main client continues transfers while preferences are open. The editor opens as a normal Workbench window with the selected palette in its content area and themed image controls. Intuition window chrome and system requesters retain Workbench styling. The client also opens on Workbench, or starts directly iconified when that setting is enabled. Neither app creates a private screen. Catppuccin Mocha is the default; Classic uses Workbench colors.

The sidebar groups settings into General, Appearance, Receiving and About. Appearance shows a live sample window with peer names, transfer progress and a send button, using the selected palette. The editor and preview obtain separate colors from Workbench's color map without changing shared pens. Browsing themes changes only the preview; low-color screens approximate the palette. The preview uses illustrative content and performs no transfer.

Edits remain in a local draft while switching sections. Revert restores the values loaded when the editor opened; Cancel/close discard the draft. Save applies every section together. The editor retains its opening palette until reopened; only the preview changes while browsing.

Use writes live settings to `ENV:BitSkim/BitSkim.prefs`; Save also writes `ENVARC:BitSkim/BitSkim.prefs`. Both then signals the primary `BITSKIM` client's task to reload. Reload waits until any transfer finishes, so its receive policy/theme is not changed halfway through a transfer. “Start iconified” applies on the next launch. Cancel and the close gadget discard editor changes. Use one editor at a time; the last saved copy wins. If the client is stopped, preferences apply on its next launch. Secondary clients can use ARexx `RELOAD` explicitly.

## Local ARexx host

With `rexxsyslib.library` available, the main client registers **BITSKIM**, or **BITSKIM.1** through **BITSKIM.99** if names are occupied. RexxMast must be running to execute scripts with RX. The client still works without ARexx support. The port remains available while iconified. Commands are local only, with the normal Amiga shared-task trust model; they are not exposed over the file transfer protocol.

Use `OPTIONS RESULTS` and `ADDRESS BITSKIM` in a script. Return codes: 0 success, 10 unknown command/bad arguments, 20 failed operation or shutting down. Success means a command was accepted; SEND completes asynchronously. Check STATUS and EVENTS for its eventual outcome. No arbitrary shell execution command is provided. Commands are case insensitive; boolean arguments accept ON/OFF or on/off. Quoted arguments support spaces and doubled matching quotes, with no shell expansion. See the versioned integration contract below.

| Command | Result or action |
|---|---|
| `VERSION` | `0.1` |
| `STATUS` | Space-separated numeric KEY=VALUE fields: ACTIVE, INCOMING, AWAITING, ICONIFIED, RECEIVING, VISIBLE, PORT, PEERS, BYTES, TOTAL, RELOAD, BPS (bytes/second; zero inactive/unmeasured) |
| `PEER index` | Zero-based peer: `IPv4:port name`; name is the remainder and may contain spaces |
| `SHOW` / `HIDE` | Restore window / iconify with Workbench AppIcon |
| `PREFS` | Launch separate BitSkimPrefs process |
| `RELOAD` | Reload saved preferences when idle |
| `REFRESH` | Trigger discovery |
| `VISIBLE ON` / `VISIBLE OFF` | Change discovery visibility for this session |
| `RECEIVING ON` / `RECEIVING OFF` | Change incoming-transfer policy for this session |
| `SEND IPv4[:port] "Amiga:path with spaces"` | Offer a file, default remote port 46440 |
| `CANCEL` | Cancel the current transfer |
| `EVENTS [sequence]` | Read retained events newer than sequence (default 0) |
| `QUIT` | Reply, then shut down the client |

## Notification integration

EVENTS is a **polling interface**, not a push callback. A dock or notification helper can poll at a modest interval and keep its own sequence cursor. Responses look like `LATEST=5 LOST=0 4:4 5:2`. Each `sequence:type` pair identifies a core event: 1 peers changed, 2 incoming offer, 3 status changed, 4 completed, 5 error. The newest 32 events are retained; LOST=1 means the requested cursor predates that history. Query STATUS to resynchronize. Event sequence numbers reset when the process restarts. Multiple scripts can read independently without consuming each other's notifications.

No third-party notification program is required. A future integration can map event types to its own notification UI, dock badge or Directory Opus action. Polling scripts should query STATUS for current progress; history contains event types, not snapshots or filenames. The running client handles incoming consent and the editable full save path in a modeless window. Networking and ARexx continue while it is open; CANCEL or timeout invalidates the offer before any late acceptance can create a file. ARexx does not bypass acceptance or overwrite protection. Dedicated DOpus buttons and push notification adapters are not yet supplied.

Example scripts are in `examples/arexx/` in source and `ARexx/` in the Amiga package. Configure a dock launch action as `RX Bin:BitSkim/ARexx/Status.rexx`, or send `SHOW` through ARexx. Adapt the port for a secondary client.

## Validation

Both executables cross-build with the Amiga GCC toolchain and OS 3.x NDK headers. Host-side command tests cover parsing, bounds, policy commands and failed sends. Native Workbench launching, saved-preferences reload, ARexx message replies and dock/DOpus integration still require guest runtime verification.

ARexx message/reply handling follows the Commodore NDK `rexx/storage.h` and `rexxsyslib` autodocs, with result Argstrings only when RXFF_RESULT is requested. Reference: [Commodore ARexx Programmer's Guide](https://retro-commodore.eu/files/downloads/amigamanuals-xiik.net/eBooks/Amiga%20Programmers%20Guide%20to%20Arexx%20-%20eBook-ENG.pdf).

## Themed Workbench controls

Native Intuition boolean image gadgets provide palette-matched buttons, selected sidebar items, checkboxes and theme selection, including pressed states. The selector cycles forward on click and backward on Shift-click. The client displays five themed peer rows with up/down controls to browse the full peer catalog. A peer-list change clears selection to avoid keeping an index that could refer to a different computer. Text fields retain native editing behavior. The widget images use allocated Workbench palette pens and are freed when the window/page closes. No Workbench pens are recolored. Startup iconified registers the Workbench AppIcon before any window is opened; restore creates a normal Workbench window.

## Iconified operation

Iconifying pauses peer searches and clears the cached peer list; STATUS reports PEERS=0 until discovery resumes. REFRESH does not initiate searches while iconified. Use SHOW to resume discovery before querying peers. Manual-address SEND, incoming transfers, discovery replies, event history and management commands remain active. The client sleeps until a network socket, its window, Workbench, an ARexx message or a Ctrl-C/Ctrl-E break signal needs attention (at most one second), so commands are serviced as soon as they arrive without idle polling. Ctrl-C quits like the QUIT command.

## NovaDock / versioned extensions

ARexx protocol 2 adds CAPABILITIES, SESSION, DETAILS, LAST, EVENTDETAILS, PEERS, PEERINFO, SENDID and ERROR without changing the existing command names or numeric STATUS fields. STATUS adds numeric BPS only. Full response schemas, byte escaping, transfer results/IDs, sequence-wrap rules, errors, bounds and modeless-window behavior are specified in [NOVADOCK.md](NOVADOCK.md). Use that contract for new dock integrations; PEER indices and legacy human-readable names are retained for compatibility.

## Modeless transfer windows

Send file opens a themed full-path entry window showing a retained recipient. Select a file in Workbench and drop it into the main window as an alternative to typing. GUI peer sends use the stable-ID engine API and check HELLO identity; a vanished/ambiguous ID never falls back to another recipient. Explicit manual-address sends retain address-based behavior. A main-window drop uses its current selection, while clicking Send in an already-open path window uses that window's displayed recipient.

The incoming window combines Accept/Decline with an editable save path. Auto-accept with Always ask disabled still saves directly to the configured folder; Always ask requires the modeless confirmation. The transfer serial is checked before acceptance. Dialogs have independent Workbench screen locks, pens and gadget lifetimes, and their signal is included in WaitSelect. SHOW/HIDE or main-window theme changes do not free dialog resources. The blocking ASL picker is replaced by path entry plus Workbench selection/drops. Normal client errors use modeless notices; startup errors and the separate preferences process do not run in the client's management loop.

## Queue additions

CAPABILITIES adds `ENQUEUEID=1 QUEUE=1` without changing existing commands.
`ENQUEUEID <stable-id> "Amiga:file path"` appends one file, up to eight total.
Resolution/HELLO consistency checks occur at send time. Direct SEND/SENDID keep
their existing immediate/busy behavior. `QUEUE STATUS` returns numeric
`COUNT=n PAUSED=n`; `QUEUE CONTINUE`, `QUEUE PAUSE`, `QUEUE SKIP` and
`QUEUE CLEAR` manage the local queue. SKIP while active returns BUSY. CLEAR
preserves an active file and removes waiting files. Failures pause the head;
Continue retries it explicitly. Iconified discovery must be active or the peer
present before a stable-ID send can resolve. No change to numeric STATUS or
notification event numbering. Amiga queues remain plaintext/non-resumable.

NETWORKCAPS returns the local wire-capability profile as decimal key=value tokens:
VERSION, TX, RX, MAXSEND, MAXRECEIVE, WIRENAME, LOCALNAME, PATH, NAMEPOLICY,
CODEPAGE, COLLECTION, SECURITY and PAIRED. Feature bits and field meanings follow
PROTOCOL.md. CAPABILITIES advertises NETWORKCAPS=1. Existing STATUS is unchanged.
