# NovaDock integration — BitSkim 0.1 / ARexx protocol 2

Project repository: [retrocs-org/bitskim](https://github.com/retrocs-org/bitskim). Website: [bitskim.retrocs.org](https://bitskim.retrocs.org).

Implemented in BitSkim, not a NovaDock plugin binary. NovaDock's existing STATUS/SHOW/REFRESH/RECEIVING/PREFS/CANCEL integration can remain unchanged. All old commands remain available. VERSION still returns **0.1**; the independent ARexx extension version is **AREXX=2** from CAPABILITIES. Hosts predating these extensions return RC 10 for CAPABILITIES; keep the old polling behavior in that case.

The host is BITSKIM or BITSKIM.1 through BITSKIM.99. This is local ARexx only; no network management endpoint is added. Use asynchronous requests, at most one in flight per plugin, and request RXFF_RESULT for success responses. Do not release a timed-out RexxMsg or its arguments until its reply has arrived. A five-second plugin deadline indicates an unresponsive/busy UI, not proof that a transfer failed. The running client uses modeless offer, save-path, send-path and notice windows. Its main loop continues bs_tick and rexx_poll while they are open. STATUS/DETAILS/LAST/CANCEL/SHOW/HIDE remain serviceable. The offer is tied to its transfer serial and is closed on cancellation/timeout, preventing late acceptance. Startup errors before the ARexx host exists and the independent preferences editor are separate from this event loop.

## Recommended plugin flow

1. Discover the host and query CAPABILITIES, SESSION and STATUS. Treat unknown capability/STATUS fields as optional additions.
2. Poll STATUS for transfer percentage and BPS. All STATUS values are decimal integers; no filenames, error text or session strings are appended there.
3. When available, poll EVENTDETAILS with your own sequence cursor. Compare its SESSION every time; a change invalidates the cursor. On a change or LOST=1, resynchronize with STATUS and LAST, then adopt LATEST. Do not try to reconstruct missing notifications.
4. Query LAST to show a completion/error/cancelled/declined badge. Deduplicate by its qualified ID. LAST remains available while idle and during the next transfer; a new terminal result replaces it. It is not a multi-transfer result queue.
5. For a future recipient picker, get PEERS, retain IDs, and fetch PEERINFO by ID. Never use a remembered PEER index for a send. SENDID resolves the ID's current address/port within BitSkim when the command runs, and checks the remote HELLO identity before sending an offer. A stale address occupied by a different advertised ID fails without offering the file. Duplicate catalog IDs are rejected as ambiguous; use a deliberate manual-address send or resolve the duplicate configuration. Host restart, rediscovery or a copied installation can change catalog membership; IDs identify advertised installations, not authenticated people.

The iconified optimization clears the peer catalog and pauses searches. PEERS/STATUS therefore report zero nearby peers; SENDID then returns UNAVAILABLE. SHOW resumes discovery; wait for the desired ID to reappear before sending. Manual-address SEND and incoming transfers continue to work while iconified. REFRESH alone does not override the hidden-mode search policy.

## Commands

| Command | Success response |
|---|---|
| CAPABILITIES | AREXX=2 plus feature flags, FORMAT=PCTBYTES, MAXCOMMAND=1199, MAXRESULT=4095, HISTORY=32 |
| SESSION | SESSION=32-lowercase-hex-digits |
| STATUS | Existing ACTIVE INCOMING AWAITING ICONIFIED RECEIVING VISIBLE PORT PEERS BYTES TOTAL RELOAD fields, followed by BPS |
| DETAILS | Current/most recent started transfer snapshot, or AVAILABLE=0 |
| LAST | Most recent terminal transfer snapshot, or AVAILABLE=0 |
| PEERS | COUNT=n ID0=32hex ID1=32hex ... |
| PEERINFO 32hex | ID=32hex PORT=decimal NAME=encoded PLATFORM=encoded ADDRESS=encoded |
| SENDID 32hex "Amiga:path" | ID=qualified-transfer-id; starts asynchronously |
| EVENTDETAILS [sequence] | LATEST=n LOST=0-or-1 SESSION=32hex followed by event entries |
| ERROR | CODE=ASCII_identifier TEXT=encoded; diagnostic for the most recent failed command |

Legacy EVENTS [sequence] retains exactly its LATEST/LOST and sequence:type syntax. Legacy PEER index remains IPv4:port followed by a name; it is not an escaped machine-detail format. Legacy SEND IPv4[:port] "path" still returns OK when accepted. The remaining existing management commands return OK.

BPS is the existing engine's recent payload rate in **bytes per second**, excluding protocol overhead. Zero means inactive or no measured payload rate yet (including offers, initial sampling and stalls). It is not a delivery acknowledgement and can briefly retain the most recent active sample during a stall. Read ACTIVE and transfer RESULT separately. STATUS fields remain readable during normal transfer activity.

### Transfer details

DETAILS and LAST use:

```
AVAILABLE=1 SESSION=32hex ID=32hex-8hex ACTIVE=0 INCOMING=1 BYTES=123 TOTAL=123 RESULT=1 FILENAME=hello%20world.txt PEER=My%20Mac ADDRESS=172.16.1.163 ERROR=
```

ID is an opaque qualified transfer identifier, not a wire-protocol ID. INCOMING=0 means sending, INCOMING=1 receiving. RESULT is 0 pending, 1 complete, 2 failed, 3 cancelled, 4 declined. LAST is a frozen terminal snapshot with ACTIVE=0. DETAILS follows the current transfer and may briefly show a terminal result with ACTIVE=1 while a final response drains. A receive result of complete means checksum verification and local save succeeded; the sender's final acknowledgement may still be lost. Local cancellation sends a CANCEL frame before closing, so the remote endpoint normally reports cancelled; it can still report a failure if that frame could not be written. Receive-side checksum, write and finalization failures report RESULT=2 (failed).

A transfer starts when an outgoing bs_send succeeds or an incoming TCP connection is accepted. Early connection/handshake failures can therefore have empty incoming filename/peer fields. A rejected SENDID (busy, unavailable, invalid file/path) creates no transfer ID and does not replace LAST. IDs distinguish consecutive transfers even with identical names and sizes. Results survive until another transfer finishes or the process exits; they are not saved to disk. A successful command means accepted locally, not delivered.

### Escaping and encoding

Machine detail responses consist of whitespace-separated KEY=VALUE tokens. Keys are uppercase ASCII. Integer values are unsigned decimal; IDs and session tokens are lowercase hexadecimal (plus the ID's hyphen). Values never contain literal whitespace or quotes.

Text fields use byte-wise percent encoding: ASCII A–Z, a–z, 0–9, hyphen, period, underscore and tilde are literal; every other byte is %HH with uppercase hexadecimal digits. Empty text is an empty value after =. Percent-decode **once**, and do not treat + as a space. FILENAME, PEER, NAME and PLATFORM are UTF-8 from the protocol engine; ADDRESS is ASCII. ERROR and ERROR's TEXT are opaque diagnostic bytes: normally ASCII/UTF-8, but an untrusted remote reason can contain invalid UTF-8. Keep the decoded bytes or display invalid sequences with replacement; never use diagnostic text as a command. Embedded zero bytes are not representable in these C-string fields. No field expands environment variables.

Thus café "A" becomes caf%C3%A9%20%22A%22; a literal percent becomes %25. A literal space cannot introduce a second field. This preserves names without requiring the plugin to parse human-readable message text.

Command names, ON/OFF and hex ID case are insensitive. Arguments are whitespace separated. A single- or double-quoted argument can contain spaces; double the matching delimiter to include it literally. For example:

```
SENDID aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa "Work:Shared files/report.txt"
SENDID aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa "Work:Shared ""Old""/report.txt"
```

Backslashes are literal bytes, not escapes. There is no shell expansion, percent decoding or Rexx-variable expansion inside BitSkim's parser. The caller must construct the ARexx command correctly first. Native command path bytes are Amiga Latin-1, as in the existing client, not UTF-8. Quotes are removed by this command parser before bs_send receives the path. The resulting path is at most 1023 bytes; the complete command is at most 1199 bytes, excluding zero termination. Command responses are at most 4095 bytes, also excluding zero termination. Peer count is bounded by the shared engine (32). Names/platforms/filenames retain the core's 63/31/255 UTF-8-byte bounds; diagnostic strings are at most 255 bytes before encoding. Failed operations never return a silently truncated successful detail response.

### Return codes and sending policy

RC 0: query succeeded/action accepted. RC 10: unknown command, malformed/oversized argument, or extra arguments. RC 20: failed operation, busy, unavailable peer, UI failure or shutdown. Native ARexx failure replies have rm_Result2=0, not an Argstring pointer. Query ERROR immediately for a diagnostic CODE/TEXT; known codes are BUSY, UNAVAILABLE, SEND_FAILED, BAD_COMMAND and FAILED. ERROR is retained across successful queries and read without consuming it. It is host-global, so another application can replace it; RC and transfer IDs/results are authoritative, not this diagnostic slot.

SENDID never falls back to an index, another peer, or a manual address. It sends exactly one regular file, at most 2,147,483,647 bytes; directories and multiple path arguments are rejected. The filename must satisfy the existing portable filename policy, so some native leafnames require renaming. A busy engine rejects a second send without disturbing the first. CANCEL keeps the existing partial-file cleanup behavior. No queue, directories, multi-file batch, overwrite bypass or incoming-accept command is added. The receiver's normal approval/save policy still applies.

## Events, sessions and retention

EVENTDETAILS entries are sequence:type:qualified-id:result, or sequence:type:-:0 when there is no associated transfer (notably peer changes). Types retain the core values: 1 peers changed, 2 incoming offer, 3 status changed, 4 completed, 5 error. Result values are the same as DETAILS/LAST. Do not infer every decline from type 5: local decline can be a status event with RESULT=4. A terminal outcome may first appear on a status event before its completed/error notification.

Events are recorded in callback order with increasing unsigned 32-bit sequences, starting at 1. The newest 32 entries are retained, including progress/status events; rapid transfers can overflow history quickly. EVENTS/EVENTDETAILS return entries strictly newer than the supplied cursor, oldest first, without consuming history. Default cursor is 0. LOST=1 means the cursor predates retained history or is numerically ahead of the current generation. An old cursor of 0 reports LOST=1 once more than 32 events exist.

SESSION is generated at process startup from system time and task identity; it is an opaque local observation token, not a security credential. Before the sequence would exceed 4294967295, the session generation changes, history clears, and sequencing restarts at 1. Transfer counter wrap similarly changes generation before reusing a counter. Active and retained transfer IDs keep their original qualified ID across a generation change. On process restart, current/last snapshots and history clear and a fresh session base is generated. Consumers must compare SESSION even when LATEST happens to match their old cursor. These tokens are not authenticated or cryptographic identities.

## Verification and remaining checks

Host tests compile the actual portable ARexx dispatcher and observer with controlled engine/UI substitutes, including legacy responses, numeric STATUS, hidden queries, active progress, result retention, names/quotes/non-ASCII byte encoding, ID lookup under peer reorder/removal, independent cursors, overflow and sequence/restart detection. Socket tests exercise the actual shared engine's serial/outcome lifecycle, completion, decline, cancellation and timeout. Both Amiga programs cross-build.

Actual ARexx message exchange, native UI/requester timing and the NovaDock plugin's end-to-end behavior still require Amiga runtime validation. No NovaDock source or binary is included or modified here. Modeless window behavior, font/layout, concurrent SHOW/HIDE and cancellation still require native Amiga runtime checks. Network setup tools are maintained separately.

Negotiated receiver-side skips use terminal RESULT=5. Existing STATUS remains numeric and unchanged. The local eight-entry transfer history is distinct from the 32-entry ARexx event history.
