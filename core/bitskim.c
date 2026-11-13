#include "bitskim.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef BS_OPENSSL
#include <openssl/evp.h>
#include <openssl/crypto.h>
#endif
#define HELLO 1
#define OFFER 2
#define ACCEPT 3
#define DECLINE 4
#define DATA 5
#define END 6
#define COMPLETE 7
#define CANCEL 8
#define FRAME_ERROR 9
#define TREE_START 27
#define TREE_ITEM 28
#define TREE_REF 29
#define TREE_WAIT 30
#define TREE_PREPARE 19
#define LIMITS 24
#define SKIP_FILE 25
#define VERIFY_WAIT 26
#define LIMIT_EXCHANGE 17
#define CAPABILITIES 21
#define START_TLS 22
#define TLS_READY 23
#define NEGOTIATING 14
#define TLS_SERVER 15
#define TLS_CLIENT 16
#define OFFER_BATCH 15
#define COLLECTION_START 16
#define COLLECTION_ITEM 17
#define COLLECTION_END 18
#define COLLECTION_REF 19
#define COLLECTION_NEED 20
#define LIST_SENDING 12
#define LIST_RECEIVING 13
#define OFFER_RESUME 10
#define ACCEPT_RESUME 11
#define RESUME_READY 12
#define PREPARE_RESUME 13
#define RESUME_WAIT 14
#define PREPARE_SEND 11
#define CHECK_RECEIVE 8
#define CHECK_SEND 9
#define WAIT_RESUME 10
#define HEADER 12
#define FRAME_MAX (BS_CHUNK+HEADER)
#define VERIFY_DISK 18
#define IDLE 0
#define CONNECTING 1
#define GREETING 2
#define DECISION 3
#define SENDING 4
#define RECEIVING 5
#define FINISHING 6
#define CLOSING 7
/* Per-tick work bounds. Larger budgets let fast hosts fill the link without a
   faster UI timer; classic CPUs keep small slices so the desktop stays responsive.
   Override at build time with -DBS_SEND_BURST=n / -DBS_RECV_FRAMES=n. */
#if defined(__amigaos__) || defined(__riscos__) || defined(BS_DOS)
#define BS_CLASSIC 1
#endif
#ifndef BS_SEND_BURST
#ifdef BS_CLASSIC
#define BS_SEND_BURST 2
#else
#define BS_SEND_BURST 32
#endif
#endif
#ifndef BS_RECV_FRAMES
#ifdef BS_CLASSIC
#define BS_RECV_FRAMES 4
#else
#define BS_RECV_FRAMES 64
#endif
#endif
#define RX_CAP (2*FRAME_MAX)
#define LINGER_MAX 4
#define LINGER_SECONDS 3
#define REPLY_SLOTS 8
typedef struct {BSBatchItem item;unsigned long hi,lo,files,dirs;} BSBatchRecord;
typedef struct {unsigned char token[16];int count,refs,streamed,tree;void *wire_tree;FILE *items;BSBatchItem cache;unsigned long hi,lo,files,dirs;} BSBatch;
typedef struct BSTreeScan BSTreeScan;
typedef struct BSTreePlan BSTreePlan;
typedef struct {BSBatch *manifest;char peer[33],address[48],folder[BS_PATH_MAX+1];unsigned short port;int next,encrypted;unsigned long last_seen;BSTreePlan *plan;} BSBatchApproval;
typedef struct {char id[33],address[48],path[BS_PATH_MAX+1];unsigned short port;unsigned long total,group;int kind;unsigned long report;char relative[256];} BSQueued;
typedef struct BSRetry BSRetry;
typedef struct {unsigned long complete,folders,skipped,failed,cancelled;int lost;} BSRun;
typedef struct {BSQueued q;int result;} BSRunRecord;
typedef struct {unsigned long rate,credit,stamp,fraction;} BSBucket;
typedef struct BSFolders BSFolders;
struct BSClient {
    BSFolders *folders;
    BSSocket check_socket;int check_pending;unsigned long check_start;
    char check_result[160],check_local[48];
    BSConfig config; BSCallback callback; void *context;
    BSSocket listener,udp,sock; int state,got_hello,discovery_paused;
    unsigned long discovery_sent,discovery_received,discovery_last;
    char expected_id[33],remote_id[33]; unsigned short remote_port;
    BSPeer peers[BS_MAX_PEERS]; int peer_count;
    unsigned peer_features[BS_MAX_PEERS],remote_features;
    int security_mode,last_encrypted,pairing_set,cap_ready;
    unsigned long send_limit,receive_limit,active_send_limit,active_receive_limit,peer_receive_limit;
    BSBucket upload,download;
    BSCapabilities peer_caps[BS_MAX_PEERS],remote_caps;
    unsigned char peer_caps_known[BS_MAX_PEERS];char upgrade_id[33];
#ifdef BS_OPENSSL
    unsigned char pairing_key[32];
    int resume_enabled,resume_offer,resume_cache;
    unsigned long verify_remaining,resume_offset,last_resume_heartbeat;
    unsigned char prefix_digest[32];EVP_MD_CTX *prefix_hash;
    char resume_meta[BS_PATH_MAX+1],trust_directory[BS_PATH_MAX+1];
#endif
    int batch_enabled,in_batch_index,out_batch_index;
    unsigned long batch_counter;int list_cursor;
    BSBatch *in_batch,*out_batch;BSBatchApproval *batch_approval;BSTreeScan *tree_scan;unsigned long tree_counter;int queue_dir_request;
    unsigned long transfer_serial; BSResult transfer_result;
    BSStatus status; char error[256], source[BS_PATH_MAX+1], dest[BS_PATH_MAX+1], temp[BS_PATH_MAX+1];
    BSHistoryItem history[BS_HISTORY_MAX];int history_count,history_next;unsigned long history_serial;
    unsigned long disk_crc,disk_bytes,last_verify;int verified,conflict_policy,replace_pending;
    FILE *file; unsigned long rate_time,rate_bytes,rate;
    unsigned long crc, last_io, decision_start, last_probe;
    unsigned char rx[RX_CAP],tx[FRAME_MAX]; int rx_count,tx_count,tx_sent,status_dirty;
    /* Sockets closed gracefully: write side shut down, inbound bytes drained until
       EOF or LINGER_SECONDS, so a final CANCEL/ERROR frame is not lost to a reset. */
    BSSocket linger[LINGER_MAX]; unsigned long linger_since[LINGER_MAX];
    /* Discovery replies are limited to one per second per source address. */
    unsigned long reply_addr[REPLY_SLOTS], reply_time[REPLY_SLOTS]; int reply_next;
    unsigned char udp_buf[513];
    FILE *run_file,*incoming_results;unsigned long run_count,receive_serial;unsigned char run_token[16];int run_token_set;BSRun outgoing_run,incoming_run;BSRetry *retry;
    FILE *queue_file;unsigned long queue_head;BSBatch *queue_batch;int queue_batch_index;int queue_count,queue_active,queue_paused;unsigned long queue_processed;
};
static void copystr(char *d,const char *s,size_t n) { if(n) { strncpy(d,s?s:"",n-1); d[n-1]=0; } }
static void batch_free(BSBatch *b) {if(b && --b->refs==0){if(b->items)fclose(b->items);if(b->wire_tree)free(b->wire_tree);free(b);}}
static BSBatch *batch_new(void) {BSBatch *b=(BSBatch *)calloc(1,sizeof *b);if(b){b->refs=1;b->items=bsp_temp_file();if(!b->items){free(b);b=NULL;}}return b;}
static void wide_add(unsigned long *hi,unsigned long *lo,unsigned long h,unsigned long l){unsigned long old=*lo;*lo=(*lo+l)&0xffffffffUL;*hi=(*hi+h+(*lo<old))&0xffffffffUL;}
static void wide_sub(unsigned long *hi,unsigned long *lo,unsigned long h,unsigned long l){unsigned long old=*lo;*lo=(*lo-l)&0xffffffffUL;*hi=(*hi-h-(old<l))&0xffffffffUL;}
static void wide_product(unsigned long a,unsigned long b,unsigned long *hi,unsigned long *lo){unsigned long w=(a&65535UL)*(b&65535UL),t=(a>>16)*(b&65535UL)+(w>>16),z=t>>16;t=(t&65535UL)+(a&65535UL)*(b>>16);*hi=((a>>16)*(b>>16)+z+(t>>16))&0xffffffffUL;*lo=((t<<16)+(w&65535UL))&0xffffffffUL;}
static int batch_record(BSBatch *b,int i,BSBatchRecord *r){return b && i>=0 && i<b->count && !fseek(b->items,(long)i*(long)sizeof *r,SEEK_SET) && fread(r,sizeof *r,1,b->items)==1;}
static const BSBatchItem *batch_item(BSBatch *b,int i){BSBatchRecord r;if(!batch_record(b,i,&r))return NULL;b->cache=r.item;return &b->cache;}
static int batch_append(BSBatch *b,const BSBatchItem *item){BSBatchRecord r;size_t n=strlen(item->filename);memset(&r,0,sizeof r);r.item=*item;
    wide_add(&b->hi,&b->lo,0,item->total);if(n && item->filename[n-1]=='/')b->dirs++;else b->files++;
    r.hi=b->hi;r.lo=b->lo;r.files=b->files;r.dirs=b->dirs;return !fseek(b->items,0,SEEK_END) && fwrite(&r,sizeof r,1,b->items)==1;
}
static void tree_plan_free(BSTreePlan *);
static void tree_scan_free(BSClient *);
static void batch_revoke(BSClient *c){if(c->batch_approval){tree_plan_free(c->batch_approval->plan);batch_free(c->batch_approval->manifest);free(c->batch_approval);c->batch_approval=NULL;}}
static int wire_filename(const char *path,char out[256]) {
    const char *base=path,*p;
    for(p=path;*p;p++) {
        if(*p=='/')base=p+1;
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
        if(*p=='\\')base=p+1;
#endif
#if defined(__amigaos__) || defined(BS_DOS) || defined(BS_ATARI)
        if(*p==':')base=p+1;
#endif
    }
    return bs_to_utf8(base,out,256) && bs_safe_name(out);
}
static unsigned long get32(const unsigned char *p) { return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3]; }
static void put32(unsigned char *p,unsigned long v) { p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16); p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v; }
static unsigned short get16(const unsigned char *p) { return (unsigned short)((p[0]<<8)|p[1]); }
static void put16(unsigned char *p,unsigned short v) { p[0]=(unsigned char)(v>>8); p[1]=(unsigned char)v; }
#include "folders.h"
static void notify(BSClient *,BSEvent);
#include "netcheck.h"
static void history_record(BSClient *c);
static void run_record(BSClient *,int);
static void incoming_record(BSClient *);
static void retry_free(BSClient *);
static void notify(BSClient *c,BSEvent event) {
    if(event==BS_EVENT_COMPLETE || (event==BS_EVENT_ERROR && !c->status.awaiting_decision) || (event==BS_EVENT_STATUS && !c->status.active && c->transfer_result!=BS_RESULT_NONE)) {
        if(c->status.incoming)incoming_record(c);else if(c->queue_active && c->transfer_result!=BS_RESULT_NONE)run_record(c,c->transfer_result);
        history_record(c);
        if(event==BS_EVENT_COMPLETE && c->status.incoming && c->in_batch && c->in_batch_index+1==c->in_batch->count)bs_run_summary(c,1,c->status.message,sizeof c->status.message);
    }
    if(c->callback)c->callback(c->context,event);
}
static void message(BSClient *c,const char *m) { copystr(c->status.message,m,sizeof(c->status.message)); notify(c,BS_EVENT_STATUS); }
/* Reflected CRC-32 (IEEE 802.3), one table lookup per byte. */
static const unsigned long crc_table[256]={
    0x00000000UL,0x77073096UL,0xee0e612cUL,0x990951baUL,0x076dc419UL,0x706af48fUL,
    0xe963a535UL,0x9e6495a3UL,0x0edb8832UL,0x79dcb8a4UL,0xe0d5e91eUL,0x97d2d988UL,
    0x09b64c2bUL,0x7eb17cbdUL,0xe7b82d07UL,0x90bf1d91UL,0x1db71064UL,0x6ab020f2UL,
    0xf3b97148UL,0x84be41deUL,0x1adad47dUL,0x6ddde4ebUL,0xf4d4b551UL,0x83d385c7UL,
    0x136c9856UL,0x646ba8c0UL,0xfd62f97aUL,0x8a65c9ecUL,0x14015c4fUL,0x63066cd9UL,
    0xfa0f3d63UL,0x8d080df5UL,0x3b6e20c8UL,0x4c69105eUL,0xd56041e4UL,0xa2677172UL,
    0x3c03e4d1UL,0x4b04d447UL,0xd20d85fdUL,0xa50ab56bUL,0x35b5a8faUL,0x42b2986cUL,
    0xdbbbc9d6UL,0xacbcf940UL,0x32d86ce3UL,0x45df5c75UL,0xdcd60dcfUL,0xabd13d59UL,
    0x26d930acUL,0x51de003aUL,0xc8d75180UL,0xbfd06116UL,0x21b4f4b5UL,0x56b3c423UL,
    0xcfba9599UL,0xb8bda50fUL,0x2802b89eUL,0x5f058808UL,0xc60cd9b2UL,0xb10be924UL,
    0x2f6f7c87UL,0x58684c11UL,0xc1611dabUL,0xb6662d3dUL,0x76dc4190UL,0x01db7106UL,
    0x98d220bcUL,0xefd5102aUL,0x71b18589UL,0x06b6b51fUL,0x9fbfe4a5UL,0xe8b8d433UL,
    0x7807c9a2UL,0x0f00f934UL,0x9609a88eUL,0xe10e9818UL,0x7f6a0dbbUL,0x086d3d2dUL,
    0x91646c97UL,0xe6635c01UL,0x6b6b51f4UL,0x1c6c6162UL,0x856530d8UL,0xf262004eUL,
    0x6c0695edUL,0x1b01a57bUL,0x8208f4c1UL,0xf50fc457UL,0x65b0d9c6UL,0x12b7e950UL,
    0x8bbeb8eaUL,0xfcb9887cUL,0x62dd1ddfUL,0x15da2d49UL,0x8cd37cf3UL,0xfbd44c65UL,
    0x4db26158UL,0x3ab551ceUL,0xa3bc0074UL,0xd4bb30e2UL,0x4adfa541UL,0x3dd895d7UL,
    0xa4d1c46dUL,0xd3d6f4fbUL,0x4369e96aUL,0x346ed9fcUL,0xad678846UL,0xda60b8d0UL,
    0x44042d73UL,0x33031de5UL,0xaa0a4c5fUL,0xdd0d7cc9UL,0x5005713cUL,0x270241aaUL,
    0xbe0b1010UL,0xc90c2086UL,0x5768b525UL,0x206f85b3UL,0xb966d409UL,0xce61e49fUL,
    0x5edef90eUL,0x29d9c998UL,0xb0d09822UL,0xc7d7a8b4UL,0x59b33d17UL,0x2eb40d81UL,
    0xb7bd5c3bUL,0xc0ba6cadUL,0xedb88320UL,0x9abfb3b6UL,0x03b6e20cUL,0x74b1d29aUL,
    0xead54739UL,0x9dd277afUL,0x04db2615UL,0x73dc1683UL,0xe3630b12UL,0x94643b84UL,
    0x0d6d6a3eUL,0x7a6a5aa8UL,0xe40ecf0bUL,0x9309ff9dUL,0x0a00ae27UL,0x7d079eb1UL,
    0xf00f9344UL,0x8708a3d2UL,0x1e01f268UL,0x6906c2feUL,0xf762575dUL,0x806567cbUL,
    0x196c3671UL,0x6e6b06e7UL,0xfed41b76UL,0x89d32be0UL,0x10da7a5aUL,0x67dd4accUL,
    0xf9b9df6fUL,0x8ebeeff9UL,0x17b7be43UL,0x60b08ed5UL,0xd6d6a3e8UL,0xa1d1937eUL,
    0x38d8c2c4UL,0x4fdff252UL,0xd1bb67f1UL,0xa6bc5767UL,0x3fb506ddUL,0x48b2364bUL,
    0xd80d2bdaUL,0xaf0a1b4cUL,0x36034af6UL,0x41047a60UL,0xdf60efc3UL,0xa867df55UL,
    0x316e8eefUL,0x4669be79UL,0xcb61b38cUL,0xbc66831aUL,0x256fd2a0UL,0x5268e236UL,
    0xcc0c7795UL,0xbb0b4703UL,0x220216b9UL,0x5505262fUL,0xc5ba3bbeUL,0xb2bd0b28UL,
    0x2bb45a92UL,0x5cb36a04UL,0xc2d7ffa7UL,0xb5d0cf31UL,0x2cd99e8bUL,0x5bdeae1dUL,
    0x9b64c2b0UL,0xec63f226UL,0x756aa39cUL,0x026d930aUL,0x9c0906a9UL,0xeb0e363fUL,
    0x72076785UL,0x05005713UL,0x95bf4a82UL,0xe2b87a14UL,0x7bb12baeUL,0x0cb61b38UL,
    0x92d28e9bUL,0xe5d5be0dUL,0x7cdcefb7UL,0x0bdbdf21UL,0x86d3d2d4UL,0xf1d4e242UL,
    0x68ddb3f8UL,0x1fda836eUL,0x81be16cdUL,0xf6b9265bUL,0x6fb077e1UL,0x18b74777UL,
    0x88085ae6UL,0xff0f6a70UL,0x66063bcaUL,0x11010b5cUL,0x8f659effUL,0xf862ae69UL,
    0x616bffd3UL,0x166ccf45UL,0xa00ae278UL,0xd70dd2eeUL,0x4e048354UL,0x3903b3c2UL,
    0xa7672661UL,0xd06016f7UL,0x4969474dUL,0x3e6e77dbUL,0xaed16a4aUL,0xd9d65adcUL,
    0x40df0b66UL,0x37d83bf0UL,0xa9bcae53UL,0xdebb9ec5UL,0x47b2cf7fUL,0x30b5ffe9UL,
    0xbdbdf21cUL,0xcabac28aUL,0x53b39330UL,0x24b4a3a6UL,0xbad03605UL,0xcdd70693UL,
    0x54de5729UL,0x23d967bfUL,0xb3667a2eUL,0xc4614ab8UL,0x5d681b02UL,0x2a6f2b94UL,
    0xb40bbe37UL,0xc30c8ea1UL,0x5a05df1bUL,0x2d02ef8dUL
};
unsigned long bs_crc32(unsigned long crc,const unsigned char *p,size_t n) {
    crc=(crc^0xffffffffUL)&0xffffffffUL;
    while(n--) crc=crc_table[(crc^*p++)&255]^(crc>>8);
    return (crc^0xffffffffUL)&0xffffffffUL;
}
static int valid_utf8(const unsigned char *s) {
    unsigned long value,min; int n,i;
    while(*s) {
        if(*s<128) { s++; continue; }
        if(*s>=0xc2 && *s<=0xdf) { n=1; value=*s&31; min=0x80; }
        else if(*s>=0xe0 && *s<=0xef) { n=2; value=*s&15; min=0x800; }
        else if(*s>=0xf0 && *s<=0xf4) { n=3; value=*s&7; min=0x10000; }
        else return 0;
        s++;
        for(i=0;i<n;i++) { if(s[i]<0x80 || s[i]>0xbf) return 0; value=(value<<6)|(s[i]&63); }
        if(value<min || value>0x10ffff || (value>=0xd800 && value<=0xdfff) || (value>=0x80 && value<=0x9f)) return 0;
        s+=n;
    }
    return 1;
}
int bs_safe_name(const char *s) {
    size_t n; const unsigned char *p=(const unsigned char *)s;
    if(!s || !valid_utf8((const unsigned char *)s) || !s[0] || !strcmp(s,".") || !strcmp(s,"..")) return 0;
    n=strlen(s); if(n>255 || s[n-1]==' ' || s[n-1]=='.') return 0;
    for(;*p;p++) if(*p<32 || *p==127 || strchr("/\\:*?\"<>|",*p)) return 0;
    return 1;
}
static void cleanup_transfer(BSClient *c) {
    if(c->file) { fclose(c->file); c->file=NULL; }
#ifdef BS_OPENSSL
    if(c->prefix_hash){EVP_MD_CTX_free(c->prefix_hash);c->prefix_hash=NULL;}
    if(c->resume_cache) {
        if(c->transfer_result==BS_RESULT_CANCELLED || c->transfer_result==BS_RESULT_DECLINED) {bsp_remove_file(c->temp);bsp_remove_file(c->resume_meta);}
        c->temp[0]=0;c->resume_meta[0]=0;c->resume_cache=0;
    }
#endif
    if(c->temp[0]) { bsp_remove_file(c->temp); c->temp[0]=0; }
    if(c->sock!=BS_INVALID_SOCKET) { bsp_close(c->sock); c->sock=BS_INVALID_SOCKET; }
    c->state=IDLE; c->status.active=0; c->status.awaiting_decision=0;
    c->tx_count=c->tx_sent=c->rx_count=0;
}
static void fail(BSClient *c,const char *m) {
    batch_revoke(c);
    if(c->transfer_result==BS_RESULT_NONE) c->transfer_result=BS_RESULT_FAILED;
    cleanup_transfer(c); copystr(c->error,m,sizeof(c->error)); copystr(c->status.message,m,sizeof(c->status.message)); notify(c,BS_EVENT_ERROR);
}
static void forget_peer(BSClient *c,const char *ip,unsigned short port,const char *identity) {
    int i,changed=0;
    for(i=c->peer_count-1;i>=0;i--) {
        BSPeer *p=&c->peers[i];
        if(strcmp(p->address,ip) || p->port!=port || (identity[0] && strcmp(p->id,identity))) continue;
        c->peer_count--;c->peers[i]=c->peers[c->peer_count];c->peer_features[i]=c->peer_features[c->peer_count];c->peer_caps[i]=c->peer_caps[c->peer_count];c->peer_caps_known[i]=c->peer_caps_known[c->peer_count];changed=1;
    }
    if(changed) notify(c,BS_EVENT_PEERS);
}
static void connection_failed(BSClient *c,const char *reason) {
#ifdef BS_OPENSSL
    if(bsp_tls_error(c->sock)[0])reason=bsp_tls_error(c->sock);
#endif
    forget_peer(c,c->status.peer_address,c->remote_port,c->remote_id);
    fail(c,reason);
}
static int frame_into(unsigned char *out,int type,const void *p,int n) {
    memcpy(out,"BSK1",4); put16(out+4,(unsigned short)type); put16(out+6,0); put32(out+8,(unsigned long)n);
    if(n && p) memcpy(out+HEADER,p,(size_t)n);
    return HEADER+n;
}
/* The payload may already be in c->tx+HEADER (DATA is read there directly). */
static void frame(BSClient *c,int type,const void *p,int n) { c->tx_count=frame_into(c->tx,type,p,n); c->tx_sent=0; }
static int terminal_type(int type) { return type==CANCEL || type==DECLINE || type==FRAME_ERROR; }
static void park_socket(BSClient *c,BSSocket s) {
    int i,slot=0;
    shutdown(s,1);
    for(i=0;i<LINGER_MAX;i++) {
        if(c->linger[i]==BS_INVALID_SOCKET) { slot=i; break; }
        if(c->linger_since[i]<c->linger_since[slot]) slot=i;
    }
    if(c->linger[slot]!=BS_INVALID_SOCKET) bsp_close(c->linger[slot]);
    c->linger[slot]=s; c->linger_since[slot]=bsp_seconds();
}
static void drain_lingering(BSClient *c,unsigned long now) {
    int i,j,n=0;
    for(i=0;i<LINGER_MAX;i++) {
        if(c->linger[i]==BS_INVALID_SOCKET) continue;
        for(j=0;j<4;j++) {
            n=bsp_recv(c->linger[i],c->udp_buf,(int)sizeof(c->udp_buf));
            if(n>0) continue;
            if(n<0 && bsp_wouldblock()) n=1;
            break;
        }
        if(n<=0 || now-c->linger_since[i]>=LINGER_SECONDS) { bsp_close(c->linger[i]); c->linger[i]=BS_INVALID_SOCKET; }
    }
}
/* Best-effort terminal frame on a local abort. A frame already partly on the wire
   is completed first (a new frame cannot be spliced into it); only a terminal
   frame written in full lets the socket close gracefully. Never blocks. */
static void abort_transfer(BSClient *c,int type,const char *wire,const char *local,BSResult result) {
    int n;
    if(c->transfer_result==BS_RESULT_NONE) c->transfer_result=result;
    if(c->sock!=BS_INVALID_SOCKET && c->state!=CONNECTING) {
        if(c->tx_count && c->tx_sent) {
            n=bsp_send(c->sock,c->tx+c->tx_sent,c->tx_count-c->tx_sent);
            if(n>0) c->tx_sent+=n;
            if(c->tx_sent==c->tx_count) c->tx_count=c->tx_sent=0;
        }
        if(!c->tx_count || !c->tx_sent) {
            frame(c,type,wire,(int)strlen(wire));
            if(bsp_send(c->sock,c->tx,c->tx_count)==c->tx_count) { park_socket(c,c->sock); c->sock=BS_INVALID_SOCKET; }
        }
    }
    fail(c,local);
}
static void finish_error(BSClient *c,int type,const char *m) {
    batch_revoke(c);
    if(c->transfer_result==BS_RESULT_NONE) c->transfer_result=type==DECLINE?BS_RESULT_DECLINED:BS_RESULT_FAILED;
    if(c->tx_count) { fail(c,m); return; }
    frame(c,type,m,(int)strlen(m)); c->state=CLOSING; c->status.awaiting_decision=0;
    copystr(c->error,m,sizeof(c->error));
    if(type==DECLINE) message(c,m);
    else { copystr(c->status.message,m,sizeof(c->status.message)); notify(c,BS_EVENT_ERROR); }
}
static int hexval(char v) { if(v>='0'&&v<='9') return v-'0'; if(v>='a'&&v<='f') return v-'a'+10; return -1; }
static unsigned local_features(const BSClient *c) {
    unsigned flags=BS_CAP_DETAILS|(BS_CAP_RATE_LIMITS|BS_CAP_SKIP|BS_CAP_DISK_VERIFY)|(c->batch_enabled?(BS_CAP_BATCH|BS_CAP_COLLECTION|BS_CAP_DIRECTORIES):0);
#ifdef BS_OPENSSL
    if(c->resume_enabled)flags|=BS_CAP_RESUME;
    if(c->security_mode){flags|=BS_CAP_TLS;if(!c->pairing_set){flags|=BS_CAP_AUTO_TLS;if(c->trust_directory[0])flags|=BS_CAP_DEVICE_TRUST;}}
#endif
    return flags;
}
#include "caps.h"
static unsigned greeting_features(const unsigned char *p,int n) {
    int a,b,h,l;const unsigned char *s;if(n<20)return 0;a=p[18];b=p[19];if(b<5 || n<20+a+b)return 0;s=p+20+a+b-5;
    if(memcmp(s,"|BX",3))return 0;
    h=hexval((char)s[3]);l=hexval((char)s[4]);return h<0 || l<0?0:(unsigned)((h<<4)|l);
}
static int greeting(BSClient *c,unsigned char *p) {
    int i,a,b,n=(int)strlen(c->config.name),k=(int)strlen(c->config.platform);
    for(i=0;i<16;i++) { a=hexval(c->config.id[i*2]); b=hexval(c->config.id[i*2+1]); p[i]=(unsigned char)((a<<4)|b); }
    put16(p+16,c->config.port); p[18]=(unsigned char)n; p[19]=(unsigned char)k;
    memcpy(p+20,c->config.name,(size_t)n); memcpy(p+20+n,c->config.platform,(size_t)k);
    if(local_features(c) && k<=26) {static const char hex[]="0123456789abcdef";unsigned f=local_features(c);memcpy(p+20+n+k,"|BX",3);p[23+n+k]=hex[(f>>4)&15];p[24+n+k]=hex[f&15];k+=5;p[19]=(unsigned char)k;}
    return 20+n+k;
}
static int parse_greeting(const unsigned char *p,int n,BSPeer *peer) {
    static const char *hex="0123456789abcdef"; int i,a,b;
    if(n<20) return 0;
    a=p[18]; b=p[19]; if(!a || a>63 || b>31 || n<20+a+b || !get16(p+16)) return 0;
    for(i=0;i<a+b;i++) if(p[20+i]<32 || p[20+i]==127) return 0;
    for(i=0;i<16;i++) { peer->id[i*2]=hex[p[i]>>4]; peer->id[i*2+1]=hex[p[i]&15]; } peer->id[32]=0;
    memcpy(peer->name,p+20,(size_t)a); peer->name[a]=0; memcpy(peer->platform,p+20+a,(size_t)b); peer->platform[b]=0;
    if(greeting_features(p,n))peer->platform[b-5]=0;
    peer->port=get16(p+16); return valid_utf8((const unsigned char *)peer->name) && valid_utf8((const unsigned char *)peer->platform);
}
static void send_hello(BSClient *c) { unsigned char p[128]; int n=greeting(c,p); frame(c,HELLO,p,n); }
#include "rate.h"
static void reset_status(BSClient *c,int incoming,const char *ip) {
    batch_free(c->in_batch);c->in_batch=NULL;c->in_batch_index=0;c->out_batch=NULL;c->out_batch_index=0;
    c->transfer_serial=(c->transfer_serial+1)&0xffffffffUL; if(!c->transfer_serial) c->transfer_serial=1;
    c->active_send_limit=c->send_limit;c->active_receive_limit=c->receive_limit;c->peer_receive_limit=0;limits_reset(c);
    c->verified=0;c->replace_pending=0;c->dest[0]=0;
    c->transfer_result=BS_RESULT_NONE;c->remote_features=0;c->last_encrypted=0;c->cap_ready=0;c->upgrade_id[0]=0;memset(&c->remote_caps,0,sizeof c->remote_caps);
#ifdef BS_OPENSSL
    c->resume_offer=c->resume_cache=0;c->resume_meta[0]=0;c->resume_offset=c->verify_remaining=0;
#endif
    memset(&c->status,0,sizeof(c->status)); c->status.active=1; c->status.incoming=incoming;
    copystr(c->status.peer_address,ip,sizeof(c->status.peer_address)); c->got_hello=0; c->crc=0; c->remote_id[0]=0; c->remote_port=0;
    c->expected_id[0]=0; c->error[0]=c->temp[0]=c->dest[0]=0; c->rx_count=c->tx_count=c->tx_sent=0; c->last_io=bsp_seconds(); c->rate_time=c->last_io; c->rate_bytes=c->rate=0; c->status_dirty=0;
}
static BSSocket open_socket(BSClient *c,int type) {
    struct sockaddr_in a; BSSocket s; int yes=1;
    s=socket(AF_INET,type,0); if(s==BS_INVALID_SOCKET) return BS_INVALID_SOCKET;
    if(type==SOCK_DGRAM) setsockopt(s,SOL_SOCKET,SO_BROADCAST,(const char *)&yes,sizeof(yes));
#ifndef _WIN32
    /* Rebind immediately after a restart while old connections sit in TIME_WAIT.
       BSD semantics still refuse a second listener on the same address/port.
       Winsock already permits this by default; its SO_REUSEADDR would instead
       allow port sharing, so Windows keeps the default. */
    else setsockopt(s,SOL_SOCKET,SO_REUSEADDR,(const char *)&yes,sizeof(yes));
#endif
#ifdef SO_NOSIGPIPE
    setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
#endif
    memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(type==SOCK_DGRAM ? c->config.discovery_port : c->config.port);
    a.sin_addr.s_addr=c->config.bind_address[0] ? inet_addr(c->config.bind_address) : INADDR_ANY;
    if(bind(s,(struct sockaddr *)&a,sizeof(a))<0) {
        /* Shared-host test systems can probe from an ephemeral UDP port. */
        if(type!=SOCK_DGRAM) { bsp_close(s); return BS_INVALID_SOCKET; }
        a.sin_port=0;
        if(bind(s,(struct sockaddr *)&a,sizeof(a))<0) { bsp_close(s); return BS_INVALID_SOCKET; }
    }
    if(bsp_nonblock(s)<0) { bsp_close(s); return BS_INVALID_SOCKET; }
    return s;
}
BSClient *bs_create(const BSConfig *config,BSCallback cb,void *context) {
    BSClient *c; int i;
    if(!config || strlen(config->id)!=32) return NULL;
    for(i=0;i<32;i++) if(hexval(config->id[i])<0) return NULL;
    c=(BSClient *)calloc(1,sizeof(*c)); if(!c) return NULL;
#ifdef BS_OPENSSL
    c->security_mode=BS_SECURITY_OPTIONAL;
#endif
    c->config=*config; c->config.name[63]=0; c->config.platform[31]=0; c->config.bind_address[47]=0;
    if(!c->config.name[0]) copystr(c->config.name,"BitSkim",64);
    if(!c->config.port) c->config.port=BS_PORT;
    if(!c->config.discovery_port) c->config.discovery_port=c->config.port;
    c->callback=cb; c->context=context; c->listener=c->udp=c->sock=BS_INVALID_SOCKET;
    for(i=0;i<LINGER_MAX;i++) c->linger[i]=BS_INVALID_SOCKET;
    c->listener=open_socket(c,SOCK_STREAM);
    if(c->listener==BS_INVALID_SOCKET || listen(c->listener,4)<0) { bs_destroy(c); return NULL; }
    c->udp=open_socket(c,SOCK_DGRAM);
    if(c->udp==BS_INVALID_SOCKET) { bs_destroy(c); return NULL; }
    copystr(c->status.message,"Ready on your local network",256); bs_refresh(c); return c;
}
void bs_destroy(BSClient *c) { int i; if(!c) return;if(c->check_pending)bsp_close(c->check_socket);retry_free(c);if(c->run_file)fclose(c->run_file);if(c->incoming_results)fclose(c->incoming_results);tree_scan_free(c); if(c->queue_file)fclose(c->queue_file);batch_free(c->queue_batch);batch_free(c->in_batch);batch_revoke(c); cleanup_transfer(c); for(i=0;i<LINGER_MAX;i++) if(c->linger[i]!=BS_INVALID_SOCKET) bsp_close(c->linger[i]); if(c->listener!=BS_INVALID_SOCKET) bsp_close(c->listener); if(c->udp!=BS_INVALID_SOCKET) bsp_close(c->udp); 
#ifdef BS_OPENSSL
    OPENSSL_cleanse(c->pairing_key,32);
#endif
    free(c->folders);free(c); }
void bs_configure(BSClient *c,const char *name,int visible,int receiving) {
    copystr(c->config.name,name,64); if(!c->config.name[0]) copystr(c->config.name,"BitSkim",64);
    if(!receiving)batch_revoke(c);
    c->config.visible=visible; c->config.receiving=receiving; bs_refresh(c);
}
void bs_refresh(BSClient *c) {
    unsigned char p[160]; int n;
    if(c->discovery_paused) return;
    memcpy(p,"BSKD",4); p[4]=1; p[5]=1; n=greeting(c,p+6);
    /* Hidden probes advertise no name/endpoint to peers: a fixed anonymous greeting. */
    if(!c->config.visible || !c->config.receiving) { memset(p+6,0,16); p[24]=7; p[25]=0; memcpy(p+26,"BitSkim",7); n=27; }
    memset(p+n+6,0,sizeof(p)-(size_t)n-6);
    if(c->config.visible && c->config.receiving)cap_encode(c,p+n+6);
    bsp_broadcast(c->udp,p,sizeof(p),c->config.discovery_port); c->last_probe=bsp_seconds();c->discovery_sent++;
}
void bs_discovery_search(BSClient *c,int enabled) {
    int paused=!enabled;
    if(c->discovery_paused==paused) return;
    c->discovery_paused=paused;
    if(paused) {
        if(c->peer_count) { c->peer_count=0; notify(c,BS_EVENT_PEERS); }
    } else bs_refresh(c);
}
static int reply_allowed(BSClient *c,unsigned long addr,unsigned long now) {
    int i;
    for(i=0;i<REPLY_SLOTS;i++) if(c->reply_time[i] && c->reply_addr[i]==addr) {
        if(c->reply_time[i]==now) return 0;
        c->reply_time[i]=now; return 1;
    }
    i=c->reply_next; c->reply_next=(c->reply_next+1)%REPLY_SLOTS;
    c->reply_addr[i]=addr; c->reply_time[i]=now; return 1;
}
static void discovery(BSClient *c) {
    unsigned char *p=c->udp_buf,reply[160]; struct sockaddr_in from; BSPeer peer; int n,i,j,kind,changed=0;
    BSSockLen len; unsigned long now=bsp_seconds();
    for(j=0;j<8;j++) {
        len=sizeof(from); n=(int)recvfrom(c->udp,(char *)p,sizeof(c->udp_buf),0,(struct sockaddr *)&from,&len);
        if(n<0) break;
        if(n<26 || n>512 || memcmp(p,"BSKD",4) || p[4]!=1 || (p[5]!=1 && p[5]!=2)) continue;
        memset(&peer,0,sizeof(peer)); if(!parse_greeting(p+6,n-6,&peer)) continue;
        if(!strcmp(peer.id,c->config.id)) continue;
        c->discovery_received++;c->discovery_last=now;
        kind=p[5];
        if(kind==1 && c->config.visible && c->config.receiving && reply_allowed(c,(unsigned long)from.sin_addr.s_addr,now)) {
            memcpy(reply,"BSKD",4); reply[4]=1; reply[5]=2; i=greeting(c,reply+6);
            memset(reply+i+6,0,sizeof(reply)-(size_t)i-6);cap_encode(c,reply+i+6);
            sendto(c->udp,(char *)reply,sizeof(reply),0,(struct sockaddr *)&from,sizeof(from));
        }
        if(c->discovery_paused) continue;
        if(kind!=2 && (kind!=1 || !strcmp(peer.id,"00000000000000000000000000000000"))) continue;
        copystr(peer.address,inet_ntoa(from.sin_addr),sizeof(peer.address)); peer.last_seen=now;
        for(i=0;i<c->peer_count;i++) if(!strcmp(c->peers[i].id,peer.id) && !strcmp(c->peers[i].address,peer.address)) break;
        if(i==BS_MAX_PEERS) continue;
        /* A refresh that only renews last_seen is not a catalog change. */
        if(i==c->peer_count) { c->peer_count++; changed=1; }
        else if(c->peer_features[i]!=greeting_features(p+6,n-6) || strcmp(c->peers[i].name,peer.name) || strcmp(c->peers[i].platform,peer.platform) || c->peers[i].port!=peer.port) changed=1;
        {BSCapabilities caps;int end=26+p[24]+p[25],known=0;
         if(end+36<=n)known=cap_decode(p+end,36,&caps);
         if(known!=c->peer_caps_known[i] || (known && memcmp(&caps,&c->peer_caps[i],sizeof caps)))changed=1;
         c->peer_caps_known[i]=(unsigned char)known;if(known)c->peer_caps[i]=caps;
        }
        c->peers[i]=peer;c->peer_features[i]=greeting_features(p+6,n-6);
    }
    for(i=c->peer_count-1;i>=0;i--) if(now-c->peers[i].last_seen>90) { c->peer_count--;c->peers[i]=c->peers[c->peer_count];c->peer_features[i]=c->peer_features[c->peer_count];c->peer_caps[i]=c->peer_caps[c->peer_count];c->peer_caps_known[i]=c->peer_caps_known[c->peer_count]; changed=1; }
    if(changed) notify(c,BS_EVENT_PEERS);
}
int bs_send(BSClient *c,const char *ip,unsigned short port,const char *path) {
    struct sockaddr_in a; char wire_name[256]; unsigned long size; BSSocket s; int r;
    if(c->state!=IDLE) { copystr(c->error,"A transfer is already active",256); return 0; }
    if(!path || strlen(path)>BS_PATH_MAX || !(bsp_size(path,&size) || (c->queue_dir_request && bsp_path_kind(path)==2 && (size=0,1)))) { copystr(c->error,"Choose a regular file smaller than 2 GiB",256); return 0; }
    if(!wire_filename(path,wire_name)) {copystr(c->error,"Filename is not portable; rename the file first",256);return 0;}
    memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(port?port:BS_PORT); a.sin_addr.s_addr=inet_addr(ip);
    if(a.sin_addr.s_addr==INADDR_NONE || !a.sin_addr.s_addr) { copystr(c->error,"Enter a valid IPv4 address",256); return 0; }
    s=socket(AF_INET,SOCK_STREAM,0); if(s==BS_INVALID_SOCKET) { copystr(c->error,"Could not create connection",256); return 0; }
#ifdef SO_NOSIGPIPE
    { int yes=1; setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes)); }
#endif
    if(bsp_nonblock(s)<0) { bsp_close(s); copystr(c->error,"Could not configure connection",256); return 0; }
#ifdef BS_OPENSSL
    {int i,encrypted=c->security_mode==BS_SECURITY_REQUIRED;
     for(i=0;i<c->peer_count;i++)if(!strcmp(c->peers[i].address,ip) && c->peers[i].port==(port?port:BS_PORT) && (c->peer_features[i]&BS_CAP_TLS) && !(c->peer_features[i]&BS_CAP_DETAILS) && c->security_mode)encrypted=1;
     if(!bsp_tls_attach(s,0,c->security_mode,encrypted,c->pairing_set?c->pairing_key:NULL)){bsp_close(s);copystr(c->error,"Cannot allocate encrypted transport",256);return 0;}}
#endif
    bsp_watch_connection(s);
    r=connect(s,(struct sockaddr *)&a,sizeof(a)); if(r<0 && !
#ifdef BS_OPENSSL
        bsp_network_wouldblock()
#else
        bsp_wouldblock()
#endif
    ) { bsp_close(s); copystr(c->error,"Could not connect to that computer",256); forget_peer(c,ip,port?port:BS_PORT,""); return 0; }
    reset_status(c,0,ip); c->remote_port=port?port:BS_PORT; c->sock=s; c->state=CONNECTING; c->status.total=size; copystr(c->source,path,sizeof(c->source)); copystr(c->status.filename,wire_name,sizeof(c->status.filename)); if(!c->queue_count)remember_folder(c,0,path,0);message(c,"Connecting..."); return 1;
}
int bs_send_peer(BSClient *c,const char *id,const char *path) {
    int i,found=-1; char identity[33],address[48]; unsigned short port;
    if(c->state!=IDLE) { copystr(c->error,"A transfer is already active",256); return 0; }
    if(!id || strlen(id)!=32) { copystr(c->error,"Invalid peer ID",256); return 0; }
    copystr(identity,id,sizeof(identity));
    for(i=0;i<c->peer_count;i++) if(!strcmp(c->peers[i].id,identity)) {
        if(found>=0) { copystr(c->error,"Peer ID is ambiguous; use a manual address",256); return 0; }
        found=i;
    }
    if(found<0) { copystr(c->error,"Peer ID is unavailable",256); return 0; }
    copystr(address,c->peers[found].address,sizeof(address)); port=c->peers[found].port;
    if(!bs_send(c,address,port,path)) return 0;
    copystr(c->expected_id,identity,sizeof(c->expected_id)); return 1;
}
static int same_local_path(const char *a,const char *b){while(*a && *b){if(toupper((unsigned char)*a++)!=toupper((unsigned char)*b++))return 0;}return !*a && !*b;}
static int storage_check(BSClient *,const char *,int);
static void storage_parent(const char *,char *);
#ifdef BS_OPENSSL
static int storage_resume(BSClient *,const char *,unsigned long);
#endif
int bs_accept(BSClient *c,const char *path) {
    int i;
    if(c->state!=DECISION || !c->status.incoming || !c->status.awaiting_decision || c->tx_count) return 0;
    if(bs_batch_is_tree(c) && (!c->batch_approval || !c->batch_approval->plan)){copystr(c->error,"Folder transfers require collection approval",256);return 0;}
    if(!path || !path[0] || strlen(path)>BS_PATH_MAX-32) { copystr(c->error,"Destination path is too long",256); return 0; }
    {char folder[BS_PATH_MAX+1];storage_parent(path,folder);if(!storage_check(c,folder,0))return 0;}
    /* Only create a temporary file after an explicit receiver decision. */
    copystr(c->dest,path,sizeof(c->dest));
    for(i=0;i<100;i++) {
        /* Short temporary leaves: old FileCore names are limited to ten characters
           and OFS/FFS/RAM: names to 30, so a suffix on the final name cannot fit.
           RISC OS paths are UnixLib-normalised ('/'); AmigaDOS also uses ':'. */
        {
            const char *slash=strrchr(path,'/'); size_t prefix;
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
            const char *backslash=strrchr(path,'\\');
#endif
#if defined(__amigaos__) || defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
            const char *colon=strrchr(path,':');
#endif
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
            if(backslash && (!slash || backslash>slash)) slash=backslash;
#endif
#if defined(__amigaos__) || defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
            if(colon && (!slash || colon>slash)) slash=colon;
#endif
            prefix=slash?(size_t)(slash-path)+1:0;
            memcpy(c->temp,path,prefix);
#if defined(BS_DOS) || defined(BS_ATARI)
            sprintf(c->temp+prefix,"BS%06lx.T%02x",bsp_seconds()&0xffffffUL,i);
#elif defined(__amigaos__) || defined(__riscos__)
            sprintf(c->temp+prefix,"BS%06lx%02x",bsp_seconds()&0xffffffUL,i);
#else
            sprintf(c->temp+prefix,"BS%06lx%02x.part",bsp_seconds()&0xffffffUL,i);
#endif
        }
        if(same_local_path(c->temp,path))continue;
        c->file=bsp_create_file(c->temp); if(c->file) break;
    }
    if(!c->file) { c->temp[0]=0; copystr(c->error,"Cannot create a file in that folder",256); return 0; }
    c->status.awaiting_decision=0; c->state=RECEIVING; c->last_io=bsp_seconds(); frame(c,ACCEPT,NULL,0); if(!c->in_batch)remember_folder(c,1,path,0);message(c,"Receiving file..."); return 1;
}
#include "resume.h"
void bs_decline(BSClient *c) { if(c->status.awaiting_decision) finish_error(c,DECLINE,"Transfer declined"); }
void bs_cancel(BSClient *c) {
    if(!c)return;
    retry_free(c);tree_scan_free(c);batch_revoke(c);
    if(c->state!=IDLE)abort_transfer(c,CANCEL,"The other computer cancelled the transfer","Transfer cancelled; remaining files discarded. Partial file removed.",BS_RESULT_CANCELLED);
    c->outgoing_run.cancelled+=(unsigned long)(c->queue_count-(c->queue_active && c->queue_count?1:0));
    c->queue_active=0;
    bs_queue_clear(c);
    message(c,"Transfer cancelled; remaining files discarded. Partial file removed.");
}

#include "storage.h"
#include "batch.h"
static void begin_offer(BSClient *c) {
    int len;unsigned char offer[270];
    if(c->status.incoming)return;
    if(c->out_batch && c->out_batch->tree && (!(c->remote_features&BS_CAP_DIRECTORIES) || !(c->remote_features&BS_CAP_COLLECTION))){finish_error(c,FRAME_ERROR,"Recipient does not support folder transfers; update it or send an archive explicitly");return;}
    if(c->cap_ready && (!(c->remote_caps.receive_features&BS_CAP_DETAILS) || c->status.total>c->remote_caps.max_receive_file || strlen(c->status.filename)>c->remote_caps.wire_name_bytes)) {finish_error(c,FRAME_ERROR,"Receiver cannot accept this file under its reported limits");return;}
            if(c->out_batch && (c->out_batch->count>1 || c->out_batch->tree) && (c->remote_features&BS_CAP_BATCH)) {
                if(c->remote_features&BS_CAP_COLLECTION){if(c->cap_ready && (unsigned long)c->out_batch->count>c->remote_caps.collection_files){finish_error(c,FRAME_ERROR,"Receiver cannot accept this collection size");return;}collection_send(c,1);message(c,"Sending file list...");return;}
                batch_send_offer(c);c->state=DECISION;c->decision_start=bsp_seconds();message(c,"Waiting for batch approval...");return;
            }
            len=(int)strlen(c->status.filename);
            memset(offer,0,4); put32(offer+4,c->status.total); put16(offer+8,(unsigned short)len); memcpy(offer+10,c->status.filename,(size_t)len);
#ifdef BS_OPENSSL
            c->resume_offer=(local_features(c)&c->remote_features&BS_CAP_RESUME)!=0;
            frame(c,c->resume_offer?OFFER_RESUME:OFFER,offer,10+len);
#else
            frame(c,OFFER,offer,10+len);
#endif
            c->state=DECISION; c->decision_start=bsp_seconds(); message(c,"Waiting for the recipient...");
}
static int trust_check(BSClient *c);
static void negotiation_ready(BSClient *c) {
#ifdef BS_OPENSSL
        if(!c->status.incoming && c->security_mode && c->remote_caps.security_mode && !bs_transfer_encrypted(c) && (c->remote_caps.receive_features&BS_CAP_TLS)) {
            copystr(c->upgrade_id,c->remote_id,33);frame(c,START_TLS,NULL,0);c->state=TLS_CLIENT;message(c,"Negotiating encryption...");return;
        }
#endif
        if(!bs_transfer_encrypted(c) && ((c->status.incoming && c->security_mode==BS_SECURITY_REQUIRED && (!c->remote_caps.security_mode || !(c->remote_caps.send_features&BS_CAP_TLS))) || (!c->status.incoming && c->remote_caps.security_mode==BS_SECURITY_REQUIRED))) {finish_error(c,FRAME_ERROR,"Encryption is required; enable Prefer or Force encryption on both clients");return;}
        if((!c->status.incoming || bs_transfer_encrypted(c)) && !trust_check(c))return;
        begin_offer(c);return;
}
static int commit_received(BSClient *c) {
    char backup[BS_PATH_MAX+1];unsigned long size;int i;
    if(!c->replace_pending || !bsp_size(c->dest,&size))return bsp_commit(c->temp,c->dest);
    for(i=0;i<100;i++){
        size_t prefix;const char *slash=strrchr(c->dest,'/'),*other=strrchr(c->dest,'\\'),*colon=strrchr(c->dest,':');
        if(other && (!slash || other>slash))slash=other;
        if(colon && (!slash || colon>slash))slash=colon;
        prefix=slash?(size_t)(slash-c->dest)+1:0;memcpy(backup,c->dest,prefix);sprintf(backup+prefix,"BK%04lx%02x",bsp_seconds()&0xffffUL,i);
        if(same_local_path(backup,c->dest))continue;
        if(bsp_commit(c->dest,backup))continue;
        if(!bsp_commit(c->temp,c->dest)){bsp_remove_file(backup);return 0;}
        if(bsp_commit(backup,c->dest)) {copystr(c->error,"Commit failed; original retained in same-folder BK backup",256);}
        return -1;
    }
    return -1;
}
void bs_conflict_policy(BSClient *c,int policy){if(c && policy>=0 && policy<=2)c->conflict_policy=policy;}
int bs_conflict_get(const BSClient *c){return c?c->conflict_policy:0;}
int bs_skip_incoming(BSClient *c){
    if(!c || c->state!=DECISION || !c->status.awaiting_decision || c->tx_count)return 0;
    if(!(c->remote_features&BS_CAP_SKIP)){copystr(c->error,"Sender cannot skip a file; decline or rename instead",256);return 0;}
    c->transfer_result=BS_RESULT_SKIPPED;c->status.awaiting_decision=0;
    if(c->batch_approval){c->batch_approval->next=c->in_batch_index+1;c->batch_approval->last_seen=bsp_seconds();if(c->batch_approval->next>=c->in_batch->count)batch_revoke(c);}
    frame(c,SKIP_FILE,NULL,0);c->state=CLOSING;message(c,"Skipped existing file");notify(c,BS_EVENT_COMPLETE);return 1;
}
int bs_accept_conflict(BSClient *c,const char *path){
    char folder[BS_PATH_MAX+1],unique[BS_PATH_MAX+1];const char *slash,*other,*leaf;unsigned long size;size_t prefix;
    if(!c || !path)return 0;
    if(!bsp_size(path,&size))return bs_accept(c,path);
    if(c->conflict_policy==BS_CONFLICT_SKIP)return bs_skip_incoming(c);
    if(c->conflict_policy==BS_CONFLICT_REPLACE){if(!bs_accept(c,path))return 0;c->replace_pending=1;return 1;}
    slash=strrchr(path,'/');other=strrchr(path,'\\');if(other && (!slash || other>slash))slash=other;other=strrchr(path,':');if(other && (!slash || other>slash))slash=other;
    prefix=slash?(size_t)(slash-path)+1:0;if(!prefix){strcpy(folder,".");leaf=path;}else {if(prefix>BS_PATH_MAX)return 0;memcpy(folder,path,prefix);folder[prefix]=0;leaf=path+prefix;}
    {char wire[256];if(!bs_to_utf8(leaf,wire,sizeof wire) || !bs_suggest_destination(folder,wire,0,unique,sizeof unique)){copystr(c->error,"Cannot choose a unique destination",256);return 0;}}
    return bs_accept(c,unique);
}
static void receive_verified(BSClient *c) {
        if(commit_received(c)!=0) { finish_error(c,FRAME_ERROR,"Destination exists or cannot be finalized; nothing overwritten"); return; }
        c->transfer_result=BS_RESULT_COMPLETE; c->temp[0]=0;
        if(c->in_batch && c->batch_approval) {
            c->batch_approval->next=c->in_batch_index+1;c->batch_approval->last_seen=bsp_seconds();
            if(c->batch_approval->next>=c->in_batch->count)batch_revoke(c);
        }
#ifdef BS_OPENSSL
        if(c->resume_cache){bsp_remove_file(c->resume_meta);c->resume_meta[0]=0;c->resume_cache=0;}
#endif
        frame(c,COMPLETE,NULL,0); c->state=CLOSING; message(c,"File received; written file verified (CRC32)"); notify(c,BS_EVENT_COMPLETE); return;
}
static void handle_frame(BSClient *c,int type,const unsigned char *p,int n) {
    BSPeer peer; unsigned long size; char filename[256]; int len;
    if(type==VERIFY_WAIT && n==0 && c->state==FINISHING && (c->remote_features&BS_CAP_DISK_VERIFY))return;
    if(type==SKIP_FILE && n==0 && c->state==DECISION && !c->status.incoming && (c->remote_features&BS_CAP_SKIP)) {
        c->transfer_result=BS_RESULT_SKIPPED;cleanup_transfer(c);message(c,"Recipient skipped existing file");notify(c,BS_EVENT_COMPLETE);return;
    }
    if(type==CANCEL || type==DECLINE || type==FRAME_ERROR) {
        char reason[256]; int i;
        if(c->transfer_result==BS_RESULT_NONE) c->transfer_result=type==CANCEL?BS_RESULT_CANCELLED:type==DECLINE?BS_RESULT_DECLINED:BS_RESULT_FAILED;
        /* Remote text is untrusted: printable, valid UTF-8, bounded; else generic. */
        len=n<255?n:255; memcpy(reason,p,(size_t)len); reason[len]=0;
        for(i=0;i<len;i++) if((unsigned char)reason[i]<32 || reason[i]==127) reason[i]=' ';
        if(!valid_utf8((const unsigned char *)reason)) reason[0]=0;
        fail(c,reason[0]?reason:type==DECLINE?"Transfer declined":type==CANCEL?"Peer cancelled the transfer":"Peer reported an error");
        if(type==CANCEL){c->outgoing_run.cancelled+=(unsigned long)(c->queue_count-(c->queue_active && c->queue_count?1:0));c->queue_active=0;bs_queue_clear(c);}
        return;
    }
    if(type==LIMITS && c->state==LIMIT_EXCHANGE) {
        if(n!=8 || get32(p)>BS_RATE_MAX || get32(p+4)>BS_RATE_MAX){fail(c,"Invalid transfer speed limits");return;}
        c->peer_receive_limit=get32(p+4);limits_reset(c);c->state=GREETING;negotiation_ready(c);return;
    }
    if(type==CAPABILITIES && c->state==NEGOTIATING && !c->cap_ready) {
        if(!cap_decode(p,n,&c->remote_caps)){fail(c,"Invalid capability profile");return;}
        c->cap_ready=1;c->state=GREETING;c->remote_features=(unsigned)(c->status.incoming?c->remote_caps.send_features:c->remote_caps.receive_features);
        if((c->remote_features&BS_CAP_RATE_LIMITS) && (local_features(c)&BS_CAP_RATE_LIMITS)) {
            unsigned char values[8];put32(values,c->active_send_limit);put32(values+4,c->active_receive_limit);frame(c,LIMITS,values,8);c->state=LIMIT_EXCHANGE;return;
        }
        negotiation_ready(c);return;
    }
#ifdef BS_OPENSSL
    if(type==START_TLS && n==0 && c->state==GREETING && c->got_hello && c->status.incoming && c->cap_ready && c->security_mode && !bs_transfer_encrypted(c)) {
        copystr(c->upgrade_id,c->remote_id,33);frame(c,TLS_READY,NULL,0);c->state=TLS_SERVER;return;
    }
    if(type==TLS_READY && n==0 && c->state==TLS_CLIENT && !c->status.incoming) {
        if(c->rx_count!=HEADER){fail(c,"Unexpected data before TLS upgrade");return;}
        if(!bsp_tls_upgrade(c->sock)){fail(c,"Cannot enable encrypted transport");return;}
        c->got_hello=c->cap_ready=0;c->state=GREETING;send_hello(c);return;
    }
#endif
    if(type==HELLO && c->state==GREETING && !c->got_hello) {
        memset(&peer,0,sizeof(peer)); if(n<20 || n!=20+p[18]+p[19] || !parse_greeting(p,n,&peer)) { fail(c,"Invalid peer greeting"); return; }
        if(c->expected_id[0] && strcmp(c->expected_id,peer.id)) { connection_failed(c,"Peer identity changed; refresh and try again"); return; }
        copystr(c->status.peer_name,peer.name,64); copystr(c->remote_id,peer.id,33); c->remote_port=peer.port; c->remote_features=greeting_features(p,n);c->got_hello=1;
        if(c->upgrade_id[0] && strcmp(c->upgrade_id,c->remote_id)){connection_failed(c,"Peer identity changed during encryption negotiation");return;}
        if(c->remote_features&BS_CAP_DETAILS){unsigned char caps[36];cap_encode(c,caps);frame(c,CAPABILITIES,caps,36);c->state=NEGOTIATING;return;}
        if((!c->status.incoming || bs_transfer_encrypted(c)) && !trust_check(c))return;
        begin_offer(c);
        return;
    }
#ifdef BS_OPENSSL
    if(c->security_mode==BS_SECURITY_REQUIRED && !bs_transfer_encrypted(c) && (type==OFFER || type==OFFER_RESUME || type==OFFER_BATCH || ((type>=COLLECTION_START && type<=COLLECTION_REF) || type==TREE_START || type==TREE_REF))) {finish_error(c,FRAME_ERROR,"This receiver requires encryption");return;}
#endif
    if((type==OFFER || type==OFFER_RESUME || type==OFFER_BATCH || type==COLLECTION_START || type==COLLECTION_REF || type==TREE_START || type==TREE_REF) && !trust_check(c))return;
    if(c->cap_ready && (type==OFFER || type==OFFER_RESUME) && n>=10 && (get32(p+4)>c->remote_caps.max_send_file || get16(p+8)>c->remote_caps.wire_name_bytes)) {fail(c,"Sender exceeded its reported file limit or filename limit");return;}
#ifdef BS_OPENSSL
    if(resume_frame(c,type,p,n))return;
#endif
    if(c->status.incoming && c->active_receive_limit && !(c->remote_features&BS_CAP_RATE_LIMITS) && (type==OFFER || type==OFFER_RESUME || type==OFFER_BATCH || type==COLLECTION_START || type==COLLECTION_REF || type==TREE_START || type==TREE_REF)) {finish_error(c,FRAME_ERROR,"Receive speed limit needs an updated sender; use Unlimited for legacy clients");return;}
    if(collection_frame(c,type,p,n))return;
    if(type==OFFER_BATCH && c->state==GREETING && c->got_hello && c->status.incoming && (local_features(c)&BS_CAP_BATCH)) {
        batch_receive_offer(c,p,n);return;
    }
    if((type==OFFER || (type==OFFER_RESUME && (local_features(c)&c->remote_features&BS_CAP_RESUME))) && c->state==GREETING && c->got_hello && c->status.incoming) {
        batch_revoke(c);
        if(n<11 || get32(p)!=0 || get32(p+4)>BS_FILE_MAX) { fail(c,"Unsupported file size"); return; }
        size=get32(p+4);if(c->cap_ready && size>c->remote_caps.max_send_file){fail(c,"Sender exceeded its reported file limit");return;} len=get16(p+8); if(c->cap_ready && len>c->remote_caps.wire_name_bytes){fail(c,"Sender exceeded its reported filename limit");return;} if(len<1 || len>255 || n!=10+len || memchr(p+10,0,(size_t)len)) { fail(c,"Invalid file offer"); return; }
        memcpy(filename,p+10,(size_t)len); filename[len]=0; if(!bs_safe_name(filename)) { fail(c,"Unsafe filename rejected"); return; }
#ifdef BS_OPENSSL
        c->resume_offer=type==OFFER_RESUME;
#endif
        copystr(c->status.filename,filename,256); c->status.total=size; c->status.awaiting_decision=1; c->state=DECISION; c->decision_start=bsp_seconds(); message(c,"Incoming file - waiting for your decision"); notify(c,BS_EVENT_OFFER); return;
    }
    if(type==ACCEPT && c->state==DECISION && !c->status.incoming && n==0) {
        c->file=bsp_open_file(c->source,"rb"); if(!c->file) { fail(c,"Source file is no longer readable"); return; }
        c->state=SENDING; message(c,"Sending file..."); return;
    }
    if(type==DATA && c->state==RECEIVING && n>0 && (unsigned long)n<=c->status.total-c->status.transferred) {
        if(fwrite(p,1,(size_t)n,c->file)!=(size_t)n) { fail(c,"Could not write file; check free disk space"); return; }
        c->crc=bs_crc32(c->crc,p,(size_t)n); c->status.transferred+=(unsigned long)n; c->status_dirty=1; return;
    }
    if(type==END && c->state==RECEIVING && n==4 && c->status.transferred==c->status.total) {
        if(get32(p)!=c->crc) { finish_error(c,FRAME_ERROR,"Checksum mismatch; file not delivered"); return; }
        { int r=fclose(c->file); c->file=NULL; if(r) { finish_error(c,FRAME_ERROR,"Could not finish writing file"); return; } }
        c->file=bsp_open_file(c->temp,"rb");
        if(!c->file){finish_error(c,FRAME_ERROR,"Cannot reopen received file for verification");return;}
        c->disk_crc=c->disk_bytes=0;c->last_verify=bsp_seconds();c->state=VERIFY_DISK;message(c,"Checking written file...");return;
    }

    if(type==COMPLETE && (c->state==FINISHING || (c->state==DECISION && c->out_batch && c->out_batch->tree && c->status.filename[0] && c->status.filename[strlen(c->status.filename)-1]=='/')) && n==0) {
        c->verified=(c->remote_features&BS_CAP_DISK_VERIFY)!=0 && !(c->status.filename[0] && c->status.filename[strlen(c->status.filename)-1]=='/');c->transfer_result=BS_RESULT_COMPLETE; cleanup_transfer(c); message(c,c->verified?"File delivered successfully; receiver verified written file (CRC32)":"File delivered successfully; receiver disk verification not confirmed"); notify(c,BS_EVENT_COMPLETE); return;
    }
    fail(c,"Unexpected or malformed protocol message");
}
static int flush(BSClient *c) {
    int n;
    if(!c->tx_count) return 1;
    n=bsp_send(c->sock,c->tx+c->tx_sent,c->tx_count-c->tx_sent);
    if(n<0) { if(!bsp_wouldblock()) connection_failed(c,"Connection failed while sending"); return 0; }
    if(!n) { connection_failed(c,"Connection closed while sending"); return 0; }
    c->last_io=bsp_seconds(); c->tx_sent+=n;
    if(c->state==SENDING && !strcmp(c->status.message,"Waiting for peer to respond...")) message(c,"Sending file...");
    if(c->tx_sent==c->tx_count) {
        if(c->state==SENDING && get16(c->tx+4)==DATA) { c->status.transferred+=get32(c->tx+8); c->status_dirty=1; }
        c->tx_count=c->tx_sent=0; return 1;
    } return 0;
}
/* Refuse an extra connection with a structured reason instead of a bare close,
   so the sender reports it without treating the receiver as unreachable. */
static void refuse(BSClient *c,BSSocket s,const char *reason) {
    unsigned char out[HEADER+80]; int n,length=(int)strlen(reason);
#ifdef BS_OPENSSL
    /* An encrypted client cannot read a pre-handshake application error. */
    if(c->security_mode){bsp_close(s);return;}
#endif
    if(length>80) length=80;
    if(bsp_nonblock(s)<0) { bsp_close(s); return; }
    n=frame_into(out,FRAME_ERROR,reason,length);
    if(bsp_send(s,out,n)==n) park_socket(c,s); else bsp_close(s);
}
static int rx_frame_ready(const BSClient *c) {
    return c->rx_count>=HEADER && get32(c->rx+8)<=BS_CHUNK && c->rx_count>=HEADER+(int)get32(c->rx+8);
}
static void tick(BSClient *c) {
    struct sockaddr_in from; BSSocket s; int n,type,frames,used,reads=0; unsigned long length,now=bsp_seconds();
    BSSockLen addrlen=sizeof(from);
    network_check_tick(c);
    limits_tick(c);
    if(c->status.active && now>c->rate_time) {
        c->rate=(c->status.transferred-c->rate_bytes)/(now-c->rate_time);
        c->rate_time=now; c->rate_bytes=c->status.transferred;
    }
#ifdef BS_OPENSSL
    if(bsp_tls_encrypted(c->sock))c->last_encrypted=1;
#endif
    drain_lingering(c,now);
    discovery(c); if(!c->discovery_paused && now-c->last_probe>=30) bs_refresh(c);
    /* Always drain incoming sockets, including when receiving is disabled. */
    s=accept(c->listener,(struct sockaddr *)&from,&addrlen);
    if(s!=BS_INVALID_SOCKET) {
        if(!c->config.receiving) refuse(c,s,"Recipient is not accepting files");
        else if(c->state!=IDLE) refuse(c,s,"Recipient is busy with another transfer; try again shortly");
        else if(bsp_nonblock(s)<0) bsp_close(s);
#ifdef BS_OPENSSL
        else if(!bsp_tls_attach(s,1,c->security_mode,0,c->pairing_set?c->pairing_key:NULL))bsp_close(s);
#endif
        else { bsp_watch_connection(s); reset_status(c,1,inet_ntoa(from.sin_addr)); c->sock=s; c->state=GREETING;
#ifdef SO_NOSIGPIPE
            { int yes=1; setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes)); }
#endif
            send_hello(c); message(c,"Connecting to sender..."); }
    }
    if(c->state==VERIFY_DISK) {
        unsigned char *data=c->tx+HEADER;size_t got;int budget=BS_RECV_FRAMES;
        if(!c->tx_count && (c->remote_features&BS_CAP_DISK_VERIFY) && now-c->last_verify>=2){frame(c,VERIFY_WAIT,NULL,0);c->last_verify=now;}
        if(c->tx_count){flush(c);if(c->tx_count || c->state!=VERIFY_DISK)return;}
        c->last_io=now;
        while(budget-- && c->state==VERIFY_DISK){
            got=fread(data,1,BS_CHUNK,c->file);
            if(got){c->disk_crc=bs_crc32(c->disk_crc,data,got);c->disk_bytes+=(unsigned long)got;}
            if(got<BS_CHUNK){int bad=ferror(c->file);fclose(c->file);c->file=NULL;
                if(bad || c->disk_bytes!=c->status.total || c->disk_crc!=c->crc)finish_error(c,FRAME_ERROR,"Written file failed integrity check; file not delivered");
                else {c->verified=1;receive_verified(c);}break;
            }
        }
    }
    if(c->state==IDLE) return;
    if(c->state==DECISION && now-c->decision_start>120) { abort_transfer(c,FRAME_ERROR,"Recipient decision timed out","Recipient decision timed out",BS_RESULT_FAILED); return; }
    if(c->state!=DECISION && now-c->last_io>30) { connection_failed(c,c->state==FINISHING?"Delivery unconfirmed; check the receiver before retrying":"Peer stopped responding; transfer timed out"); return; }
    if(c->state==CONNECTING) {
        n=bsp_connected(c->sock); if(n<0) connection_failed(c,"Could not connect; check IP address and firewall");
        else if(n>0) { c->state=GREETING; send_hello(c); }
        return;
    }
    if(c->state==SENDING && now-c->last_io>=5 && strcmp(c->status.message,"Waiting for peer to respond...")) message(c,"Waiting for peer to respond...");
    /* A blocked writer must still read EOF, cancellation and network errors. */
    flush(c); if(c->state==IDLE) return;
    if(c->state==TREE_PREPARE && !c->tx_count)tree_prepare_tick(c);
    if(c->state==CLOSING) {
        if(!c->tx_count) { park_socket(c,c->sock); c->sock=BS_INVALID_SOCKET; cleanup_transfer(c); notify(c,BS_EVENT_STATUS); }
        return;
    }
#ifdef BS_OPENSSL
    resume_tick(c,now);if(c->state==IDLE)return;
#endif
#ifdef BS_OPENSSL
    if(c->state==TLS_SERVER && !c->tx_count) {
        if(c->rx_count){fail(c,"Unexpected data before TLS upgrade");return;}
        if(!bsp_tls_upgrade(c->sock)){fail(c,"Cannot enable encrypted transport");return;}
        c->got_hello=c->cap_ready=0;c->state=GREETING;send_hello(c);return;
    }
#endif
    /* Bounded work keeps cooperative desktops responsive. Complete buffered frames
       are handled before reading more, so a peer's final frame precedes its EOF. */
    for(frames=0;frames<BS_RECV_FRAMES && c->state!=IDLE;) {
        if(c->rx_count>=HEADER) {
            if(memcmp(c->rx,"BSK1",4) || get16(c->rx+6)!=0) { fail(c,"Invalid frame header"); return; }
            length=get32(c->rx+8); if(length>BS_CHUNK) { fail(c,"Frame too large"); return; }
            if(c->rx_count>=HEADER+(int)length) {
                type=get16(c->rx+4);
                /* Preserve a queued/partial outgoing frame. Terminal peer messages can
                   abort it; normal replies wait until the writer has drained. */
                if(c->tx_count && !terminal_type(type)) break;
                if(type==DATA && c->state==RECEIVING && !limits_receive(c,length))break;
                used=HEADER+(int)length;
                handle_frame(c,type,c->rx+HEADER,(int)length); frames++;
                if(c->state==IDLE) return;
                c->rx_count-=used; if(c->rx_count) memmove(c->rx,c->rx+used,(size_t)c->rx_count);
                if(c->tx_count || c->state==CLOSING) break;
                continue;
            }
        }
        if(reads++>=2*BS_RECV_FRAMES+2) break;
        n=bsp_recv(c->sock,c->rx+c->rx_count,RX_CAP-c->rx_count);
        if(n==0) {
            /* Closed before any greeting: the host answered, so keep its catalog entry. */
            if(c->state==GREETING && c->status.incoming && !c->got_hello && !c->rx_count){cleanup_transfer(c);message(c,"Ready on your local network");}
            else if(c->state==GREETING) fail(c,"Peer closed the connection");
            else connection_failed(c,c->state==FINISHING?"Completion unconfirmed; check the receiver before retrying":"Peer disconnected; transfer interrupted");
            return;
        }
        if(n<0) { if(!bsp_wouldblock()) { if(c->state==GREETING) fail(c,"Network error"); else connection_failed(c,"Network error"); } break; }
        c->rx_count+=n; c->last_io=now;
    }
    if(c->state==IDLE) return;
    for(frames=0;frames<BS_SEND_BURST && c->state==LIST_SENDING && !c->tx_count;frames++){collection_tick(c);flush(c);}
    for(frames=0;frames<BS_SEND_BURST && c->state==SENDING && !c->tx_count;frames++) {
        if(c->status.transferred<c->status.total) {
            size_t count;
            length=c->status.total-c->status.transferred; if(length>BS_CHUNK) length=BS_CHUNK;
            length=limits_send(c,length);if(!length)break;
            count=fread(c->tx+HEADER,1,(size_t)length,c->file);
            if(count!=(size_t)length) { fail(c,"Source file changed or could not be read"); return; }
            c->crc=bs_crc32(c->crc,c->tx+HEADER,count); frame(c,DATA,NULL,(int)count);
        } else {
            if(fgetc(c->file)!=EOF || ferror(c->file)) { fail(c,"Source file changed while sending"); return; }
            fclose(c->file); c->file=NULL; put32(c->tx+HEADER,c->crc); frame(c,END,NULL,4); c->state=FINISHING; message(c,"Verifying delivery...");
        }
        flush(c);
    }
#ifdef BS_OPENSSL
    /* Read phase-changing replies before generating optional keepalives. */
    resume_keepalive(c,now);
#endif
}
#include "queue.h"
void bs_tick(BSClient *c) {
    tick(c);
    queue_tick(c);
    /* One progress notification per tick, however many chunks moved. */
    if(c->status_dirty) { c->status_dirty=0; if(c->status.active) notify(c,BS_EVENT_STATUS); }
}
/* Work that can proceed without waiting for a socket: file reads to queue,
   a drained CLOSING writer, or a complete frame left over from the last budget. */
static int immediate_work(const BSClient *c) {
    if(c->retry || c->state==VERIFY_DISK || c->state==TREE_PREPARE)return 1;
    if(c->state==IDLE && c->queue_count && !c->queue_paused)return 1;
#ifdef BS_OPENSSL
    if(c->state==CHECK_RECEIVE || c->state==CHECK_SEND)return 1;
#endif
    if(c->state==LIST_SENDING && !c->tx_count)return 1;
    if(c->state==SENDING && !c->tx_count && (!c->upload.rate || c->status.transferred==c->status.total || c->upload.credit>=limits_piece(c)))return 1;
    if(c->state==CLOSING && !c->tx_count) return 1;
    return c->state!=IDLE && c->state!=CONNECTING && !c->tx_count && rx_frame_ready(c) && !limits_read_blocked(c);
}
void bs_service(BSClient *c,unsigned long budget_ms) {
    unsigned long start;int steps=0;
    if(!c)return;
    if(budget_ms>8)budget_ms=8;
    start=bsp_millis();
    do {bs_tick(c);steps++;}
    while(steps<64 && immediate_work(c) && ((bsp_millis()-start)&0xffffffffUL)<budget_ms);
}
unsigned long bs_wait(BSClient *c,unsigned long sigmask,unsigned long timeout_ms) {
    BSSocket reads[3+LINGER_MAX],writer=BS_INVALID_SOCKET; int n=0,i;
    reads[n++]=c->listener; reads[n++]=c->udp;
    if(c->sock!=BS_INVALID_SOCKET) {
        if(c->state==CONNECTING || c->tx_count) writer=c->sock;
        if(c->state!=CONNECTING && !limits_read_blocked(c)) reads[n++]=c->sock;
    }
    for(i=0;i<LINGER_MAX;i++) if(c->linger[i]!=BS_INVALID_SOCKET) reads[n++]=c->linger[i];
    if(timeout_ms>1000) timeout_ms=1000;
    if((c->upload.rate || c->download.rate) && timeout_ms>20)timeout_ms=20;
    if(immediate_work(c)) timeout_ms=0;
    return bsp_wait(reads,n,writer,sigmask,timeout_ms);
}
unsigned long bs_transfer_rate(const BSClient *c) { return c->status.active?c->rate:0; }
int bs_peer_count(const BSClient *c) { return c->peer_count; }
const BSPeer *bs_peer(const BSClient *c,int index) { return index>=0 && index<c->peer_count ? &c->peers[index] : NULL; }
const BSStatus *bs_status(const BSClient *c) { return &c->status; }
const char *bs_last_error(const BSClient *c) { return c->error; }

BSClient *bs_create_app(const char *name,const char *platform,const char *id,int visible,int receiving) {
    BSConfig config; memset(&config,0,sizeof(config));
    copystr(config.name,name,64); copystr(config.platform,platform,32); copystr(config.id,id,33);
    config.visible=visible; config.receiving=receiving; config.port=BS_PORT;
    return bs_create(&config,NULL,NULL);
}

unsigned long bs_transfer_serial(const BSClient *c) { return c->transfer_serial; }
BSResult bs_transfer_result(const BSClient *c) { return c->transfer_result; }

int bs_security_available(void) {
#ifdef BS_OPENSSL
    return 1;
#else
    return 0;
#endif
}
int bs_generate_pairing_key(char out[65]) {
#ifdef BS_OPENSSL
    return bsp_pairing_key(out);
#else
    (void)out;return 0;
#endif
}
int bs_security_mode(const BSClient *c) {return c?c->security_mode:0;}
int bs_transfer_encrypted(const BSClient *c) {
#ifdef BS_OPENSSL
    return c && (c->last_encrypted || bsp_tls_encrypted(c->sock));
#else
    (void)c;return 0;
#endif
}
int bs_security_configure(BSClient *c,int mode,const char *key) {
    if(!c)return 0;
    if(c->status.active || c->queue_count){copystr(c->error,"Wait until the transfer queue is empty before changing encryption",256);return 0;}
    if(mode<0 || mode>2){copystr(c->error,"Invalid encryption mode",256);return 0;}
#ifdef BS_OPENSSL
    {unsigned char decoded[32];int i,a,b;if(mode && key && *key) {if(strlen(key)!=64){copystr(c->error,"Pairing key must contain 64 hexadecimal characters",256);return 0;}
     for(i=0;i<32;i++){a=hexval((char)tolower((unsigned char)key[i*2]));b=hexval((char)tolower((unsigned char)key[i*2+1]));if(a<0 || b<0){OPENSSL_cleanse(decoded,32);copystr(c->error,"Invalid pairing key",256);return 0;}decoded[i]=(unsigned char)((a<<4)|b);}memcpy(c->pairing_key,decoded,32);c->pairing_set=1;OPENSSL_cleanse(decoded,32);
    }else {OPENSSL_cleanse(c->pairing_key,32);c->pairing_set=0;}c->security_mode=mode;bs_refresh(c);return 1;}
#else
    (void)key;if(mode){copystr(c->error,"Encryption is unavailable in this client build",256);return 0;}return 1;
#endif
}
unsigned bs_peer_features(const BSClient *c,const char *id) {int i;unsigned flags=0;if(!c || !id)return 0;for(i=0;i<c->peer_count;i++)if(!strcmp(c->peers[i].id,id))flags|=c->peer_features[i];return flags;}

#include "activity.h"

#include "trust.h"
