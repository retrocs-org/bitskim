# BitSkim protocol 1 (application release 0.1)

Implemented by `core/bitskim.c`. All multibyte wire integers are unsigned, big-endian. Do not serialize native C structures. TCP is a byte stream: reads and writes may be partial. One connection carries one offer and one file, then closes.

## Discovery

Default UDP destination port: **46440**, experimental/private-use choice, not a registered service assignment. TCP listening port defaults to 46440 and is advertised independently. A development client sharing a host can listen on 46441 and bind an ephemeral UDP socket if the discovery port is occupied.

Discovery packets emitted by 0.1 are 160 bytes. Prefix: ASCII `BSKD`, byte version `1`, byte kind (`1` probe, `2` reply), followed by the greeting below, an optional 36-byte BXC1 capability profile, and zero padding. Input is bounded to 512 bytes and must contain the complete greeting. Replies go to the probe's source address and UDP source port. Use packet source IP plus advertised TCP port as the peer endpoint. Names and installation IDs are unverified.

Visible, receiving-enabled probes advertise their greeting; receivers may add those peers as well as replies. Hidden or receive-disabled probes use a zero installation ID and anonymous name; do not add them as visible peers. Respond only while visible and receiving-enabled. The core limits responses to one per second per source address (eight tracked sources) and handles at most eight UDP packets per tick. Probe around every 30 seconds and expire peers after 90 seconds. Modern hosts send per-interface subnet broadcasts plus limited broadcast; the classic adapter currently sends limited broadcast only. No mDNS or nonce negotiation in 0.1.

## Greeting payload

| Field | Bytes |
|---|---:|
| Installation ID (binary, 32 lowercase hex digits in local config) | 16 |
| TCP listening port | 2 |
| Name length in UTF-8 bytes, 1–63 | 1 |
| Platform length in UTF-8 bytes, 0–31 | 1 |
| Name | variable |
| Platform | variable |

UTF-8 must be well formed and free of ASCII/C1 controls. IDs are correlation identifiers, not credentials. Discovery pads this payload; TCP HELLO has the exact payload length.

## TCP envelope

| Offset | Field |
|---:|---|
| 0 | `BSK1` magic/version marker (4 bytes) |
| 4 | Message type (2 bytes) |
| 6 | Reserved flags, must be zero (2 bytes) |
| 8 | Payload length (4 bytes), at most 4096 |
| 12 | Payload |

| Type | Name | Payload |
|---:|---|---|
| 1 | HELLO | Exact greeting payload |
| 2 | OFFER | 64-bit file length, 16-bit UTF-8 basename length, basename |
| 3 | ACCEPT | Empty |
| 4 | DECLINE | Optional bounded explanation |
| 5 | DATA | 1–4096 file bytes |
| 6 | END | 32-bit CRC-32 of the entire file |
| 7 | COMPLETE | Empty |
| 8 | CANCEL | Optional bounded explanation |
| 9 | ERROR | Optional bounded explanation |

Application release 0.1 supports lengths through **0x7fffffff**. Reject a nonzero high word or larger low word in OFFER. This does not certify each filesystem's maximum capacity. Filenames must be 1–255 UTF-8 bytes, a single basename, and contain no controls or `/\:*?"<>|`; reject `.` / `..` and trailing dot/space. The Amiga UI imposes its narrower Latin-1/30-character receiving limit. The sender converts Latin-1 local names to UTF-8; modern names remain UTF-8. File contents are never converted.

## Sequence

Sender initiates TCP; both sides exchange HELLO. Sender sends OFFER and waits. Receiver validates and prompts; no destination file is created before local acceptance. Receiver opens a same-directory temporary destination then sends ACCEPT. Sender streams DATA, then END. An empty file has no DATA and CRC zero. Receiver checks the exact accepted byte count and checksum, closes the temporary file, and finalizes without replacing an existing file. Only then does it send COMPLETE. Sender reports success only after COMPLETE.

Decline, malformed input, wrong-state messages, unexpected end of stream, timeout, cancellation and file I/O failures abort. A local Cancel sends CANCEL and a recipient-decision timeout sends ERROR, each as a best-effort, nonblocking write; if a partly written frame is still queued, the engine completes it first, and if the terminal frame cannot be written in full the socket simply closes. ERROR and DECLINE from receiver failures are flushed where possible. After a terminal or final frame the engine shuts down its write side and discards inbound bytes until EOF or about three seconds, so the frame is not lost to a TCP reset. Peers must still treat an unexpected closure as an incomplete transfer.

A listener that is busy with another transfer, or has incoming transfers disabled, accepts the extra connection and sends one ERROR frame with a short reason before any HELLO (for example “Recipient is busy with another transfer; try again shortly” or “Recipient is not accepting files”), then closes gracefully. Existing receivers already handle ERROR in any state, so no new message type or version is needed.

Decision timeout: 120 seconds. Connect and other inactivity timeout: 30 seconds. Engine time comes from a monotonic clock where the platform offers one (POSIX `CLOCK_MONOTONIC`, Windows `GetTickCount64`, the AmigaOS E-clock); a remaining backward step, as on RISC OS when the clock is set, is absorbed rather than treated as elapsed time. The negotiated resume extension below adds verification-only heartbeats; the base sequence has none. TCP keepalive and TCP_NODELAY are enabled on transfer connections where supported; frames are written whole, so Nagle's algorithm would only hold short control frames behind delayed acknowledgments. The macOS adapter requests a 10-second keepalive idle, three probes 3 seconds apart, and a 30-second retransmission drop interval; OS scheduling can extend detection time. Engine send backpressure does not suppress reading peer EOF/cancellation. A sender with no socket progress for 5 seconds reports that it is waiting for the peer. A lost final acknowledgment produces “completion unconfirmed” on the sender; the receiver may already have saved the file. Do not automatically resend or overwrite.

CRC: CRC-32/ISO-HDLC, reflected polynomial `0xedb88320`, initial `0xffffffff`, final XOR `0xffffffff`. API calls accept the previous finalized CRC (initial call zero). Test vector ASCII `123456789` → `cbf43926`.

## Security and extension policy

The base sequence remains plaintext and unauthenticated. Capable builds implement the opt-in TLS and resume extensions below. Unknown messages/reserved flags remain errors; an extension is never sent to a peer that has not advertised it. Installation IDs and names remain unverified labels even in a shared-key TLS group.

## Windows implementation

The Windows client uses the same 0.1 wire format. Winsock handles retain their native pointer-sized SOCKET type. Local UTF-8 paths convert to UTF-16 for filesystem access. Windows receivers reject DOS device names and invalid Windows filenames before writing; this is a local filesystem constraint, not a protocol change. Temporary files use exclusive creation, and finalization refuses to replace an existing destination. On macOS and Linux finalization uses an atomic no-replace rename (`renamex_np(RENAME_EXCL)`, `renameat2(RENAME_NOREPLACE)`), then a same-directory hard link, if neither operation is available, finalization fails without creating, replacing or deleting the destination. No placeholder-and-replace fallback is used.

A local connection failure, unexpected TCP closure after the greeting stage, or transfer inactivity timeout removes the matching discovery entry immediately (address, listening port and negotiated installation ID when known). Other services on that address remain listed. A closure before any HELLO arrives, a busy/not-receiving ERROR, local or remote cancellation, remote decline and recipient-decision timeout do not evict the peer, because the host answered. A subsequent valid discovery advertisement can add it again; otherwise passive discovery expiry remains 90 seconds.

BS_EVENT_PEERS is raised only when a catalog entry is added, removed or changes name, platform or port; a refresh that only renews its last-seen time is not a change. Progress BS_EVENT_STATUS notifications are coalesced to at most one per bs_tick.

Clients may pause outgoing discovery searches and peer-catalog maintenance without stopping their TCP service or replies to valid discovery probes. The Amiga client does this while iconified and refreshes immediately on restore. This is local scheduling policy; the wire format is unchanged.

## Engine scheduling

Each bs_tick does bounded work: modern hosts write up to 32 DATA chunks and handle up to 64 received frames per tick; AmigaOS and RISC OS builds use 2 and 4 to keep cooperative desktops responsive. Builders can override these with `-DBS_SEND_BURST` and `-DBS_RECV_FRAMES`. The optional bs_wait helper blocks until an engine socket needs service (and, on AmigaOS, until a caller-supplied signal arrives), capped at one second for timers. None of this changes the wire format.

## Transfer telemetry

Sender progress counts payload bytes only after a DATA frame has been fully written to the local TCP socket. Receiver progress counts payload bytes written to the receive file. Neither means durable delivery until the final COMPLETE exchange. Recent speed is payload-byte delta over the most recent elapsed sampling interval (about one second), excluding protocol overhead and consent wait time. A stalled interval reports zero. The UI computes remaining data as max(total − transferred, 0). Telemetry does not add wire messages or change public BSStatus layout.

## Experimental RISC OS adapter

The wire format is unchanged. The RISC OS 3.x client translates local Latin-1 names to UTF-8 and requires incoming names to be representable in Latin-1. Its UI proposes ten-character FileCore leafnames and an editable native destination. Receive temporary files have ten-character leaves (`BS` + six hex timestamp digits + two hex attempt digits). The AmigaOS adapter uses the same short leaves, because OFS/FFS/RAM: names are limited to 30 characters and a suffix on the final name would not fit. Finalization uses native OS_FSControl 25 rather than UnixLib rename, to refuse an existing destination. File contents are unmodified; native filetype/load/exec metadata are not transmitted. Host loopback tests cover this adapter policy with a host commit surrogate; actual UnixLib networking and native FileSwitch remain untested.

## Local transfer observation

The C API exposes a per-client transfer serial and numeric pending/complete/failed/cancelled/declined result without changing BSStatus layout, event numbers or wire frames. Amiga ARexx uses this to retain local transfer snapshots and qualify IDs with a session token; these IDs are not transmitted or remote management handles. See NOVADOCK.md. Local cancellation sends CANCEL, so the remote endpoint normally records cancelled; if that frame cannot be written (for example a full send buffer), the remote endpoint sees a closure and records failed. Receiver-side checksum, write and finalization failures record failed and raise BS_EVENT_ERROR.

The additive bs_send_peer API resolves one unique advertised ID from the current catalog and checks the existing HELLO ID before emitting OFFER. A stale address with a different ID is rejected without a file offer or payload. IDs remain unauthenticated; this is recipient-consistency checking, not cryptographic pairing. Legacy address-based sends remain unchanged.

## Local NAT test launcher

There is no wire-format/version change for NAT testing. Discovery still uses
the datagram sender address plus the greeting's advertised TCP port. The DOS
launcher opts into BITSKIM_NAT_HOST=10.0.2.2: its local mTCP adapter maps
forwarded loopback source addresses (127/8) to the DOS runtime host alias before
passing them to the engine. Real LAN addresses and ordinary DOS operation
without the option are unchanged.

The launcher sets BITSKIM_PORT to the selected host-forward port (default
46442), binds the DOS TCP listener on that same port and forwards host TCP
46442 to guest TCP 46442. The greeting therefore advertises the port reachable
by the host; UDP discovery remains on guest 46440. The host can use the DOS
peer entry (loopback plus advertised 46442), while DOS can use its host entry
(10.0.2.2 plus the host's advertised port). Stable-ID HELLO verification remains
unchanged. Native DOS UI interoperability tests assert both resolved endpoints
and send in both directions using discovered IDs.

## Optional modern extensions (0.1, BX capability revision 1)

Disk-backed queues stream one file per connection. Capability 8 negotiates a
streamed complete collection and one consent; capability 4 alone retains the
older eight-entry format. Older peers retain per-file consent. Failed/declined/cancelled items pause for explicit retry or skip.
There is no automatic resend after unconfirmed delivery.
Queue pause is a local scheduling control: it prevents the next file from
starting and does not suspend the active file. No active-transfer PAUSE frame
or capability exists in protocol 1. Do not present queue pause as an active-file
pause or infer support from the peer operating system. Capability hints for TLS
and interrupted-file resume already travel in discovery and HELLO; optional
transfer extensions must also be checked against the live HELLO, not just a
cached advertisement. Unknown bits must not enable unsupported actions.

A capable greeting appends the five ASCII bytes `|BXhh` to its platform field,
where `hh` are two hexadecimal digits: bit 0 = resumable offers, bit 1 = TLS
available under current policy, bit 2 = eight-entry batch receiving UI, bit 3 = streamed collection receiving UI, bit 4 = versioned directional profiles and TLS-upgrade control, bit 5 = automatic unpaired TLS under current policy. Bit 6 (64) advertises negotiated directory collections when the receiving UI is enabled. Bit 7 reserves active-file-pause support and remains unset.
The platform field remains at most 31 bytes;
a configured platform longer than 26 bytes emits no extension. Updated peers
strip a recognized nonzero suffix from the display label. Old peers simply
show a longer valid platform string; HELLO length, discovery version and all
base frame definitions remain unchanged. Capability advertisements are public
and unauthenticated. They are hints, not a security-policy negotiation.

### Directional capability profile (BXC1 version 1)

Discovery places this 36-byte profile immediately after the exact greeting (not
at a fixed packet offset). Hidden/receive-disabled anonymous probes omit it.
Old peers ignore trailing discovery padding. Discovery claims are unauthenticated
hints: live HELLO/profile exchange is authoritative before any file offer.

When both peers advertise bit 4 in HELLO, each sends frame **21 CAPABILITIES**
with exactly this profile before offering files. Without bit 4, use the existing
HELLO/offer sequence and conservative legacy limits; never send a new control
frame to an old peer. Do not infer features from OS names.

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 4 | ASCII BXC1 |
| 4 | 1 | Profile version, 1 |
| 5 | 1 | Encryption policy: 0 Disable, 1 Prefer, 2 Force |
| 6 | 1 | Local save-name policy: 0 UTF-8, 1 Latin-1 mapping, 2 DOS 8.3 mapping, 3 ASCII mapping, 4 Windows ANSI code page |
| 7 | 1 | Optional pairing key required: 0/1 |
| 8 | 4 | Sending feature flags |
| 12 | 4 | Receiving feature flags; zero when reception disabled |
| 16 | 4 | Maximum supported outgoing file bytes |
| 20 | 4 | Maximum supported incoming file bytes |
| 24 | 2 | Maximum wire filename UTF-8 bytes |
| 26 | 2 | Effective local basename byte limit under save-name policy |
| 28 | 2 | Effective local path byte limit |
| 30 | 2 | Windows ANSI code page for policy 4; zero otherwise |
| 32 | 4 | Maximum approved receiving collection count; 1 for single-file UI |

Integers are big-endian. Current clients use 2,147,483,647 bytes per file and 255
wire-name bytes. These are implementation limits, not filesystem guarantees or
free-space estimates. Amiga reports a conservative 30-byte Latin-1 save name;
old FileCore reports 10; DOS/STiNG report 8.3 (12 including dot) and 127-byte
paths. Classic Windows reports its real active ANSI code page and 259-byte path
limit. Filename mapping remains receiver-side. Directory support is negotiated as described below; active-file-pause
remains unsupported. Queues and cancel/progress/CRC remain shared base functionality;
UI-only features such as trays are not transfer capabilities.

Profile TLS flags report build support even when policy is Disable; always check
policy as well. The smaller HELLO TLS hint remains effective availability under
current policy for old peers. AUTO_TLS describes enabled unpaired TLS, not a
permission to bypass Disable or a configured pairing key.

Sending flags describe outgoing implementation even if its receiving UI is
single-file-only. Receiving flags govern which offers may be sent. Negotiate
resume, collection approval and encryption using the matching direction, not an
intersection of unrelated receiving capabilities. A sender respects the confirmed
receiver's file/name/collection limits before offering. A receiver rejects a
sender exceeding its declared file/name limits. Malformed/inconsistent live
profiles fail closed. Invalid discovery profiles are ignored as unknown claims.
Unknown optional bits are retained for display but never enable an unimplemented
command. UI catalog updates include profile/policy changes, not only name/IP.

### TLS transport and upgrade

TLS builds default to Prefer (1); legacy builds report Disable (0). Existing
explicit saved settings retain their value. Disable always uses plaintext file
transport; Force always requires encrypted files in both directions. Prefer
uses encrypted transport when both live profiles allow it, even at a manual IP;
it uses plaintext only when the peer does not allow/support encryption. A TLS
failure or pairing mismatch never triggers a plaintext fallback/reconnect.

Force senders start TLS immediately. For other updated peers, the sender reads
the public HELLO/profile and, if TLS is mutually enabled, sends empty frame
**22 START_TLS**. The receiver sends empty frame **23 TLS_READY**, fully flushes
it, then upgrades the same socket. The sender upgrades only after receiving the
acknowledgement. Extra buffered plaintext across this boundary is rejected.
Both repeat HELLO and CAPABILITIES inside TLS before any file offer; the
installation ID must match the pre-upgrade greeting. This does not authenticate
an unpaired identity. Force receivers may exchange these public negotiation
frames but reject every plaintext file/collection offer. Plaintext legacy
senders therefore fail before approval or file creation. Disable receivers
reject encrypted connections. Encrypted-policy busy listeners close additional
connections rather than leak application errors before a handshake.

TLS parameters: OpenSSL 3.5.9, TLS 1.3 only, TLS_AES_128_GCM_SHA256, standard
ECDHE. With an empty optional pairing key, modern apps use a per-user saved
P-256 key and self-signed certificate. Clients explicitly do not authenticate
that certificate: this is opportunistic encryption, vulnerable to an active
intermediary. Force prevents plaintext, not impersonation. Prefer permits
unauthenticated downgrade to retain retro interoperability.

With a configured 64-hex key, use the existing TLS 1.3 external PSK identity
BitSkim-0.1 and 32 key bytes. No certificate fallback is allowed for a paired
connection. Matching keys authenticate group membership, not names/IDs. Key
creation uses RAND_bytes. No early data, tickets or session cache are enabled.
The custom BIO retains native socket width and nonblocking I/O. Old TLS peers
without bit 4 retain immediate TLS for advertised endpoints and require a
matching PSK; automatic unpaired TLS/manual upgrades require updated peers.

### Resumable offer sequence

Only when both TCP HELLOs advertise resume may a sender use these frames.
All reserved flags stay zero and all existing length bounds apply.

| Type | Name | Payload / permitted state |
|---:|---|---|
| 10 | OFFER_RESUME | Same payload as OFFER; incoming after HELLO |
| 11 | ACCEPT_RESUME | 4-byte offset + 32-byte SHA-256 of retained prefix; sender after PREPARE_RESUME |
| 12 | RESUME_READY | Empty; receiver after ACCEPT_RESUME |
| 13 | PREPARE_RESUME | 4-byte retained offset; sender waiting for recipient decision |
| 14 | RESUME_WAIT | Empty; only while preparing/checking/waiting for prefix validation |

OFFER_RESUME still permits ordinary ACCEPT for a fresh non-resumable receive.
On explicit local acceptance for resume, the receiver sends PREPARE_RESUME and
hashes its retained prefix. It sends ACCEPT_RESUME when hashing completes. The
sender opens the source, independently hashes exactly that prefix and compares
it in constant time. A mismatch sends ERROR and stops. A match sends
RESUME_READY, then DATA beginning at the offset. Offset must not exceed the
offered total. Zero offset and an empty file are valid (SHA-256 of empty input).
Both sides seed the normal running CRC by reading the prefix, so END remains
the CRC of the entire original file. COMPLETE and collision protection are
unchanged. No DATA is permitted before RESUME_READY.

Prefix work is bounded to four 4096-byte blocks per tick. During these states
RESUME_WAIT is emitted at most once per second when the output queue is empty (CHECK_RECEIVE, PREPARE_SEND or CHECK_SEND only). A receiver in WAIT_RESUME listens but does not emit: the hashing sender supplies keepalives, ordered before RESUME_READY on TCP;
receiving it outside these states is an error. It cannot extend the initial
120-second user decision. The usual 30-second inactivity timeout applies;
local hashing progress and valid peer heartbeats count as progress.

Checkpoints are local, not wire job IDs. The same-directory metadata format is:
8 bytes `BSRSM01\0`, 32 ASCII sender-ID digits, 4-byte total, 2-byte filename
UTF-8 length, and exactly that filename (all integers big-endian). The data
sidecar size supplies the offset. Exact metadata equality, regular-file checks,
no symlink/reparse reopening, exclusive creation and no-replace finalization
protect ownership. Unknown or mismatched objects are preserved. Network loss,
process exit or failed validation retains matching owned checkpoints; explicit
cancel of an accepted transfer deletes them. See TRANSFERS.md for UI recovery
and the limits of plaintext prefix verification.

## Atari adapters

Atari clients emit the ordinary plaintext protocol-1 sequence; no TLS/resume capability is advertised. Queues use independent approved connections. Local Atari GUI paths are ASCII (8.3 on TOS), with explicit save-name editing for other offered filenames. STiNG uses a native-ABI adapter: passive TCP handles are rearmed on accept, passive UDP handles are rearmed after each datagram and source addresses are captured before release. Deferred API calls preserve cooperative behavior. MiNTNet uses nonblocking BSD sockets. No wire-version, frame type or existing client behavior changes are required. Native driver/kernel validation is pending.

## Negotiated batch offers (capability 0x04)

A sender snapshots the contiguous queued files for the same recipient when the head starts. There are 2–8 entries; later additions form a new offer. Only a receiving UI that can display the full remaining list opts in with `bs_batch_enable`. The sender checks the live receiver HELLO for bit 2 before using frame **15 (OFFER_BATCH)**; otherwise it sends the ordinary OFFER/OFFER_RESUME sequence for every file. Protocol version 1 and old frames are unchanged.

Frame 15 replaces the ordinary offer for that connection. Its payload is:

| Offset | Field |
| --- | --- |
| 0 | 16-byte opaque batch identifier (not authentication) |
| 16 | Current zero-based item index, one byte |
| 17 | Total entry count, one byte (2–8) |
| 18 | Reserved flags, one byte, must be zero |
| 19 | Repeated entries: u32 size, u16 UTF-8 name byte length, name bytes |

Integers are big-endian. Every name has 1–255 bytes, valid portable UTF-8 with no embedded NUL, directory separators or traversal names; every size is at most 2,147,483,647. The entire payload must match the declared entries exactly. Maximum payload is 2,107 bytes. The full list is repeated on each file connection and validated before any offer callback or file creation. A source whose basename or size changes after the snapshot pauses the sender until the user clears/requeues it. Each file still uses ordinary ACCEPT/DATA/END/COMPLETE and CRC verification. Initial batch flags do not support resumable offers/checkpoints; TLS remains available according to the normal transport policy.

The receiver can decline, accept the current file through the existing API, or explicitly approve all remaining entries into one folder with `bs_accept_batch`. Accepting one file never grants the remainder. Accept-all retains the approved manifest and destination folder. Each destination is resolved just before that file is accepted, preserving existing regular files by collision numbering; final commit remains no-overwrite, including races and existing directories. A conflicting object that cannot be numbered safely or a finalization race fails the transfer without replacing it. Platform filename adaptations are shown before consent. No sender-provided path is used. The base flat collection has no paths; directory collections use the negotiated tree extension below. Automatic unpacking remains unsupported.

Retained consent is valid only for the same peer ID, source IPv4 address, advertised port, transport encryption state, batch identifier, complete ordered manifest and exact next index. The next index advances only after successful checksum validation and no-overwrite commit. Consent expires after 120 seconds between files (long active transfers are allowed), at the end of the list, on errors/cancellation/decline, when incoming reception or batch receiving is disabled, on restart, or when another offer does not match. An unexpected or reordered valid manifest requires fresh consent. No implicit retry is introduced. In plaintext, IDs and tokens are unverified network labels; this extension does not authenticate a sender.

The incoming progress UI can use `bs_batch_index()+1` and `bs_batch_count()` to show file position/remaining count. The ordinary BSStatus layout, numeric Amiga ARexx STATUS tokens and existing commands remain unchanged. Receivers without the feature continue one approval per file.

### Streamed collections (capability 8)

The version remains 1. Frames remain at most 4,096 payload bytes. All new integers
below are unsigned big-endian u32 unless stated otherwise. A receiving UI opts in
with bs_batch_enable only after it can browse a complete paged/scrolling list.
Bit 3 is receiving capability, not a requirement that a sender display incoming lists.

| Frame | Payload | Meaning |
| --- | --- | --- |
| 19 COLLECTION_REF | token[16], current index u32, total count u32 | Sent after HELLO; reuse an exact retained collection approval. |
| 20 COLLECTION_NEED | empty | Receiver has no matching approval; request the complete list on this connection. |
| 16 COLLECTION_START | token[16], current index u32, total count u32 | Start an immutable full-list transmission after NEED. |
| 17 COLLECTION_ITEM | size u32, name length u16, UTF-8 name bytes | Exactly one ordered entry, with the same name/size rules as frame 15. |
| 18 COLLECTION_END | empty | All declared entries have arrived and temporary storage is flushed. Only now notify the receiving UI. |

Counts must be 2–32,760 and index must be less than count. Missing, excess,
out-of-state or malformed frames terminate the connection, revoke consent and
never prompt for a partial list. List storage failure does the same with a useful
error. The engine stores one fixed-size entry at a time and limits frames/tick;
metadata receives the usual 30-second inactivity timeout. The 120-second decision
timeout starts once the list is complete, rather than during transmission.

REF reuses only a retained immutable manifest with matching token, count, exact
next index, installation ID, source IP, advertised port, transport state and
120-second inter-file lifetime. It never supplies replacement names. Missing or
expired consent causes NEED, full-list transfer and a new approval. START always
revokes prior consent. Tokens are scoped labels, not authentication; plaintext
remains unauthenticated. Later connections normally carry only REF, avoiding
retransmitting the full list for every file. Each file keeps ACCEPT/DATA/END/COMPLETE,
CRC checking and no-overwrite commit. Collections do not enable resume checkpoints;
TLS remains available on capable clients. No active-file PAUSE frame is introduced.

Outgoing paths and manifest snapshots are temporary-file records. Preparation
checks at most 16 files per tick. Appended files cannot enter an already started
snapshot. The receiving API loads one entry on demand: bs_batch_item returns a
cache pointer valid only until the next item lookup on that client. Approval keeps
one folder, not one destination path per entry. Collision numbering occurs when
each file starts; a final commit race fails without overwriting. Ordinary numeric
ARexx STATUS fields and public BSStatus layout remain unchanged.

Compatibility: frame 15 and capability 4 retain their original 2–8-entry bounds.
A new sender uses that format for a small selection to an old batch peer; larger
selections fall back to individual offers. A new receiver continues to accept
both old single-file and eight-entry offers. Unknown capabilities do not enable
new frames. The local count ceiling reflects signed 16-bit UI/API counts and is
not a network payload allocation size.

### Transfer speed limits

Each client provides separate send and receive limits in its Network preferences (KiB/s). Zero means Unlimited; the maximum is 1,048,575 KiB/s. Settings apply to the next connection/file. Limits cover file payload, not discovery, control messages or encryption overhead. Small bounded bursts are permitted. A sender uses the lower of its own upload limit and the recipient’s download limit. Limited reception requires an updated sender; legacy senders are refused before approval when a receive limit is set. Unlimited transfers remain compatible with older clients. DOS also accepts `LIMITS <sendKiB> <receiveKiB>`.

Rate negotiation uses profile capability bit 256 (not the short HELLO suffix). After both CAPABILITIES responses, capable peers exchange frame 24 LIMITS: two unsigned big-endian 32-bit byte-per-second values, send then receive; zero is unlimited. Both values must be at most 1,073,740,800. The exchange repeats after a TLS upgrade. Offers follow this exchange. No new frames are sent to peers without the capability. Token credit starts at zero, accumulates using a monotonic clock, and is bounded to the larger of one data chunk or 250 ms of traffic.

### Skip and written-file verification

BXC1 directional feature bit 512 negotiates `SKIP_FILE` (frame 25, zero-length payload). The receiver may send it only during a pending file decision. The sender treats it as a terminal **skipped** result (5), advances that queue item and preserves collection ordering/consent. No payload is sent. Receivers must not send it to legacy senders lacking the bit; they leave the decision pending with an explicit error instead.

Feature bit 1024 negotiates written-file readback verification. After END CRC/size and close succeed, the receiver reopens the temporary file and recomputes CRC/size in bounded slices before committing and sending COMPLETE. While checking, it can send `VERIFY_WAIT` (frame 26, empty) approximately every two seconds. Only a negotiated sender in FINISHING accepts this frame; ordinary I/O timeout tracking remains active. Without the bit the old streamed CRC/receipt exchange is unchanged. No new END/COMPLETE payload is introduced.

Feature bit 2048 indicates configured persistent certificate/device trust in unpaired TLS mode. Pairing uses the existing TLS connection, not a new public network command. Both computers can compare 6 bytes (48 bits) from SSL_export_keying_material with label `EXPORTER-BitSkim-Pairing-v1`, no context, rendered XXXX-XXXX-XXXX uppercase hex. Explicit local approval stores the peer X509 public-key SHA-256 digest against its installation ID. New certificate-mode clients request and supply certificates in both TLS roles. Unknown self-signed certificates require code comparison for trust; a remembered pin is checked before offers. See TRANSFER-TOOLS.md for storage, compatibility and security limits. The shared-key PSK mode is unchanged. Discovery carries capabilities, never private keys or trust pins.

Wire/app version remains 0.1. ARexx STATUS retains its existing decimal key=value fields; terminal detail/event RESULT may now be 5 for a negotiated skipped file. The new local history APIs are separate from the existing ARexx event ring.

## Negotiated directory collections (capability 64)

Both endpoints must negotiate DIRECTORIES (64) and streamed COLLECTION support.
TREE_START (27), TREE_ITEM (28) and TREE_REF (29) mirror COLLECTION_START,
COLLECTION_ITEM and COLLECTION_REF respectively. End/need use COLLECTION_END
(18) and COLLECTION_NEED (20). TREE_ITEM retains the collection item's unsigned
32-bit size, unsigned 16-bit UTF-8 name length and name bytes. A relative name
ending in `/` denotes a directory with size zero; other entries are regular
files. A tree may contain just its empty root directory.

The first entry is the root directory. Entries are depth-first with each parent
declared before its children; all paths stay under that root. Paths are at most
255 UTF-8 bytes and 16 components. Absolute paths, empty components, traversal,
backslashes, colons and unsafe/control characters are refused before approval.
The manifest remains disk-backed and scoped to its token and peer. It is never
sent to a peer lacking the negotiated capabilities; there is no flat fallback.

Explicit accept-all starts bounded destination preparation in a fresh unique
root. TREE_WAIT (30), with empty payload, is emitted at most once every two
seconds while preparing; a negotiated tree sender in decision state refreshes
its decision timer. Once prepared, a directory entry receives COMPLETE directly
in decision state, with no ACCEPT/DATA/END. Regular entries retain the ordinary
file transfer and checksum sequence. Tree references resolve only within the
approved collection. Local preparation errors refuse the transfer; created
folders and completed files are retained. Native paths, filename adaptations and
free-space estimates are local policy and are not transmitted as authority.
