#include <stdlib.h>
#include <string.h>
#include "transprt.h"
static TPL table={"01.26"};
static void *lookup(const char *name){(void)name;return &table;}
static DRV_LIST driver={"STiKmagic",lookup};
static struct {int type,state,count,locked;CAB cab;char data[1024];} handles[1000];
static int next_handle,last_tcp,last_udp,last_send_port,last_send_count,send_error;
int Getcookie(long magic,long *value){(void)magic;*value=(long)&driver;return 0;}
const char *getvstr(const char *name){(void)name;if(!tpl)abort();return "Test Atari";}
int TCP_open(unsigned long host,unsigned short port,unsigned short tos,unsigned short buffer){int h=++next_handle;(void)tos;(void)buffer;handles[h].type=1;handles[h].state=host?TSYN_SENT:TLISTEN;handles[h].cab.rhost=host;handles[h].cab.rport=port;last_tcp=h;return h;}
int TCP_close(int h,int mode,short *result){(void)mode;(void)result;handles[h].state=TCLOSED;return 0;}
int TCP_info(int h,TCPIB *info){if(info->request==TCPI_state)info->state=handles[h].state;return 0;}
int TCP_send(int h,const void *p,short count){(void)h;(void)p;last_send_count=count;return send_error;}
int UDP_open(unsigned long host,unsigned short port){int h=++next_handle;handles[h].type=2;if(host && !port)handles[h].cab=*(CAB *)host;else handles[h].cab.lport=port;last_udp=h;return h;}
int UDP_close(int h){(void)h;return 0;}
int UDP_info(int h,UDPIB *info){(void)h;(void)info;return 0;}
int UDP_send(int h,const void *p,short n){(void)p;(void)n;last_send_port=handles[h].cab.lport;return 0;}
CIB *CNgetinfo(int h){static CIB cib;cib.address=handles[h].cab;return &cib;}
int CNbyte_count(int h){return handles[h].count;}
int CNget_block(int h,void *out,short n){memcpy(out,handles[h].data,n);handles[h].count-=n;return n;}
NDB *CNget_NDB(int h){NDB *n;if(handles[h].locked)return (NDB *)-27L;if(!handles[h].count)return NULL;n=calloc(1,sizeof *n);n->ptr=malloc(handles[h].count);n->ndata=n->ptr;n->len=handles[h].count;memcpy(n->ptr,handles[h].data,n->len);handles[h].count=0;return n;}
void KRfree(void *p){free(p);}
int test_tcp(void){return last_tcp;}int test_udp(void){return last_udp;}
int test_port(void){return last_send_port;}int test_count(void){return last_send_count;}
void test_state(int h,int state){handles[h].state=state;}
void test_source(int h,unsigned long host,unsigned short port){handles[h].cab.rhost=host;handles[h].cab.rport=port;}
void test_data(int h,const char *data,int n){handles[h].count=n;if(n>0)memcpy(handles[h].data,data,n);}
void test_locked(int h,int n){handles[h].locked=n;}
void test_send_error(int n){send_error=n;}
