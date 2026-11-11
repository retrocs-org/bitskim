# Transfer tools (0.1)

## Open the tools

- macOS: the information button in the main window.
- Modern and classic Windows: **Transfer tools** in the window menu.
- AmigaOS: hold the right mouse button, then choose **Tools → Transfers / Diagnostics**. The window remains modeless; transfers and ARexx keep running.
- RISC OS: **Tools...** in the main window.
- Atari: **F1** opens the tools page; Escape returns to the main page.
- DOS: **F1**, then **T**. D selects diagnostics, H history, N/B older/newer entries, R retries an unsuccessful outgoing item, C cycles the conflict policy, and Page Up/Down scrolls. CLI equivalents: `DIAGNOSTICS`, `HISTORY`, `RETRY <one-based number>` and `CONFLICT RENAME`, `CONFLICT SKIP`, `CONFLICT REPLACE`.

## Filenames and collisions

Single-file offers and collection previews use the shared destination-name mapper. It retains a usable extension, substitutes unsupported characters, shortens names for classic filesystem limits, and avoids DOS/Windows device names. The default Amiga suggestion remains conservative at 30 bytes; OS version alone does not establish the destination filesystem's filename limit. DOS and STiNG use 8.3 names; old RISC OS FileCore uses ten-character leaves. Original wire names remain in the offer/history so a changed suggestion can be reviewed.

Incoming collection approvals also expose the conflict choice directly: use the popup on macOS/Windows, Conflicts on Amiga, the Existing button on RISC OS, C in the DOS offer, or F2 in the Atari offer.

The default conflict policy is **Rename safely**. A collision gets a numbered name with the extension retained. **Skip** leaves the existing file intact and advances the outgoing collection without broadening its approval. Skip requires the negotiated skip capability; a legacy sender gets a clear refusal and the user can rename or decline instead. **Replace** is an explicit session choice. Policies apply to the remaining files of an approved collection, so a separate decision is not needed for each collision. They reset to Rename when the application restarts.

Replacement never truncates the original while receiving: the new contents are written and checked first. Finalization moves the old file to an unused same-folder `BKxxxxxx` backup, commits the new file without overwriting a competing writer, then removes the backup. On commit failure the engine attempts to restore the original. If another writer prevents restoration, the original stays in the BK backup. A power loss during this short sequence may also leave that backup; this is recoverable replacement, not a cross-platform atomic exchange. Directories are not replaced. An explicitly selected Replace policy can replace earlier files with the same adapted name; choose Rename to retain every duplicate.

## Integrity and delivery

Every client retains the existing streamed IEEE CRC-32 and exact-length checks. When both clients advertise written-file verification, the receiver closes and reopens the temporary file, recalculates its CRC in bounded slices, and checks its length before finalization and the delivery receipt. A readback failure preserves any original destination and is reported to the sender. Capable senders accept verification-wait messages while this runs. Legacy senders retain their original CRC/receipt exchange, without the extra readback delay. CRC detects accidental corruption; it is not a cryptographic authenticity guarantee. Modern TLS protects transmitted records; device trust authenticates the remembered key.

## Diagnostics and history

Diagnostics show the selected socket stack, bind address, current local connection address where the socket adapter exposes it, ports, visibility/receiving state, payload caps, negotiated feature masks, file/name limits, peer endpoint and last error. DOS/Atari users should consult their stack settings for the local address. A bind address of all interfaces does not identify a LAN address. Nothing starts or reconfigures the stack, firewall, test system or bridge. Discovery across NAT/subnets may require manual addressing and port mapping.

The last **eight** terminal transfers are retained in a bounded, in-memory session history. It includes success/failure/decline/cancellation/skip, direction, filename, peer, bytes, source/save path and the result message. The history survives another transfer, but not application exit. Retry is for unsuccessful outgoing files and resolves the saved stable peer ID again; it never silently chooses another catalog entry. Source files are rechecked as new outgoing transfers. History is not a persistent queue or automatic resume. macOS and Windows can reveal the recorded file's folder without opening the received file itself. Files that were declined may have no save path.

## Verified device pairing (macOS and modern Windows)

New installations still default to Prefer encryption. They create a private per-user `identity.pem` containing a P-256 key and self-signed certificate:

- macOS: `~/Library/Application Support/BitSkim/Trust/`
- Windows: `%APPDATA%\BitSkim\Trust\`

While an unpaired TLS connection is active, both updated clients display a **XXXX-XXXX-XXXX** verification code. Compare the entire code on the two computers, or read it to the other person over a trusted channel. The sender's main status displays the code while awaiting approval. A manual incoming offer provides an explicit Accept & Trust / remember-device decision, and Transfer tools can accept the code read from the other screen. Trust on both computers if both should remember the identity. Never approve a mismatching code.

The code is a 48-bit TLS exporter comparison value, not a password or reusable key. It is derived with the application label `EXPORTER-BitSkim-Pairing-v1`; it is never used as a PSK. The saved `.pin` contains the peer public-key SHA-256 fingerprint, indexed by installation ID. Future connections check the saved key after TLS negotiation and before any offer. A changed key or plaintext downgrade for that remembered ID is refused. Unknown devices remain unverified; trusting a device does not grant automatic file acceptance. Display names remain user-controlled labels.

**Forget device** is an explicit removal of the current peer's pin, including after a changed-key refusal. Verify the real device and compare fresh codes before trusting it again. Do not distribute or include `identity.pem` in packages or source archives. Restore it from a private backup to retain an installation identity. If you intentionally replace it, other computers will need to forget and re-pair that installation. Corrupt/unreadable identity files are not silently regenerated. Shared 64-hex-key PSK mode remains available and authenticates membership in that key group rather than an individual device. Plaintext retro clients do not advertise device pairing.

The implementation uses OpenSSL's [TLS exporter API](https://docs.openssl.org/3.4/man3/SSL_export_keying_material/) and TLS CertificateVerify proof of private-key possession, with explicit local pin decisions rather than public-CA hostname validation. Automated tests cover code agreement, wrong-code refusal, persistence/restart, changed keys/pins and plaintext downgrade refusal. These checks are not an independent security audit.

## Folder transfers

Updated clients negotiate folder support before sending a folder. The folder,
its nested folders and empty directories form one paged collection. Approve the
list once and select a parent destination. BitSkim creates a **new root folder**;
an existing root is kept and a numbered name is chosen, even under Replace.
Files stream sequentially. Older clients receive ordinary file queues as before,
but cannot receive folder collections; folders are refused rather than flattened.

On macOS select files and folders together or drop them onto a peer. Windows has
Send folder in the menu and accepts folder drops. On Amiga drop a Workbench drawer
or enter its path. In DOS tag a directory with Space or use F5; Enter still opens
it for browsing. Atari accepts a folder path in Path. RISC OS accepts Filer folder
drops or a folder path. A single empty folder still requires folder approval.

Scanning and destination preparation advance in bounded slices. Temporary disk
spools hold the manifest, queue and destination map; the full list is not kept in
RAM. The queue supports at most 32,760 entries including directories. Each wire
relative path is at most 255 UTF-8 bytes and 16 components; native destination
path limits and the 2 GiB minus one byte per-file limit still apply. Names are
adapted component by component for the receiver. Links and special files are
refused. Permissions, timestamps, resource forks, filetypes and other metadata
are not preserved. Archives are not automatically unpacked.

Cancel stops listing or the active transfer. Completed files and created folders
remain; an interrupted folder is not recursively deleted. Retrying a remaining
folder collection creates a new safe root rather than merging into an earlier
partial root. Local filesystem changes while scanning or receiving can make the
operation fail; the checks do not provide an atomic filesystem snapshot.

## Disk space

Incoming prompts show estimated required bytes and available space when the
filesystem can report it. BitSkim checks before collection approval, before each
file opens its temporary destination, and before resuming an owned checkpoint.
Resume counts only the remaining payload. A low-space refusal leaves the offer
pending so another destination can be chosen.

The conservative estimate includes payload, one allocation unit plus 4 KiB per
file/directory, and a 64 KiB reserve. Collection totals use paired 32-bit words,
so totals above 4 GiB do not wrap on retro processors. Existing replacements and
files that may be skipped are not credited as free space. The estimate can
therefore refuse a collection that would fit after skips; choose another volume
or send fewer entries. Available space is a snapshot, not a reservation. Other
programs, filesystem overhead, temporary manifest spools and quotas can still
cause a write to fail; normal write and finalization checks remain active.

When querying space is unsupported, the prompt explicitly says it is unavailable
and reception may continue. RISC OS uses native OS_FSControl 55 with reason 49
fallback on older systems; known Net:/NetFS: paths report unavailable because
those calls do not reliably report NetFS free space. Native RISC OS runtime
validation is still pending. API references: [reason 55](https://www.riscos.com/support/developers/prm/fileswitchnew.html)
and [reason 49](https://www.riscos.com/support/developers/prm/fileswitch.html).

## Collection previews, results and retry

The approval prompt shows file count, directory count and total payload size
before choosing a destination. Counts describe the full manifest, including the
root and empty directories; when continuing an existing collection, the paged
list starts at the current entry. Disk-space estimates remain separate.

The main status shows aggregate results at the end of a collection or queued
run. Transfer tools also retain outgoing and incoming summaries for this
session. Completed payload files and completed directory entries are counted
separately; skipped, failed and cancelled/declined entries are distinct. The
incoming report tracks the latest manifest, and the outgoing ledger tracks the
latest queued run. Reports are bounded by the queue ceiling and explicitly
mark themselves incomplete if temporary storage fails. They do not survive a
restart. The ordinary eight-entry history remains available independently.

**Retry failed only** is in Queue (DOS: F9, then F; CLI: `QUEUE FAILED`). It
requires an idle, empty queue. Use Continue to retry the current paused entry,
or finish/clear the waiting queue before retrying recorded failures. Completed,
skipped, cancelled, declined and unattempted files are excluded. Nested retries
rebuild only the parent directories needed by the failed files and create a
fresh safe destination root. Folder entries that failed before sending any
payload need an explicit folder requeue. Check the receiver before retrying an
unconfirmed delivery, since a missing acknowledgement can hide a saved file.

Completion feedback is optional. Enable it in Preferences on Mac, Amiga, DOS,
Atari and RISC OS, or via the Windows menu. Classic Windows plays its system
sound; modern Windows uses its existing tray alert. Feedback is for successful
completion of a single transfer or the last delivered item in a queue or
collection, rather than one sound per file. Modern Windows keeps its existing
completion alerts enabled by default; sound options default off.

## Automatic acceptance permissions

macOS and modern Windows offer **Auto-accept only verified devices**, enabled by
default. With automatic acceptance on, this option limits unattended approval to
connections matching a remembered device public key. Unknown devices require a
normal explicit decision. Turn the option off only when automatic acceptance from
any sender is intended. Always ask where to save still applies in either mode.

macOS receive sheets are tied to the original offer and close when that offer
ends. Decisions from an old sheet are ignored after cancellation or replacement.
Adding outgoing items preserves an existing paused queue; Continue resumes it.
