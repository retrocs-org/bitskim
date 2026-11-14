#include "bitskim.h"
#include "platform.h"
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#if defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__)
#include <kernel.h>
static unsigned long bs_riscos_millis(void){_kernel_swi_regs r;memset(&r,0,sizeof r);if(_kernel_swi(0x42,&r,&r))return 0;return (unsigned long)r.r[0]*10UL;}
#endif
#ifndef _WIN32
#include <netinet/tcp.h>
#endif
#ifdef BS_OPENSSL
#define bsp_send bsp_raw_send
#define bsp_recv bsp_raw_recv
#define bsp_close bsp_raw_close
#define bsp_wouldblock bsp_raw_wouldblock
#endif
#ifdef _WIN32
#ifdef BS_WINDOWS_LEGACY
#include "windows-legacy-platform.h"
#else
#include "windows-platform.h"
#endif
#elif defined(__amigaos__)
#include <proto/dos.h>
#include <dos/dos.h>
#include <sys/stat.h>
#include <devices/timer.h>
#include <proto/timer.h>
struct Library *SocketBase = NULL;
struct Device *TimerBase = NULL;
static struct timerequest timer_request;
/* E-clock counter: monotonic, unaffected by the system clock being set. */
static unsigned long raw_seconds(void) {
    struct EClockVal clock; unsigned long frequency,whole,remainder;
    if(!TimerBase) return (unsigned long)time(NULL);
    frequency=ReadEClock(&clock); if(!frequency) return (unsigned long)time(NULL);
    whole=0xffffffffUL/frequency; remainder=0xffffffffUL%frequency+1;
    if(remainder==frequency) { whole++; remainder=0; }
    return clock.ev_hi*whole+(clock.ev_hi*remainder+clock.ev_lo%frequency)/frequency+clock.ev_lo/frequency;
}
int bs_platform_init(void) {
    if(!TimerBase && !OpenDevice((STRPTR)TIMERNAME,UNIT_ECLOCK,(struct IORequest *)&timer_request,0)) TimerBase=timer_request.tr_node.io_Device;
    SocketBase=OpenLibrary("bsdsocket.library",4); return SocketBase!=NULL;
}
void bs_platform_cleanup(void) {
    if(SocketBase) CloseLibrary(SocketBase);
    SocketBase=NULL;
    if(TimerBase) CloseDevice((struct IORequest *)&timer_request);
    TimerBase=NULL;
}
unsigned long bsp_wait(const BSSocket *reads,int count,BSSocket writer,unsigned long sigmask,unsigned long timeout_ms) {
    fd_set r,w; struct timeval tv; int i; long top=0; ULONG signals=sigmask;
    FD_ZERO(&r); FD_ZERO(&w);
    for(i=0;i<count;i++) if(reads[i]>=0) { FD_SET(reads[i],&r); if(reads[i]>=top) top=reads[i]+1; }
    if(writer>=0) { FD_SET(writer,&w); if(writer>=top) top=writer+1; }
    tv.tv_sec=timeout_ms/1000; tv.tv_usec=(timeout_ms%1000)*1000;
    if(WaitSelect(top,&r,&w,NULL,&tv,sigmask?&signals:NULL)<0) return SetSignal(0,sigmask)&sigmask;
    return sigmask?signals&sigmask:0;
}
int bsp_close(BSSocket s) { return CloseSocket(s); }
int bsp_nonblock(BSSocket s) { long yes=1; return IoctlSocket(s,FIONBIO,(char *)&yes); }
int bsp_wouldblock(void) { long e=Errno(); return e==EWOULDBLOCK || e==EINPROGRESS || e==EALREADY || e==EINTR; }
int bsp_send(BSSocket s,const void *p,int n) { return send(s,(char *)p,n,0); }
int bsp_recv(BSSocket s,void *p,int n) { return recv(s,(char *)p,n,0); }
int bsp_connected(BSSocket s) {
    fd_set f; struct timeval tv; int r,e=0; long len=sizeof(e);
    FD_ZERO(&f); FD_SET(s,&f); tv.tv_sec=0; tv.tv_usec=0;
    r=WaitSelect(s+1,NULL,&f,NULL,&tv,NULL);
    if(r<=0) return r;
    if(getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&e,&len)<0 || e) return -1;
    return 1;
}
FILE *bsp_create_file(const char *path) {
    BPTR l=Lock((STRPTR)path,ACCESS_READ);
    if(l) { UnLock(l); return NULL; }
    return fopen(path,"wb");
}
int bsp_commit(const char *temp,const char *dest) {
    BPTR l=Lock((STRPTR)dest,ACCESS_READ);
    if(l) { UnLock(l); return -1; }
    return Rename((STRPTR)temp,(STRPTR)dest) ? 0 : -1;
}
#else
#include <fcntl.h>
#include <sys/stat.h>
#ifndef __riscos__
#include <ifaddrs.h>
#include <net/if.h>
#endif
#if defined(__linux__)
#include <sys/syscall.h>
#endif
static unsigned long raw_seconds(void) {
#if defined(CLOCK_MONOTONIC) && !defined(__riscos__)
    struct timespec now;
    if(!clock_gettime(CLOCK_MONOTONIC,&now)) return (unsigned long)now.tv_sec;
#endif
    return (unsigned long)time(NULL);
}
unsigned long bsp_wait(const BSSocket *reads,int count,BSSocket writer,unsigned long sigmask,unsigned long timeout_ms) {
    fd_set r,w; struct timeval tv; int i,top=0;
    (void)sigmask; FD_ZERO(&r); FD_ZERO(&w);
    for(i=0;i<count;i++) if(reads[i]>=0) { FD_SET(reads[i],&r); if(reads[i]>=top) top=reads[i]+1; }
    if(writer>=0) { FD_SET(writer,&w); if(writer>=top) top=writer+1; }
    tv.tv_sec=(long)(timeout_ms/1000); tv.tv_usec=(long)(timeout_ms%1000)*1000;
    select(top,&r,&w,NULL,&tv); return 0;
}
int bs_platform_init(void) { return 1; }
void bs_platform_cleanup(void) {}
int bsp_close(BSSocket s) { return close(s); }
int bsp_nonblock(BSSocket s) { int f=fcntl(s,F_GETFL,0); return f<0 ? -1 : fcntl(s,F_SETFL,f|O_NONBLOCK); }
int bsp_wouldblock(void) { return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINPROGRESS || errno==EALREADY || errno==EINTR; }
int bsp_send(BSSocket s,const void *p,int n) {
#ifdef MSG_NOSIGNAL
    return (int)send(s,p,(size_t)n,MSG_NOSIGNAL);
#else
    return (int)send(s,p,(size_t)n,0);
#endif
}
int bsp_recv(BSSocket s,void *p,int n) { return (int)recv(s,p,(size_t)n,0); }
int bsp_connected(BSSocket s) {
    fd_set f; struct timeval tv; int r,e=0; socklen_t len=sizeof(e);
    FD_ZERO(&f); FD_SET(s,&f); tv.tv_sec=0; tv.tv_usec=0;
    r=select(s+1,NULL,&f,NULL,&tv); if(r<=0) return r;
    if(getsockopt(s,SOL_SOCKET,SO_ERROR,&e,&len)<0 || e) return -1;
    return 1;
}
FILE *bsp_create_file(const char *path) {
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600); FILE *f;
    if(fd<0) return NULL;
    f=fdopen(fd,"wb"); if(!f) { close(fd); unlink(path); } return f;
}
int bsp_commit(const char *temp,const char *dest) {
#ifdef __riscos__
    extern int bsp_riscos_commit(const char *,const char *);
    return bsp_riscos_commit(temp,dest);
#else
#if defined(__APPLE__) && defined(RENAME_EXCL)
    /* Atomic no-replace rename; works where hard links do not (exFAT, SMB). */
    if(!renamex_np(temp,dest,RENAME_EXCL)) return 0;
    if(errno==EEXIST) return -1;
#elif defined(__linux__) && defined(SYS_renameat2)
    if(!syscall(SYS_renameat2,AT_FDCWD,temp,AT_FDCWD,dest,1 /* RENAME_NOREPLACE */)) return 0;
    if(errno==EEXIST) return -1;
#endif
    /* Same-directory hard link is atomic and refuses any existing destination. */
    if(!link(temp,dest)) { unlink(temp); return 0; }
    if(errno==EEXIST) return -1;
    /* Never fall back to replacing a pathname we cannot reserve atomically.
       Another writer can replace an O_EXCL placeholder before rename/unlink. */
    return -1;
#endif
}
#endif
#ifndef _WIN32
FILE *bsp_open_file(const char *path,const char *mode) { return fopen(path,mode); }
int bsp_remove_file(const char *path) { return remove(path); }
void bs_hostname(char *out,size_t n) { if(!n) return; if(gethostname(out,(int)n)<0 || !out[0]) { strncpy(out,"BitSkim",n); } out[n-1]=0; }
#endif
/* Best-effort transport failure detection; older stacks retain engine timeouts. */
void bsp_watch_connection(BSSocket s) {
    int enabled=1;
    setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(const char *)&enabled,sizeof(enabled));
#ifdef TCP_NODELAY
    /* Whole frames are written at once; Nagle would hold short control frames
       (OFFER, END, COMPLETE) behind a delayed ACK for 40-200 ms. */
    setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char *)&enabled,sizeof(enabled));
#endif
#ifdef __APPLE__
    {
        int idle=10,interval=3,count=3,drop=30;
        setsockopt(s,IPPROTO_TCP,TCP_KEEPALIVE,&idle,sizeof(idle));
        setsockopt(s,IPPROTO_TCP,TCP_KEEPINTVL,&interval,sizeof(interval));
        setsockopt(s,IPPROTO_TCP,TCP_KEEPCNT,&count,sizeof(count));
        setsockopt(s,IPPROTO_TCP,TCP_RXT_CONNDROPTIME,&drop,sizeof(drop));
    }
#endif
}
/* Monotonic where the platform offers it. Any remaining backward step (a wall
   clock being set) is absorbed so elapsed-time arithmetic never wraps. */
unsigned long bsp_seconds(void) {
    static unsigned long last,offset; static int started;
    unsigned long now=raw_seconds()+offset;
    if(started && now<last) { offset+=last-now; now=last; }
    started=1; last=now; return now;
}
/* Wrapping milliseconds; rate arithmetic uses modular unsigned deltas. */
unsigned long bsp_millis(void) {
#ifdef _WIN32
    return (unsigned long)GetTickCount();
#elif defined(__amigaos__)
    static unsigned long previous,value,remainder;static int initialized;
    struct EClockVal clock;unsigned long frequency,delta,part;
    if(!TimerBase)return (bsp_seconds()*1000UL)&0xffffffffUL;
    frequency=ReadEClock(&clock);if(!frequency)return (bsp_seconds()*1000UL)&0xffffffffUL;
    if(!initialized){previous=clock.ev_lo;initialized=1;return value;}
    delta=clock.ev_lo-previous;previous=clock.ev_lo;
    value+=(delta/frequency)*1000UL;
    if(frequency<=4290000UL){part=(delta%frequency)*1000UL+remainder;value+=part/frequency;remainder=part%frequency;}
    else value+=(delta%frequency)/(frequency/1000UL);
    return value;
#elif defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__)
    return bs_riscos_millis();
#else
#if defined(CLOCK_MONOTONIC)
    struct timespec now;
    if(!clock_gettime(CLOCK_MONOTONIC,&now))return ((unsigned long)now.tv_sec*1000UL+(unsigned long)now.tv_nsec/1000000UL)&0xffffffffUL;
#endif
    return (bsp_seconds()*1000UL)&0xffffffffUL;
#endif
}
#ifndef _WIN32
int bsp_size(const char *path,unsigned long *out) {
    struct stat st;
    if(stat(path,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0 || (unsigned long)st.st_size>BS_FILE_MAX) return 0;
    *out=(unsigned long)st.st_size; return 1;
}
#endif
#ifndef BS_NO_BROADCAST
static void broadcast_to(BSSocket s,const void *p,int n,unsigned short port,unsigned long ip) {
    struct sockaddr_in a; memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(port); a.sin_addr.s_addr=ip;
    sendto(s,(const char *)p,n,0,(struct sockaddr *)&a,sizeof(a));
}
#endif
void bsp_broadcast(BSSocket s,const void *p,int n,unsigned short port) {
#ifdef BS_NO_BROADCAST
    (void)s; (void)p; (void)n; (void)port;
#else
#if !defined(__amigaos__) && !defined(_WIN32) && !defined(__riscos__)
    struct ifaddrs *all=NULL,*i;
    if(getifaddrs(&all)==0) {
        for(i=all;i;i=i->ifa_next) if(i->ifa_addr && i->ifa_broadaddr && i->ifa_addr->sa_family==AF_INET && (i->ifa_flags&IFF_UP) && (i->ifa_flags&IFF_BROADCAST))
            broadcast_to(s,p,n,port,((struct sockaddr_in *)i->ifa_broadaddr)->sin_addr.s_addr);
        freeifaddrs(all);
    }
#endif
#ifdef _WIN32
    bsp_windows_broadcast(s,p,n,port);
#endif
    broadcast_to(s,p,n,port,INADDR_BROADCAST);
#endif
}
/* The classic client assumes Latin-1 Workbench filenames; unsupported characters
   fail explicitly. Modern clients keep UTF-8 unchanged. */
int bs_to_utf8(const char *source,char *out,size_t cap) {
    size_t used=0; const unsigned char *p=(const unsigned char *)source;
    if(!source || !cap) return 0;
    while(*p) {
#if defined(__amigaos__) || defined(__riscos__)
        if(*p>=128) { if(used+2>=cap) return 0; out[used++]=(char)(0xc0|(*p>>6)); out[used++]=(char)(0x80|(*p&63)); }
        else
#endif
        { if(used+1>=cap) return 0; out[used++]=(char)*p; }
        p++;
    }
    out[used]=0; return 1;
}
int bs_to_local(const char *source,char *out,size_t cap) {
    size_t used=0; const unsigned char *p=(const unsigned char *)source;
    if(!source || !cap) return 0;
    while(*p) {
        if(used+1>=cap) return 0;
#if defined(__amigaos__) || defined(__riscos__)
        if(*p>=128) {
            if((*p!=0xc2 && *p!=0xc3) || p[1]<0x80 || p[1]>0xbf) return 0;
            out[used++]=(char)(((*p&3)<<6)|(p[1]&63)); p+=2;
        } else
#endif
        out[used++]=(char)*p++;
    }
    out[used]=0; return 1;
}

#ifdef BS_OPENSSL
#undef bsp_send
#undef bsp_recv
#undef bsp_close
#undef bsp_wouldblock
#include "tls.h"
#endif

#ifdef BS_OPENSSL
FILE *bsp_resume_file(const char *path,unsigned long expected) {
#ifdef _WIN32
    wchar_t wide[BS_PATH_MAX+1];HANDLE h;BY_HANDLE_FILE_INFORMATION info;LARGE_INTEGER size;int fd;FILE *f;
    if(!windows_path(path,wide))return NULL;
    h=CreateFileW(wide,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(h==INVALID_HANDLE_VALUE)return NULL;
    if(!GetFileInformationByHandle(h,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)) || !GetFileSizeEx(h,&size) || size.QuadPart!=(LONGLONG)expected){CloseHandle(h);return NULL;}
    fd=_open_osfhandle((intptr_t)h,_O_RDWR|_O_BINARY);if(fd<0){CloseHandle(h);return NULL;}f=_fdopen(fd,"r+b");if(!f)_close(fd);return f;
#else
    int fd;struct stat st;FILE *f;
    fd=open(path,O_RDWR|O_NOFOLLOW);if(fd<0)return NULL;
    if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (unsigned long)st.st_size!=expected){close(fd);return NULL;}
    f=fdopen(fd,"r+b");if(!f)close(fd);return f;
#endif
}
#endif

#include "fs.h"
