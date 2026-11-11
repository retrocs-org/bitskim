/* BitSkim adapter for Michael B. Brutman's mTCP. Linked DOS build: GPLv3+. */
#include <stdio.h>
#include <stddef.h>
#define BS_DOS_NO_SOCKET_MACROS
extern "C" {
#include "bitskim.h"
#include "platform.h"
}
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <time.h>
#include "types.h"
#include "packet.h"
#include "arp.h"
#include "ip.h"
#include "tcp.h"
#include "tcpsockm.h"
#include "udp.h"
#include "timer.h"
#include "utils.h"
#define SLOT_COUNT 12
struct Slot { int type,closing; unsigned short port; TcpSocket *tcp; };
static Slot slots[SLOT_COUNT];
static int blocked,started,udp_slot=-1;
static unsigned short next_port=49152;
/* Explicit launcher opt-in; ordinary DOS/LAN addresses are never rewritten. */
static unsigned long nat_host;
static unsigned long remote_address(unsigned long address) { return nat_host && (address&255UL)==127UL?nat_host:address; }
static volatile int interrupted;
struct Datagram { int length; sockaddr_in from; unsigned char bytes[512]; };
static Datagram datagrams[4]; static int head,count;
struct PendingUdp { int used,length,slot; unsigned short port; IpAddr_t ip; unsigned long since; unsigned char bytes[512]; };
static PendingUdp pending_udp[4];
static void __interrupt __far break_handler(void) { interrupted=1; }
static int valid(int s) { return s>=0 && s<SLOT_COUNT && slots[s].type && !slots[s].closing; }
static int allocate(int type) { int i; for(i=0;i<SLOT_COUNT;i++) if(!slots[i].type) { memset(&slots[i],0,sizeof slots[i]); slots[i].type=type; return i; } return -1; }
static void udp_callback(const unsigned char *packet,const UdpHeader *udp) {
    int n=ntohs(udp->len)-sizeof(UdpHeader);
    if(n>=0 && n<=512 && count<4) {
        Datagram *d=&datagrams[(head+count)%4]; const IpHeader *ip=(const IpHeader *)(packet+sizeof(EthHeader));
        d->length=n; memset(&d->from,0,sizeof d->from); d->from.sin_family=AF_INET; d->from.sin_port=udp->src;
        memcpy(&d->from.sin_addr.s_addr,ip->ip_src,4); d->from.sin_addr.s_addr=remote_address(d->from.sin_addr.s_addr); memcpy(d->bytes,udp+1,n); count++;
    }
    Buffer_free(packet);
}
extern "C" int bs_dos_break(void) { return interrupted; }
extern "C" void bs_dos_pump(void) {
    int i; if(!started) return; Utils::processPackets();
    for(i=0;i<4;i++) if(pending_udp[i].used) {
        PendingUdp *p=&pending_udp[i];
        if(!valid(p->slot) || slots[p->slot].type!=SOCK_DGRAM || TIMER_GET_CURRENT()-p->since>=54UL) p->used=0;
        else { int rc=Udp::sendUdp(p->ip,slots[p->slot].port,p->port,p->length,p->bytes,0); if(rc!=1) p->used=0; }
    }
    for(i=0;i<SLOT_COUNT;i++) if(slots[i].closing && slots[i].tcp->isCloseDone()) { TcpSocketMgr::freeSocket(slots[i].tcp); memset(&slots[i],0,sizeof slots[i]); }
}
extern "C" int bs_platform_init(void) {
    const char *host=getenv("BITSKIM_NAT_HOST");
    if(host) {nat_host=bs_dos_inet_addr(host);if(nat_host==INADDR_NONE || !nat_host || (nat_host&255UL)==127UL) {puts("Invalid BITSKIM_NAT_HOST IPv4 address.");return 0;}}
    if(Utils::parseEnv()) return 0;
    if(Utils::initStack(10,8,break_handler,break_handler)) return 0;
    started=1; return 1;
}
extern "C" void bs_platform_cleanup(void) {
    if(started) {
        unsigned long began=TIMER_GET_CURRENT(); int i,pending;
        /* mTCP queues sends in user space. Service final COMPLETE/CANCEL/FIN
           before removing the packet/timer handlers; bound shutdown to 3 s. */
        do { pending=0; bs_dos_pump(); for(i=0;i<SLOT_COUNT;i++) if(slots[i].closing) pending=1;
        } while(pending && TIMER_GET_CURRENT()-began<54UL);
        Utils::endStack(); started=0;
    }
}
extern "C" int bs_dos_socket(int family,int type,int protocol) {
    int s; (void)protocol; blocked=0; if(family!=AF_INET || (type!=SOCK_STREAM && type!=SOCK_DGRAM)) return -1;
    s=allocate(type); if(s<0) return -1;
    if(type==SOCK_STREAM) { slots[s].tcp=TcpSocketMgr::getSocket(); if(!slots[s].tcp || slots[s].tcp->setRecvBuffer(8192)) { if(slots[s].tcp) TcpSocketMgr::freeSocket(slots[s].tcp); memset(&slots[s],0,sizeof slots[s]); return -1; } }
    return s;
}
extern "C" int bs_dos_bind(int s,const sockaddr *address,int length) {
    const sockaddr_in *a=(const sockaddr_in *)address; (void)length;
    if(!valid(s)) return -1; slots[s].port=ntohs(a->sin_port);
    if(slots[s].type==SOCK_DGRAM) { if(udp_slot>=0) return -1; if(!slots[s].port) slots[s].port=Udp::getUnusedCallbackPort(); if(Udp::registerCallback(slots[s].port,udp_callback)) return -1; udp_slot=s; } return 0;
}
extern "C" int bs_dos_listen(int s,int backlog) { (void)backlog; return valid(s) && slots[s].tcp?slots[s].tcp->listen(slots[s].port,8192):-1; }
extern "C" int bs_dos_accept(int listener,sockaddr *address,int *length) {
    TcpSocket *tcp; int s; blocked=0; if(!valid(listener)) return -1;
    s=allocate(SOCK_STREAM); if(s<0) return -1;
    tcp=TcpSocketMgr::accept(); if(!tcp) { memset(&slots[s],0,sizeof slots[s]); blocked=1; return -1; }
    slots[s].tcp=tcp;
    if(address && length && *length>=(int)sizeof(sockaddr_in)) { sockaddr_in *a=(sockaddr_in *)address; memset(a,0,sizeof *a); a->sin_family=AF_INET; a->sin_port=htons(tcp->dstPort); memcpy(&a->sin_addr.s_addr,tcp->dstHost,4); a->sin_addr.s_addr=remote_address(a->sin_addr.s_addr); *length=sizeof *a; } return s;
}
extern "C" int bs_dos_connect(int s,const sockaddr *address,int length) {
    const sockaddr_in *a=(const sockaddr_in *)address; IpAddr_t ip; int rc; (void)length;
    if(!valid(s) || !slots[s].tcp) return -1;
    int i,collision;
    do { next_port++; if(next_port<49152) next_port=49152; collision=0;
        for(i=0;i<SLOT_COUNT;i++) if(slots[i].type && slots[i].port==next_port) collision=1;
    } while(collision);
    slots[s].port=next_port;
    memcpy(ip,&a->sin_addr.s_addr,4); rc=slots[s].tcp->connectNonBlocking(next_port,ip,ntohs(a->sin_port));
    blocked=rc==0; return -1; /* completion is polled through bsp_connected */
}
extern "C" int bsp_connected(int s) { if(!valid(s) || !slots[s].tcp) return -1; bs_dos_pump(); if(slots[s].tcp->isConnectComplete()) return 1; return slots[s].tcp->isClosed()?-1:0; }
extern "C" int bsp_nonblock(int s) { return valid(s)?0:-1; }
extern "C" int bsp_wouldblock(void) { return blocked; }
extern "C" int bsp_send(int s,const void *p,int n) {
    int rc; blocked=0; if(!valid(s) || !slots[s].tcp) return -1;
    rc=slots[s].tcp->send((unsigned char *)p,n); if(!rc) { blocked=1; return -1; } return rc;
}
extern "C" int bsp_recv(int s,void *p,int n) {
    int rc; blocked=0; if(!valid(s) || !slots[s].tcp) return -1;
    bs_dos_pump(); rc=slots[s].tcp->recv((unsigned char *)p,n);
    if(rc>0) return rc; if(slots[s].tcp->isRemoteClosed() && !slots[s].tcp->recvDataWaiting()) return 0;
    if(rc<0) return -1; blocked=1; return -1;
}
extern "C" int bs_dos_shutdown(int s,int how) { (void)how; return valid(s) && slots[s].tcp?slots[s].tcp->shutdown(TCP_SHUT_WR):-1; }
extern "C" int bsp_close(int s) {
    if(!valid(s)) return -1;
    if(slots[s].tcp) { slots[s].tcp->closeNonblocking(); slots[s].closing=1; }
    else { if(s==udp_slot) { Udp::unregisterCallback(slots[s].port); udp_slot=-1; } memset(&slots[s],0,sizeof slots[s]); } return 0;
}
extern "C" int bs_dos_setsockopt(int s,int level,int option,const char *p,int n) { (void)level;(void)option;(void)p;(void)n;return valid(s)?0:-1; }
extern "C" int bs_dos_sendto(int s,const char *p,int n,int flags,const sockaddr *address,int length) {
    const sockaddr_in *a=(const sockaddr_in *)address; IpAddr_t ip; int rc; (void)flags;(void)length;
    if(!valid(s) || slots[s].type!=SOCK_DGRAM || n<0 || n>512) return -1;
    memcpy(ip,&a->sin_addr.s_addr,4); rc=Udp::sendUdp(ip,slots[s].port,ntohs(a->sin_port),n,(unsigned char *)p,0);
    if(rc==1) {
        int i; for(i=0;i<4;i++) if(!pending_udp[i].used) { PendingUdp *q=&pending_udp[i]; q->used=1; q->slot=s; q->length=n; q->port=ntohs(a->sin_port); memcpy(q->ip,ip,4); memcpy(q->bytes,p,n); q->since=TIMER_GET_CURRENT(); return n; }
        return -1;
    }
    return rc<0?-1:n;
}
extern "C" int bs_dos_recvfrom(int s,char *p,int n,int flags,sockaddr *a,int *length) {
    Datagram *d; (void)flags; blocked=0; bs_dos_pump(); if(!valid(s) || s!=udp_slot) return -1; if(!count) { blocked=1; return -1; }
    d=&datagrams[head]; if(n>d->length) n=d->length; memcpy(p,d->bytes,n);
    if(a && length && *length>=(int)sizeof d->from) { memcpy(a,&d->from,sizeof d->from); *length=sizeof d->from; }
    head=(head+1)%4; count--; return n;
}
extern "C" unsigned short bs_dos_htons(unsigned short n) { return (n<<8)|(n>>8); }
extern "C" unsigned long bs_dos_inet_addr(const char *text) {
    unsigned long result=0; int i; const char *p=text;
    for(i=0;i<4;i++) { unsigned value=0; int digits=0; while(*p>='0' && *p<='9') { if(++digits>3) return INADDR_NONE; value=value*10+*p++-'0'; } if(!digits || value>255) return INADDR_NONE; result|=(unsigned long)value<<(i*8); if(i<3) { if(*p++!='.') return INADDR_NONE; } else if(*p) return INADDR_NONE; } return result;
}
extern "C" char *bs_dos_inet_ntoa(in_addr a) { static char text[16]; unsigned char *p=(unsigned char *)&a.s_addr; sprintf(text,"%u.%u.%u.%u",p[0],p[1],p[2],p[3]);return text; }
extern "C" unsigned long bsp_millis(void) { unsigned long ticks; _disable(); ticks=TIMER_GET_CURRENT(); _enable(); return (ticks/1000UL)*54925UL+(ticks%1000UL)*54925UL/1000UL; }
extern "C" unsigned long bsp_seconds(void) { unsigned long ticks; _disable(); ticks=TIMER_GET_CURRENT(); _enable(); return ticks/18UL; }
extern "C" unsigned long bsp_wait(const BSSocket *,int,BSSocket,unsigned long,unsigned long) { bs_dos_pump(); return 0; }
extern "C" void bsp_watch_connection(BSSocket) {}
extern "C" void bsp_broadcast(BSSocket s,const void *p,int n,unsigned short port) { sockaddr_in a; memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=INADDR_NONE;bs_dos_sendto(s,(const char *)p,n,0,(sockaddr *)&a,sizeof a); }
extern "C" void bs_hostname(char *p,size_t n) { if(n) { strncpy(p,MyHostname,n-1);p[n-1]=0; } }

extern "C" void bs_dos_identity(char *out) {
    sprintf(out,"%02x%02x%02x%02x%02x%02x%08lx%08lx%04x",
        MyEthAddr[0],MyEthAddr[1],MyEthAddr[2],MyEthAddr[3],MyEthAddr[4],MyEthAddr[5],
        (unsigned long)time(NULL),(unsigned long)rand(),(unsigned)rand());
}
