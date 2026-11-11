/* STinG 1.26 adapter. No POSIX sockets or compiler-short ABI assumptions.
   transprt.h is fetched as an external SDK, never vendored in this project. */
#define BS_ATARI_NO_SOCKET_MACROS
#include "platform.h"
#include <transprt.h>
#include <mint/cookie.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
DRV_LIST *drivers;
TPL *tpl;
#ifdef __m68k__
#include <stddef.h>
typedef char bs_atari_abi[(sizeof(int)==4 && sizeof(long)==4 && sizeof(CAB)==12 && offsetof(CIB,address)==2 && offsetof(DRV_LIST,get_dftab)==10)?1:-1];
#endif
#define SLOTS 12
static struct {int used,type,handle,listening;unsigned short port;} slots[SLOTS];
static int blocked;
static int valid(int s){return s>=0 && s<SLOTS && slots[s].used;}
static int alloc_slot(int type){int i;for(i=0;i<SLOTS;i++)if(!slots[i].used){memset(&slots[i],0,sizeof slots[i]);slots[i].used=1;slots[i].type=type;slots[i].handle=-1;return i;}return -1;}
static int failure(int rc){blocked=rc==E_OBUFFULL || rc==E_NODATA || rc==E_LISTEN || rc==E_LOCKED;return -1;}
static void defer_tcp(int handle){TCPIB info;memset(&info,0,sizeof info);info.request=TCPI_defer;TCP_info(handle,&info);}
static void defer_udp(int handle){UDPIB info;memset(&info,0,sizeof info);info.request=UDPI_defer;UDP_info(handle,&info);}
static int passive_udp(int s){int h=UDP_open(0,slots[s].port);slots[s].handle=h;if(h<0)return failure(h);defer_udp(h);return 0;}
static int passive_tcp(int s){int h=TCP_open(0,slots[s].port,0,8192);slots[s].handle=h;if(h<0)return failure(h);defer_tcp(h);return 0;}
int bs_platform_init(void){long cookie;memset(slots,0,sizeof slots);if(Getcookie(STIK_COOKIE_MAGIC,&cookie)!=C_FOUND)return 0;drivers=(DRV_LIST *)cookie;if(!drivers || memcmp(drivers->magic,STIK_DRVR_MAGIC,9))return 0;tpl=(TPL *)get_dftab(TRANSPORT_DRIVER);return tpl && tpl->version && strcmp(tpl->version,"01.26")>=0;}
void bs_platform_cleanup(void){int i;for(i=0;i<SLOTS;i++)if(slots[i].used)bsp_close(i);tpl=NULL;drivers=NULL;}
void bs_hostname(char *out,size_t n){const char *s=getvstr("HOSTNAME");size_t i;if(!s || !strcmp(s,"0"))s="Atari-ST";for(i=0;i+1<n && s[i] && (unsigned char)s[i]>=32 && (unsigned char)s[i]<127;i++)out[i]=s[i];out[i]=0;}
unsigned short bs_at_htons(unsigned short n){return n;}
unsigned long bs_at_inet_addr(const char *s){unsigned long n=0;int i;const char *p=s;char *end;if(!p)return INADDR_NONE;for(i=0;i<4;i++){unsigned long part;if(*p<'0'||*p>'9')return INADDR_NONE;part=strtoul(p,&end,10);if(end==p || part>255 || (i<3?*end!='.':*end!=0))return INADDR_NONE;n=(n<<8)|part;p=end+1;}return n;}
char *bs_at_inet_ntoa(struct in_addr a){static char text[16];sprintf(text,"%lu.%lu.%lu.%lu",(a.s_addr>>24)&255,(a.s_addr>>16)&255,(a.s_addr>>8)&255,a.s_addr&255);return text;}
int bs_at_socket(int family,int type,int protocol){(void)protocol;blocked=0;if(family!=AF_INET || (type!=SOCK_STREAM && type!=SOCK_DGRAM))return -1;return alloc_slot(type);}
int bs_at_bind(int s,const struct sockaddr *p,int length){const struct sockaddr_in *a=(const struct sockaddr_in *)p;(void)length;if(!valid(s)||a->sin_addr.s_addr)return -1;slots[s].port=a->sin_port;return slots[s].type==SOCK_DGRAM?passive_udp(s):0;}
int bs_at_listen(int s,int backlog){(void)backlog;if(!valid(s)||slots[s].type!=SOCK_STREAM)return -1;slots[s].listening=1;return passive_tcp(s);}
int bs_at_accept(int s,struct sockaddr *p,int *length){int accepted,rc;TCPIB info;CIB *cib;struct sockaddr_in a;
    if(!valid(s)||!slots[s].listening)return -1;
    if(slots[s].handle<0){passive_tcp(s);blocked=1;return -1;}
    memset(&info,0,sizeof info);info.request=TCPI_state;rc=TCP_info(slots[s].handle,&info);
    if(rc<0){if(rc!=E_LOCKED){TCP_close(slots[s].handle,TCP_IMMEDIATE,NULL);passive_tcp(s);}return failure(rc);}
    if(info.state!=TESTABLISH && info.state!=TCLOSE_WAIT){if(info.state==TCLOSED || info.state>TCLOSE_WAIT){TCP_close(slots[s].handle,TCP_IMMEDIATE,NULL);passive_tcp(s);}blocked=1;return -1;}
    accepted=alloc_slot(SOCK_STREAM);if(accepted<0){blocked=1;return -1;}
    cib=CNgetinfo(slots[s].handle);if(!cib){slots[accepted].used=0;blocked=1;return -1;}memset(&a,0,sizeof a);a.sin_family=AF_INET;
    if(cib){a.sin_port=cib->address.rport;a.sin_addr.s_addr=cib->address.rhost;}
    slots[accepted].handle=slots[s].handle;slots[s].handle=-1;passive_tcp(s);
    if(p && length && *length>=(int)sizeof a){memcpy(p,&a,sizeof a);*length=sizeof a;}return accepted;
}
int bs_at_connect(int s,const struct sockaddr *p,int length){const struct sockaddr_in *a=(const struct sockaddr_in *)p;int h;(void)length;if(!valid(s))return -1;h=TCP_open(a->sin_addr.s_addr,a->sin_port,0,8192);if(h<0)return failure(h);slots[s].handle=h;defer_tcp(h);blocked=1;return -1;}
int bsp_connected(int s){TCPIB info;int rc;if(!valid(s)||slots[s].handle<0)return -1;memset(&info,0,sizeof info);info.request=TCPI_state;rc=TCP_info(slots[s].handle,&info);if(rc==E_LOCKED)return 0;if(rc<0)return -1;if(info.state==TESTABLISH)return 1;if(info.state==TCLOSED || info.state>=TCLOSE_WAIT)return -1;return 0;}
int bsp_nonblock(int s){return valid(s)?0:-1;}
void bsp_watch_connection(int s){(void)s;}
int bsp_wouldblock(void){return blocked;}
int bsp_send(int s,const void *p,int n){int rc;blocked=0;if(!valid(s)||slots[s].handle<0)return -1;if(n>4096)n=4096;rc=TCP_send(slots[s].handle,p,(short)n);return rc==E_NORMAL?n:failure(rc);}
int bsp_recv(int s,void *p,int n){int count,rc;blocked=0;if(!valid(s)||slots[s].handle<0)return -1;count=CNbyte_count(slots[s].handle);if(count==E_EOF)return 0;if(count<=0){if(!count)count=E_NODATA;return failure(count);}if(n>count)n=count;if(n>4096)n=4096;rc=CNget_block(slots[s].handle,p,(short)n);if(rc==E_EOF)return 0;return rc>=0?rc:failure(rc);}
int bs_at_setsockopt(int s,int level,int option,const char *value,int n){(void)level;(void)option;(void)value;(void)n;return valid(s)?0:-1;}
int bs_at_shutdown(int s,int how){(void)how;if(!valid(s)||slots[s].handle<0)return -1;return TCP_close(slots[s].handle,TCP_HALFDUPLEX,NULL);}
int bsp_close(int s){int h;if(!valid(s))return -1;h=slots[s].handle;if(h>=0){if(slots[s].type==SOCK_DGRAM)UDP_close(h);else TCP_close(h,TCP_IMMEDIATE,NULL);}memset(&slots[s],0,sizeof slots[s]);return 0;}
int bs_at_sendto(int s,const char *p,int n,int flags,const struct sockaddr *address,int length){CAB cab;int h,rc;const struct sockaddr_in *a=(const struct sockaddr_in *)address;(void)flags;(void)length;blocked=0;if(!valid(s)||n>512)return -1;memset(&cab,0,sizeof cab);cab.lport=slots[s].port;cab.rhost=a->sin_addr.s_addr;cab.rport=a->sin_port;h=UDP_open((unsigned long)&cab,UDP_EXTEND);if(h<0)return failure(h);defer_udp(h);rc=UDP_send(h,p,(short)n);UDP_close(h);return rc==E_NORMAL?n:failure(rc);}
int bs_at_recvfrom(int s,char *p,int n,int flags,struct sockaddr *address,int *length){NDB *data;CIB *cib;struct sockaddr_in a;int count;(void)flags;blocked=0;if(!valid(s))return -1;if(slots[s].handle<0){passive_udp(s);blocked=1;return -1;}data=CNget_NDB(slots[s].handle);if(!data){blocked=1;return -1;}if((long)data<0)return failure((int)(long)data);memset(&a,0,sizeof a);a.sin_family=AF_INET;cib=CNgetinfo(slots[s].handle);if(cib){a.sin_port=cib->address.rport;a.sin_addr.s_addr=cib->address.rhost;}count=data->len;if(count>n)count=n;memcpy(p,data->ndata,count);KRfree(data->ptr);KRfree(data);
    /* A passive STinG UDP handle locks to its first sender. Reopen it after
       every datagram so subsequent peers cannot inherit the first address. */
    UDP_close(slots[s].handle);passive_udp(s);
    if(!cib){blocked=1;return -1;}if(address && length && *length>=(int)sizeof a){memcpy(address,&a,sizeof a);*length=sizeof a;}return count;
}
void bsp_broadcast(int s,const void *p,int n,unsigned short port){struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_port=port;a.sin_addr.s_addr=INADDR_NONE;bs_at_sendto(s,p,n,0,(struct sockaddr *)&a,sizeof a);}
unsigned long bsp_wait(const int *r,int count,int w,unsigned long mask,unsigned long timeout){(void)r;(void)count;(void)w;(void)mask;(void)timeout;return 0;}
