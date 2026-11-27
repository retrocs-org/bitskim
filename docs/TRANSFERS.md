# Queues, optional encryption and resume — BitSkim 0.2

## File queues

The shared engine stores pending paths and incoming file lists in temporary files,
then streams one file at a time over a separate connection per file. Updated
receivers browse the complete collection, approve it once and choose one folder.
Memory use does not grow with the list. The clients checkpoint outgoing paths and
the last eight transfer results in private local storage. Restored queues start
paused: review the recipients/files, then choose Continue. Nothing is resent
automatically. Pending mobile imports remain available after app restart. Temporary
storage follows the platform C runtime (for example T: on Amiga); on Amiga T: is
usually RAM-backed, so very large selections still consume available system RAM.
A full/unwritable temporary volume reports an error and does not silently drop files.
Folder collections are supported on capable receivers. Automatic archive creation is not provided.

macOS and Windows 10 support multiple selection and multiple file drops.
Amiga and classic Windows accept multiple file drops, or repeated individual
sends. DOS: Space tags files in the file pane; F5 queues the tagged files for the
selected computer. F9 opens queue controls. RISC OS queues repeated file paths
or Filer drops; its native runtime remains untested.

Failures, declines and cancellation pause the current item and retain its queue
entry. **Continue / Retry** retries that item, **Skip** removes the idle head,
and **Clear waiting** removes waiting items without cancelling the current file.
Pause takes effect after the current file. Explicit Cancel stops that file.
A failed or lost final acknowledgement may mean the file was already delivered:
check the receiver before retrying. There is no automatic retry.

Discovered recipients are stored by stable ID, resolved again at send time and
verified in HELLO. A missing or ambiguous ID pauses the queue rather than
selecting another recipient. Address-based connections explicitly target that
address and port. Names and IDs are not authenticated identities.

## Mobile folder sending

iOS **Files → Choose Folder** and Android **Files → Choose Folder** use the platform's native folder picker. Enabled providers must grant readable directory access. BitSkim copies the tree to private staging without modifying provider originals, reviews nested paths/empty directories and sizes, then requires explicit confirmation of the recipient. The staged contents are checked again before queueing. A changed or unreadable selection is rejected. Cancelling selection or review sends nothing.

Selections allow at most 32 roots and 32,760 file/directory entries, 2 GiB minus one byte per file, portable relative paths of 255 UTF-8 bytes and at most 16 path components (directories below level 16). Links, special files, loops and unsupported names fail preparation. Cloud folders may need a local download or individual file selection. Staged roots stay owned by the entire queue, so completing one directory or file cannot delete later children. Restored work remains paused until explicitly continued. The Share extensions continue to import regular files.

## Encryption on capable clients

macOS arm64 and Windows 10 x64 use statically linked OpenSSL 3.5.9 for
TLS 1.3. New configurations default to **Prefer encryption**. Explicitly saved
settings, including Disable, are retained. Retro builds report plaintext-only
support and remain interoperable when the modern peer allows plaintext.

- **Disable encryption**: send and accept plaintext files only.
- **Prefer encryption** (default): use the live handshake to encrypt automatically
  when both clients enable TLS, including manual-IP connections. Otherwise use
  plaintext. A TLS or pairing failure never causes a plaintext retry.
- **Force encryption**: require TLS for every file in both directions. Reject
  clients with encryption disabled or unavailable; never fall back to plaintext.

No key copying is required for automatic encryption. Without a pairing key, TLS
uses ECDHE and a per-user self-signed EC certificate on the modern apps. Before a device is explicitly paired, certificates
are not verified as identities. This protects against passive observation, not
an active impersonating intermediary. Force guarantees encrypted transport, not
peer identity. Names/installation IDs remain labels. Discovery is always public.
Prefer permits plaintext compatibility for unknown devices. Remembered device IDs require a matching encrypted public key, preventing plaintext downgrade for those IDs.

For authenticated pairing, optionally generate a random **64-hex-character key**
and copy it privately to trusted computers. Both ends must use the same key;
when TLS is mutually enabled, a mismatched or missing key fails closed, even in Prefer mode. This retains the
existing TLS 1.3 external-PSK + ECDHE mode. Paired computers authenticate group
membership, not an individual device name. Old TLS clients require this matching
key; they do not implement the new automatic certificate mode or manual-IP upgrade.

TLS uses AES-128-GCM/SHA-256 with no early data, session tickets or session cache.
The optional key is saved in macOS account preferences or the Windows user's
AppData INI, not an OS credential vault. Encryption settings cannot change while
files are active or queued. Actual transfer indicators describe the connection.

The initial public HELLO/profile exchange can negotiate a TLS upgrade, followed
by a fresh HELLO/profile exchange inside TLS. File names, lists and contents are
not offered until negotiation finishes. Force senders start TLS immediately;
Force receivers may exchange public negotiation frames but reject plaintext file
offers. See PROTOCOL.md for exact states, framing and compatibility rules.

## Resuming interrupted transfers

Resume defaults on in the modern macOS and Windows clients and only negotiates
when **both** peers advertise it. It works over TLS or plaintext. Retro clients
use the existing non-resumable transfer sequence; their ordinary incomplete
files are removed.

Accepting a resumable receive creates two same-directory sidecars:
`destination.bsk-part` and `destination.bsk-resume`. The metadata binds the cache
to the sender ID, offered basename and exact total size. Network interruption,
process exit and errors retain this cache. Explicit cancellation of an accepted
transfer removes its owned cache. Declining a later offer leaves previously
retained files untouched. Unknown, malformed or mismatched sidecars are never
silently deleted or adopted.

Resend the **same file**, select the **same destination**, and choose **Resume**.
Both sides read the retained prefix in bounded blocks and compare its SHA-256
before sending the remainder. A mismatch stops the transfer; choose **Restart**
to explicitly discard that matching checkpoint and start afresh, or choose a
new destination. No entire file is buffered. The final CRC-32 still covers the
whole file; the destination is committed only after size/checksum verification,
without replacing an existing file. Completion is acknowledged as before.

Checkpoint files are local data, not authentication credentials. Prefix hashing
over plaintext does not defend against a malicious network peer; use Force encryption and a matching pairing key
 when authenticity of transferred bytes matters. The current implementation
uses sender ID/name/size plus prefix validation, not a persistent remote job ID
or a durable queue. Moving/renaming files or changing client IDs can prevent
resume. Checkpoints occupy disk space until resumed, explicitly restarted or
manually deleted. Very long basenames may require a shorter destination name
so the sidecar suffix fits the filesystem component limit.

## Pausing a queue

“Pause after file” lets the current transfer finish, then holds the waiting files. “Continue queue” can remove that hold while the current file is still transferring. It does not pause the current file on any platform. Active-file pause is not implemented; cancellation and interrupted-file resume are separate features. The Mac peer tile tooltip shows advertised TLS and resume capabilities. Actual transfers still negotiate capabilities in HELLO. Older peers use basic transfers.

## Consistent compact progress

Clients use a single per-file progress indicator, current filename and telemetry, plus the known outgoing waiting count. The active send is excluded from that waiting count. Outgoing FIFO ordinals advance after completion/skip and restart when a new run begins. Negotiated incoming batches show their current file position and remaining count; older single-file offers have no remote total. Optional queue operations stay in menus or existing DOS/Atari command views.

## Accepting a batch

When several files are queued together for one recipient, updated clients show their full remaining list and sizes. Choose Accept all (or Yes on Windows) and one existing destination folder. Subsequent files in that exact manifest arrive without another prompt. This does not enable permanent auto-accept. Existing auto-accept/remembered-folder preferences still apply. Amiga lists adapted local names; DOS/old FileCore/TOS may shorten or map filenames and collision names. Existing files are preserved by default; Transfer tools offers explicit Skip/Replace policies.

Mac uses a folder chooser; Amiga uses the batch dialog's Save all in folder field. Modern Windows uses its folder chooser. Classic Windows uses its existing Save dialog to select the folder and explicitly ignores the filename field. DOS F6 opens one folder prompt; CLI uses `ACCEPTALL <folder>` (the single-file `/RECEIVE` automation remains per-file). RISC OS and Atari show modeless lists with an editable folder field and Accept all. Native visual/driver verification on the experimental platforms remains pending.

Cancelling or failing a file stops automatic batch acceptance and pauses the sender. Explicit retry may ask for consent again. Files added after the initial list was sent require a new offer. Older batch peers restart interrupted files. Updated trusted encrypted peers offer an explicit Resume existing folder choice after fresh approval; completed files and retained prefixes are verified before remaining data is received. Individually offered modern-client files keep their negotiated resume support. TLS works for batches on capable clients. See PROTOCOL.md for the scope and timeout rules.

## Queue storage and practical limits

All clients use the same disk-backed format and a portable ceiling of 32,760
pending files, including the active queued file. This keeps counts representable
in the signed 16-bit interfaces used by the oldest clients; it is not a measured
performance limit. Temporary disk capacity and filesystem limits may be reached
first. Each stored path record uses roughly 1.1 KiB and each manifest entry roughly
264 bytes, independent of file contents. An incoming list stores entries only;
acceptance retains its folder and scoped consent rather than an array of paths.
The engine prepares at most 16 outgoing entries per tick and sends bounded
metadata frames. No file content is buffered as part of the queue.

One recipient's selection forms one immutable collection. Lists are browsed eight
rows at a time on retro clients, with Previous/Next on Amiga and Windows, PgUp/PgDn
on DOS, Previous/Next on RISC OS and Up/Down on Atari. macOS uses a scrolling table.
Only visible rows are loaded. The native Windows file picker has a 65,536-character
selection buffer; larger selections can use file drops or be added while paused.
This picker buffer is separate from queue capacity. Retro file choosers may have
their own selection limits; repeated additions while paused join the next snapshot.
Files added after snapshot preparation starts form a later collection and need
fresh consent. Different recipients never share approval.

Live HELLO negotiates streamed-collection support (capability 8). Both updated
clients show one full list and ask once for the folder. Peers supporting only the
older eight-entry protocol retain that behavior for selections up to eight;
larger selections safely use individual offers. No unlisted file gains consent.
Native memory, temporary-file behavior and performance on experimental hardware
still require runtime validation.

### Transfer speed limits

Each client provides separate send and receive limits in its Network preferences (KiB/s). Zero means Unlimited; the maximum is 1,048,575 KiB/s. Settings apply to the next connection/file. Limits cover file payload, not discovery, control messages or encryption overhead. Small bounded bursts are permitted. A sender uses the lower of its own upload limit and the recipient’s download limit. Limited reception requires an updated sender; legacy senders are refused before approval when a receive limit is set. Unlimited transfers remain compatible with older clients. DOS also accepts `LIMITS <sendKiB> <receiveKiB>`.

See [Transfer tools](TRANSFER-TOOLS.md) for destination-name adaptation, Rename/Skip/Replace decisions, negotiated disk verification, diagnostics, saved recent history and verified device pairing.

See [remembered folders, overall progress, cancellation and connection checks](TRANSFER-EXPERIENCE.MD) for current client behavior.


## Version 0.2 verification and diagnostics

Incoming files are checked against the sender's streaming CRC32, closed, reopened and read back before the temporary file is committed. This readback also applies to legacy senders; legacy peers receive the same completion frames as before. Readback work is bounded per event-loop tick. CRC32 detects accidental corruption; it does not authenticate a sender or protect plaintext transfers from deliberate modification.

History distinguishes written-file verification from unconfirmed receiver disk verification and incomplete transfers. A new sender cannot claim that an older receiver performed readback unless that receiver advertises the disk-verification capability. Folder entries are not reported as verified file contents.

Transfer tools show whether discovery searches are paused, probe attempts, valid peer packets and the age of the last peer packet. Probe counts record attempts, not confirmed network delivery. The existing connection test shows a local route address when available. Guest-network isolation, VPN routing and broadcast filtering can prevent discovery while a direct IPv4 connection still works.

Modern peer details show receiving state, folder support, filename limits/mapping, written-file verification and encryption policy. These are discovery hints; capabilities are checked again on connection. Unknown profiles remain explicitly unknown. Retro transfer-tools diagnostics report negotiated compatibility where available.

## Saved work, QR invitations and retro metadata (0.2)

Outgoing queues restore paused after restarting. Use the existing Continue/Retry, Skip and Clear controls to review and manage them. Quitting interrupts the active file; the queue retains its source path for an explicit retry. Completed collection entries are not queued again, but a restarted collection requests fresh consent and may get a new safely renamed root. This is not mid-file pause or automatic collection resume. A corrupt or incompatible local checkpoint is retained and reported in Diagnostics; it is never interpreted as a transfer command. Close BitSkim and move the checkpoint aside if you want a clean state. Checkpoint files contain local paths and recent filenames, not pairing passwords or private keys. They are local ABI-versioned files, not portable exports.

State locations: Mac Application Support/BitSkim/transfers.dat; iOS Application Support/transfers.dat; Android private files/Trust/transfers.dat; Amiga ENVARC:BitSkim/BSTRANS.DAT; RISC OS !BitSkim.Transfers; Windows, DOS and Atari BSTRANS.DAT beside settings. Mobile outgoing staging is private durable app storage. Forced process termination can leave the latest incomplete checkpoint unsaved; the previous complete checkpoint remains available. Never remove staging files needed by a restored queue.

Mac Transfer tools → Invite a phone with QR, Windows More → Connection QR, and the mobile Clients tab can display an invitation. Select an address reachable from the other device. Scan it in the mobile client (or paste a bitskim:// invitation on iOS), then select/review files. Camera access is requested only for scanning. A code expires after five minutes; generating another replaces the previous invitation, and closing the receiving app revokes it. The sender checks the advertised device ID and TLS certificate fingerprint before offering any file. The receiver checks the invitation’s random token and original expiry. A QR code does not save a trust pin or approve incoming files. Certificate encryption is required; shared-key/plaintext connections use the existing connection controls instead. A queue held beyond expiry needs a fresh invitation.

Enable **Preserve retro metadata** on both clients before queuing a transfer. It is off by default. Amiga Preferences → Network includes attributes/comments and companion .info files; RISC OS Preferences → Network includes filetypes. Modern clients expose the option in Settings, Mac Transfer tools, or Windows More. Native Amiga protection flags and comments are restored only on Amiga; RISC OS filetypes only on RISC OS. Other hosts keep an exclusive `<filename>.bskmeta` companion bound to content size and CRC32, allowing an onward transfer with metadata enabled. Keep the file and companion together. Changed content makes the old companion ineligible for forwarding. Companions are never overwritten. Native restore failures fall back to the companion; an inability to preserve either is reported while the verified data file remains available.

When queuing a file or drawer with metadata enabled, an adjacent `.info` is added as a normal, visible file in the collection preview. Folder contents already include icons. Recognized .bskmeta companions are sent as metadata rather than duplicate collection files. Safe conflict handling can rename payload and icon separately; review names after collisions. Only regular-file attributes are supported; directory attributes, timestamps, Mac resource forks and arbitrary RISC OS load/exec addresses are outside this extension. Clients without the negotiated capability receive normal file data. CRC32 detects accidental changes and is not authentication: use a trusted local network or verified TLS device identities.

On mobile, use a received file’s **Send to client** action for onward transfer with its adjacent metadata. Provider imports copy only user-selected files; an unrelated cloud provider cannot supply hidden companion files implicitly. Exporting/renaming a payload separately requires keeping its .bskmeta companion with the matching base name.

## Preserve file modification dates

Preservation is off by default. Enable **Preserve file dates and retro metadata** on both clients before queuing work. Use the Network preferences on AmigaOS, Atari and RISC OS, Settings on mobile, the preferences checkbox on Windows Classic, `METADATA ON` on DOS, or `--metadata` on the Linux CLI. Older clients and clients with preservation disabled receive normal file contents without the date extension.

The sender captures each regular file's modification time and sends it only if it remains unchanged when reading finishes. After verifying the written contents, the receiver restores the date on its temporary file before finalizing it. The completion result reports whether restoration succeeded. A date that the destination cannot represent does not discard verified contents; the result includes a warning. File-system resolution and the retro system's configured clock/time zone can limit precision. Access times, directory dates, ownership and foreign permission bits are not copied. File dates are descriptive metadata, not proof of when content was created.

This preference also controls the existing Amiga/RISC OS metadata companions. It does not affect release packaging: packaged archives continue to use the current packaging date.

## Recover after a transfer error

The app keeps the original technical error in Diagnostics and adds a **Next:** suggestion to user-facing errors. Network failures offer Test connection; a failed outgoing file can be retried from Transfer tools/history after checking the recipient. Mac error dialogs and iOS failure status expose the relevant actions directly. Windows Transfer tools shows the recovery guidance beside Retry outgoing, Test connection and Open folder. Retry sends a new offer and still requires approval; it does not bypass device trust, overwrite an existing file or reuse an unrelated previous transfer. For identity errors, compare the two devices before changing saved trust.

## Resume an interrupted folder

On updated modern clients, enable retained partial files and establish a trusted encrypted connection (remembered device identity or matching shared key). Re-send the folder, then explicitly choose **Resume existing folder** at the receiver and select the parent of the previous root. Linux CLI uses `receive --resume-folder --directory PARENT` and still prompts unless `--accept` is explicitly supplied. A fresh manifest approval is required.

Completed files are checked against the sender with SHA-256 prefix validation and CRC32 disk readback. Partial files are validated before appending. Different contents, sizes, unsafe paths or mismatched checkpoints stop the job and preserve originals. Files absent from the new manifest are kept. Cancellation removes the current partial file; a broken connection retains its checkpoint. Other files may have completed before a mismatch is encountered. Automatic acceptance never selects recovery mode on desktop clients. Without the negotiated extensions, normal safe receiving remains available.
