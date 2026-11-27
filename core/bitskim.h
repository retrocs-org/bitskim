#ifndef BITSKIM_H
#define BITSKIM_H
#include <stddef.h>
#define BS_VERSION "0.2"
/* Build scripts supply a source fingerprint; direct compiler invocations are explicit. */
const char *bs_build_id(void);
#define BS_PORT 46440
#define BS_NAME_MAX 63
#define BS_PATH_MAX 1023
#define BS_MAX_PEERS 32
#define BS_FILE_MAX 2147483647UL
#define BS_CHUNK 4096
/* Disk-backed records; count fits the signed 16-bit interfaces of retro UIs.
   File data remains streamed separately. Disk exhaustion is reported on enqueue. */
#define BS_QUEUE_MAX 32760
#define BS_BATCH_MAX 8
#define BS_SECURITY_OFF 0
#define BS_SECURITY_OPTIONAL 1
#define BS_SECURITY_REQUIRED 2
#define BS_CAP_RESUME 1
#define BS_CAP_TLS 2
#define BS_CAP_BATCH 4
#define BS_CAP_COLLECTION 8
#define BS_CAP_DETAILS 16
#define BS_CAP_AUTO_TLS 32
#define BS_CAP_DIRECTORIES 64
#define BS_CAP_ACTIVE_PAUSE 128
#define BS_CAP_RATE_LIMITS 256
#define BS_CAP_SKIP 512
#define BS_CAP_DISK_VERIFY 1024
#define BS_CAP_DEVICE_TRUST 2048
#define BS_CAP_RETRO_METADATA 4096
#define BS_CAP_INVITATION 8192
#define BS_CAP_FILE_TIMES 16384
#define BS_CAP_SESSION 32768UL
typedef struct BSMetadata {unsigned long protection,filetype;unsigned char kind;char comment[160];} BSMetadata;

#define BS_CONFLICT_RENAME 0
#define BS_CONFLICT_SKIP 1
#define BS_CONFLICT_REPLACE 2
#define BS_RATE_KIB_MAX 1048575UL
#define BS_RATE_MAX (BS_RATE_KIB_MAX*1024UL)
/* Wire names are UTF-8. Local save-name policies: 0 UTF-8, 1 Latin-1
   with mapping, 2 DOS 8.3 mapping, 3 ASCII with mapping, 4 Windows ANSI code page. */
typedef struct {
    unsigned long send_features,receive_features,max_send_file,max_receive_file;
    unsigned short wire_name_bytes,local_name_bytes,path_bytes,code_page;
    unsigned char security_mode,name_policy,paired;
    unsigned long collection_files;
} BSCapabilities;
/* The core is single-threaded. Call all functions from the same event loop. */
typedef struct BSClient BSClient;
void bs_metadata_enable(BSClient *,int);
int bs_metadata_enabled(const BSClient *);
int bs_metadata_seed(const char *path,const BSMetadata *);
typedef struct {
    char id[33], name[64], platform[32], address[48];
    unsigned short port;
    unsigned long last_seen;
} BSPeer;
typedef struct {
    char name[64], platform[32], id[33], bind_address[48];
    unsigned short port;
    int visible, receiving;
    unsigned short discovery_port; /* zero: use port; app default BS_PORT */
} BSConfig;
typedef struct {
    int active, incoming, awaiting_decision;
    char peer_name[64], peer_address[48], filename[256], message[256];
    unsigned long total, transferred;
} BSStatus;
typedef enum { BS_EVENT_PEERS=1, BS_EVENT_OFFER, BS_EVENT_STATUS,
               BS_EVENT_COMPLETE, BS_EVENT_ERROR } BSEvent;
/* BS_EVENT_PEERS fires when a peer is added, removed or changes name, platform
   or port, not for refreshes that only renew last_seen. BS_EVENT_STATUS for
   progress is coalesced to at most one per bs_tick. */
typedef void (*BSCallback)(void *context, BSEvent event);
BSClient *bs_create(const BSConfig *, BSCallback, void *context);
/* Manual-only startup: no discovery probe or well-known UDP listener. */
BSClient *bs_create_manual(const BSConfig *,BSCallback,void *);
BSClient *bs_create_app(const char *name, const char *platform, const char *id, int visible, int receiving);
int bs_session_enable(BSClient *,int enabled);
void bs_destroy(BSClient *);
void bs_tick(BSClient *);
/* Service immediately available work with a 0..8 ms budget, checked between
   ticks, and a hard cap of 64 ticks. No socket waits; individual bounded file
   operations can exceed the time budget. Timer-driven modern UIs use this. */
void bs_service(BSClient *,unsigned long budget_ms);
/* Optional event-loop helper: block until an engine socket needs service, until
   a signal in sigmask arrives (AmigaOS; ignored elsewhere) or for timeout_ms
   (capped at 1000 so timers keep running), then call bs_tick. Returns at once
   when the engine has work that needs no socket. Returns the received Amiga
   signals (0 elsewhere). Clients driven by a periodic timer need not use it. */
unsigned long bs_wait(BSClient *, unsigned long sigmask, unsigned long timeout_ms);
void bs_refresh(BSClient *);
/* Pause searches/catalog updates, retaining discovery replies and file service. */
void bs_discovery_search(BSClient *, int enabled);
void bs_configure(BSClient *, const char *name, int visible, int receiving);
int bs_send(BSClient *, const char *address, unsigned short port, const char *path);
/* Resolve a unique catalog ID now and verify it in HELLO before offering data. */
int bs_send_peer(BSClient *, const char *id, const char *path);
/* Select one currently advertised endpoint without dropping HELLO identity checks. */
int bs_send_peer_address(BSClient *,const char *id,const char *address,unsigned short port,const char *path);
int bs_accept(BSClient *, const char *destination);
/* Session policy; applies to future approvals, including the rest of a batch.
   Replace moves an existing file to a unique backup until final commit succeeds. */
void bs_conflict_policy(BSClient *,int policy);
int bs_conflict_get(const BSClient *);
int bs_accept_conflict(BSClient *,const char *destination);
int bs_skip_incoming(BSClient *);
/* Receiving UIs opt in only after they can show the complete manifest. */
void bs_batch_enable(BSClient *,int enabled);
typedef struct { char filename[256]; unsigned long total; } BSBatchItem;
/* In negotiated folder manifests names are safe root-relative UTF-8 paths;
   a trailing slash denotes an empty-payload directory entry. */
int bs_safe_relative(const char *);
int bs_batch_is_tree(const BSClient *);
typedef struct {unsigned long required_high,required_low,available_high,available_low,allocation_unit;int known,enough;} BSStorage;
int bs_storage_info(const BSClient *,const char *folder,int collection,BSStorage *);
/* Query the destination volume without parsing display text. */
int bs_destination_space_info(const BSClient *,const char *destination,int collection,BSStorage *);
int bs_destination_space_report(const BSClient *,const char *destination,int collection,char *out,size_t capacity);
int bs_storage_report(const BSClient *,const char *folder,int collection,char *out,size_t capacity);
int bs_queue_scanning(const BSClient *);
int bs_batch_count(const BSClient *);
int bs_batch_index(const BSClient *);
/* One cached entry; pointer expires at the next item lookup on this client. */
const BSBatchItem *bs_batch_item(const BSClient *,int index);
int bs_batch_save_name(const BSClient *,int index,char *out,size_t capacity);
/* Explicit approval of the remaining manifest into one existing folder.
   Preserves existing files; does not change permanent auto-accept settings. */
int bs_accept_batch(BSClient *,const char *folder);
int bs_accept_batch_resume(BSClient *,const char *parent_folder);
int bs_batch_can_resume(const BSClient *);
/* Folder-recovery phases: 0 none, 1 preparing, 2 checking, 3 transferring,
   4 complete, 5 stopped. Counts include files only, excluding directories. */
typedef struct {int phase;unsigned long verified,resumed,received,remaining,total,checked_bytes,check_total;} BSRecoveryProgress;
int bs_recovery_progress(const BSClient *,BSRecoveryProgress *);
int bs_recovery_summary(const BSClient *,char *out,size_t capacity);

void bs_decline(BSClient *);
/* Cancels the entire local run, discards waiting entries and revokes batch approval.
   Explicit cancellation removes the active partial file, including resume caches.
   Completed files remain. Network interruptions may retain resumable data. */
void bs_cancel(BSClient *);
/* Small, fixed-size checksummed store beside the application preferences. */
int bs_folders_configure(BSClient *,const char *store);
const char *bs_last_folder(const BSClient *,int incoming);
/* Local FIFO; failures/cancellation pause the current item for explicit retry.
   Capacity includes the running queued item. Folders are scanned; no implicit retry. */
int bs_queue_peer(BSClient *,const char *id,const char *path);
int bs_queue_peer_address(BSClient *,const char *id,const char *address,unsigned short port,const char *path);
int bs_queue_address(BSClient *,const char *address,unsigned short port,const char *path);
typedef struct {char address[48],id[33];unsigned char pin[32],token[16];unsigned short port;unsigned long expires;} BSInvitation;
int bs_invitation_create(BSClient *,const char *address,char *out,size_t capacity);
int bs_invitation_parse(const char *,BSInvitation *);
int bs_queue_invitation(BSClient *,const char *invitation,const char *path);
typedef struct { char path[BS_PATH_MAX+1]; unsigned long total; int active,paused; } BSQueueItem;
/* Copy a bounded queue snapshot. Paths are local native encoding, not wire UTF-8.
   Total is the enqueue-time size; the engine rechecks before sending. */
int bs_queue_item(const BSClient *,int index,BSQueueItem *out);
/* Outgoing FIFO ordinal since it was last empty; skips count as processed.
   Newly appended items extend total; clearing waiting items reduces total. */
void bs_queue_progress(const BSClient *,unsigned long *position,unsigned long *total);
/* Waiting outgoing items, excluding an active queued send. */
int bs_queue_waiting(const BSClient *);
/* Portable signed-count ceiling, including the active queued item. */
int bs_queue_limit(const BSClient *);
int bs_queue_count(const BSClient *);
int bs_queue_paused(const BSClient *);
void bs_queue_pause(BSClient *);
void bs_queue_continue(BSClient *);
void bs_queue_skip(BSClient *); /* paused/idle head only */
int bs_retry_failed(BSClient *); /* idle, empty queue; failed regular files only */
int bs_batch_summary(const BSClient *,char *out,size_t capacity);
int bs_run_summary(const BSClient *,int incoming,char *out,size_t capacity);
int bs_completion_notice(const BSClient *,unsigned long *cursor);
void bs_queue_clear(BSClient *); /* preserves an in-flight item */
/* Pairing keys are 32 random bytes represented by 64 hex characters. */
int bs_security_available(void);
/* TLS builds default to Prefer. Empty key selects automatic unpaired TLS;
   a 64-hex key requires a matching PSK and never falls back after TLS failure. */
int bs_security_configure(BSClient *,int mode,const char *pairing_key);
int bs_security_mode(const BSClient *);
int bs_generate_pairing_key(char out[65]);
/* Modern TLS builds only. Directory must already exist and be user-private.
   Call before first connection. Existing identities are never silently replaced. */
int bs_trust_configure(BSClient *,const char *directory);
int bs_pairing_code(const BSClient *,char out[20]);
int bs_trust_current(BSClient *,const char *code_from_other_screen);
int bs_trust_status(const BSClient *); /* 0 unknown, 1 matched pin, -1 changed/unavailable */
int bs_forget_current(BSClient *);
int bs_transfer_encrypted(const BSClient *);
unsigned bs_peer_features(const BSClient *,const char *id);
void bs_local_capabilities(const BSClient *,BSCapabilities *);
int bs_peer_capabilities(const BSClient *,const char *id,BSCapabilities *);
int bs_transfer_capabilities(const BSClient *,BSCapabilities *);
/* Discovery hints, not authenticated identity or a live compatibility guarantee. */
int bs_peer_summary(const BSClient *,const char *id,char *out,size_t capacity);
int bs_resume_enable(BSClient *,int enabled);
int bs_resume_available(BSClient *,const char *destination);
int bs_accept_resumable(BSClient *,const char *destination,int restart);
/* Bounded local checkpoint review. 1 scanning, 2 ready, 3 removing, 4 done,
   5 stopped/error. Only a ready, unchanged review can be confirmed for cleanup.
   Transfers/queued work block cleanup. Completed files are never candidates. */
/* removed: 0 listed, 1 removed, -1 preserved, -2 not fully removed. */
typedef struct {char path[BS_PATH_MAX+1];unsigned long bytes,total;int removed;} BSRetainedItem;
unsigned long bs_retained_scan(BSClient *,const char *receive_folder);
int bs_retained_step(BSClient *);
int bs_retained_phase(const BSClient *);
unsigned long bs_retained_serial(const BSClient *);
int bs_retained_count(const BSClient *);
int bs_retained_item(const BSClient *,int index,BSRetainedItem *);
int bs_retained_summary(const BSClient *,char *out,size_t capacity);
int bs_retained_remove(BSClient *,unsigned long reviewed_serial);
void bs_retained_close(BSClient *);

int bs_peer_count(const BSClient *);
const BSPeer *bs_peer(const BSClient *, int index);
const BSStatus *bs_status(const BSClient *);
/* Local observation only; no change to the wire protocol or BSStatus layout. */
typedef enum { BS_RESULT_NONE=0, BS_RESULT_COMPLETE=1, BS_RESULT_FAILED=2,
               BS_RESULT_CANCELLED=3, BS_RESULT_DECLINED=4, BS_RESULT_SKIPPED=5 } BSResult;
unsigned long bs_transfer_serial(const BSClient *);
BSResult bs_transfer_result(const BSClient *);
/* Recent payload bytes per second; socket writes do not prove delivery. */
/* Zero is unlimited; byte/s limits apply to payload, starting next connection. */
int bs_limits_configure(BSClient *,unsigned long send_bps,unsigned long receive_bps);
void bs_limits_get(const BSClient *,unsigned long *send_bps,unsigned long *receive_bps);
/* Strict shared parser for UI/config KiB/s values (0..BS_RATE_KIB_MAX). */
int bs_limit_parse(const char *kib,unsigned long *bytes_per_second);
unsigned long bs_transfer_rate(const BSClient *);
typedef enum { BS_ERR_NONE=0, BS_ERR_INPUT, BS_ERR_BUSY, BS_ERR_SOURCE, BS_ERR_DESTINATION,
 BS_ERR_STORAGE, BS_ERR_NETWORK, BS_ERR_TIMEOUT, BS_ERR_PROTOCOL, BS_ERR_SECURITY,
 BS_ERR_TRUST, BS_ERR_LIMIT, BS_ERR_DECLINED, BS_ERR_CANCELLED, BS_ERR_REMOTE, BS_ERR_INTERNAL } BSErrorCode;
BSErrorCode bs_error_code(const BSClient *);
int bs_transfer_verified(const BSClient *);
const char *bs_last_error(const BSClient *);
#define BS_ERROR_RETRY 1
#define BS_ERROR_TEST_CONNECTION 2
#define BS_ERROR_OPEN_FOLDER 4
const char *bs_user_error(BSClient *);
const char *bs_user_status(BSClient *);
unsigned bs_error_actions(const BSClient *);
int bs_error_retry(BSClient *);
unsigned long bs_crc32(unsigned long crc, const unsigned char *, size_t);
int bs_safe_name(const char *);
int bs_to_utf8(const char *, char *, size_t);
int bs_to_local(const char *, char *, size_t);
/* Safe destination suggestions. Never writes or overwrites a file. */
int bs_suggest_name(const char *wire,int ordinal,char *out,size_t capacity);
int bs_suggest_destination(const char *folder,const char *wire,int ordinal,char *out,size_t capacity);
#define BS_HISTORY_MAX 8
typedef struct {
    unsigned long serial,total,transferred,finished;
    int incoming,result,encrypted,verified;
    char filename[256],peer[64],path[BS_PATH_MAX+1],message[256];
    char peer_id[33],address[48];unsigned short port;
} BSHistoryItem;
int bs_history_count(const BSClient *);
int bs_history_find(const BSClient *,unsigned long serial);
int bs_history_item(const BSClient *,int newest_first,BSHistoryItem *);
void bs_history_clear(BSClient *);
/* Private local state; restored outgoing work is always paused until Continue.
   Local ABI-versioned format; never exchange these files between devices. */
int bs_state_configure(BSClient *,const char *path);
int bs_state_flush(BSClient *);
const char *bs_state_error(const BSClient *);
int bs_state_restored(const BSClient *);
/* Bounded text for local diagnostic/history UI; never includes secrets. */
/* Nonblocking TCP reachability only, no file offer; outcome is in diagnostics.
   NULL address tests the last remote address or first discovered peer. */
int bs_network_test(BSClient *,const char *address,unsigned short port);
int bs_progress_summary(const BSClient *,char *out,size_t capacity);
int bs_diagnostics(const BSClient *,char *out,size_t capacity);
int bs_history_text(const BSClient *,int index,char *out,size_t capacity);
int bs_retry_history(BSClient *,int index);
int bs_tools_line(const BSClient *,int history_index,int line,char *out,size_t capacity);
/* Initialize once before creating a client; close after all clients are destroyed. */
int bs_platform_init(void);
void bs_platform_cleanup(void);
void bs_hostname(char *out, size_t capacity);
#endif
