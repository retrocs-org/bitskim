/* Deterministic clock/backpressure injection around the actual production engine. */
#include "../core/bitskim.h"
#include "../core/platform.h"
#include <errno.h>
static unsigned long clock_offset,millis_offset;
void bs_test_advance_ms(unsigned long ms){millis_offset+=ms;}
static unsigned long test_millis(void){return (bsp_millis()+clock_offset*1000UL+millis_offset)&0xffffffffUL;}
static long write_remaining=-1;static int close_failed;static FILE *receive_file;
static FILE *test_create_file(const char *path){receive_file=bsp_create_file(path);return receive_file;}
void bs_test_write_limit(long bytes){write_remaining=bytes;}
void bs_test_fail_close(int value){close_failed=value;}
static size_t test_fwrite(const void *p,size_t size,size_t count,FILE *file){
    size_t allowed=count,got;
    if(write_remaining<0 || file!=receive_file)return fwrite(p,size,count,file);
    if(size && (unsigned long)write_remaining/size<count)allowed=(size_t)((unsigned long)write_remaining/size);
    got=fwrite(p,size,allowed,file);write_remaining-=(long)(got*size);
    if(allowed<count)errno=ENOSPC;
    return got;
}
static int test_fclose(FILE *file){int result=fclose(file);if(close_failed && file==receive_file){close_failed=0;errno=ENOSPC;return EOF;}return result;}
static int blocked,send_limit,broadcasts,temp_failed,space_mode=-1;static unsigned long space_hi,space_lo,space_unit;
void bs_test_space(int mode,unsigned long hi,unsigned long lo,unsigned long unit){space_mode=mode;space_hi=hi;space_lo=lo;space_unit=unit;}
static int test_disk_free(const char *path,unsigned long *hi,unsigned long *lo,unsigned long *unit){if(space_mode<0)return bsp_disk_free(path,hi,lo,unit);*hi=space_hi;*lo=space_lo;*unit=space_unit;return space_mode;}
void bs_test_fail_temp(int value){temp_failed=value;}
static FILE *test_tmpfile(void){return temp_failed?NULL:tmpfile();}
void bs_test_reset(void) { write_remaining=-1;close_failed=0;receive_file=NULL;clock_offset=millis_offset=0; blocked=0; send_limit=0; broadcasts=0; temp_failed=0;space_mode=-1; }
void bs_test_advance(unsigned long seconds) { clock_offset+=seconds; }
void bs_test_send_limit(int value) { send_limit=value; }
void bs_test_block_send(int value) { blocked=value; }
static unsigned long test_seconds(void) { return bsp_seconds()+clock_offset; }
static int test_send(BSSocket s,const void *p,int n) {
    if(blocked) { errno=EWOULDBLOCK; return -1; }
    if(send_limit && n>send_limit) n=send_limit;
    return bsp_send(s,p,n);
}
static void test_broadcast(BSSocket s,const void *p,int n,unsigned short port) { broadcasts++; bsp_broadcast(s,p,n,port); }
int bs_test_broadcast_count(void) { return broadcasts; }
#define bsp_create_file test_create_file
#define fwrite test_fwrite
#define fclose test_fclose
#define bsp_disk_free test_disk_free
#define bsp_broadcast test_broadcast
#define bsp_seconds test_seconds
#define bsp_millis test_millis
#define bsp_send test_send
#define tmpfile test_tmpfile
#include "../core/bitskim.c"
int bs_test_batch_token(const BSClient *c,unsigned char *out) {if(!c->in_batch)return 0;memcpy(out,c->in_batch->token,16);return 1;}

void bs_test_peer_endpoint(BSClient *c,const char *id,const char *address,unsigned short port) {
    BSPeer *p;if(c->peer_count>=BS_MAX_PEERS)return;p=&c->peers[c->peer_count++];memset(p,0,sizeof *p);
    copystr(p->id,id,sizeof p->id);copystr(p->address,address,sizeof p->address);copystr(p->name,"Test endpoint",sizeof p->name);p->port=port;p->last_seen=bsp_seconds();
}
