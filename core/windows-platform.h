/* Included only by platform.c for Win32/Winsock. UTF-8 paths at core boundary. */
#include <iphlpapi.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>
#include "windows-names.h"
/* Exclusive, automatically deleted spool in the user's temporary directory. */
FILE *bsp_temp_file(void) {
    static unsigned long sequence;int attempt,fd;FILE *file;
    wchar_t folder[MAX_PATH],path[MAX_PATH];DWORD length=GetTempPathW(MAX_PATH,folder);
    if(!length || length>=MAX_PATH-40)return NULL;
    for(attempt=0;attempt<100;attempt++) {
        swprintf(path,MAX_PATH,L"%lsBS%08lx%08lx.tmp",folder,(unsigned long)GetCurrentProcessId(),++sequence);
        fd=_wopen(path,_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY|_O_TEMPORARY|_O_NOINHERIT,_S_IREAD|_S_IWRITE);
        if(fd>=0){file=_fdopen(fd,"w+b");if(!file)_close(fd);return file;}
        if(errno!=EEXIST)return NULL;
    }return NULL;
}
static int windows_wide(const char *text,wchar_t *out,int capacity) {
    return MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,out,capacity)!=0;
}
static int windows_path(const char *path,wchar_t *out) {
    const wchar_t *name,*p;
    if(!windows_wide(path,out,BS_PATH_MAX+1)) return 0;
    name=out; for(p=out;*p;p++) if(*p==L'\\' || *p==L'/') name=p+1;
    return windows_name_supported(name);
}
int bs_platform_init(void) { WSADATA data; return WSAStartup(MAKEWORD(2,2),&data)==0; }
static unsigned long raw_seconds(void) { return (unsigned long)(GetTickCount64()/1000); }
unsigned long bsp_wait(const BSSocket *reads,int count,BSSocket writer,unsigned long sigmask,unsigned long timeout_ms) {
    fd_set r,w,e; struct timeval tv; int i;
    (void)sigmask; FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e);
    for(i=0;i<count;i++) if(reads[i]!=INVALID_SOCKET) FD_SET(reads[i],&r);
    /* A failed connect is reported in the exception set on Winsock. */
    if(writer!=INVALID_SOCKET) { FD_SET(writer,&w); FD_SET(writer,&e); }
    tv.tv_sec=(long)(timeout_ms/1000); tv.tv_usec=(long)(timeout_ms%1000)*1000;
    if(select(0,&r,&w,&e,&tv)==SOCKET_ERROR && !count && writer==INVALID_SOCKET) Sleep((DWORD)timeout_ms);
    return 0;
}
void bs_platform_cleanup(void) { WSACleanup(); }
int bsp_close(BSSocket s) { return closesocket(s); }
int bsp_nonblock(BSSocket s) { u_long enabled=1; return ioctlsocket(s,FIONBIO,&enabled); }
int bsp_wouldblock(void) {
    int e=WSAGetLastError(); return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS || e==WSAEALREADY || e==WSAEINTR;
}
int bsp_send(BSSocket s,const void *p,int n) { return send(s,(const char *)p,n,0); }
int bsp_recv(BSSocket s,void *p,int n) { return recv(s,(char *)p,n,0); }
int bsp_connected(BSSocket s) {
    fd_set write_set,error_set; struct timeval wait={0,0}; int rc,error=0,length=sizeof error;
    FD_ZERO(&write_set); FD_ZERO(&error_set); FD_SET(s,&write_set); FD_SET(s,&error_set);
    rc=select(0,NULL,&write_set,&error_set,&wait); if(rc<=0) return rc;
    if(getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&error,&length) || error) return -1;
    return 1;
}
void bs_hostname(char *out,size_t n) {
    wchar_t name[256]; DWORD length=256;
    if(!n) return;
    if(!GetComputerNameW(name,&length) || !WideCharToMultiByte(CP_UTF8,0,name,-1,out,(int)n,NULL,NULL)) {
        strncpy(out,"BitSkim",n); out[n-1]=0;
    }
}
FILE *bsp_open_file(const char *path,const char *mode) {
    wchar_t wide[BS_PATH_MAX+1],wide_mode[16];
    if(!windows_path(path,wide) || !windows_wide(mode,wide_mode,16)) return NULL;
    return _wfopen(wide,wide_mode);
}
int bsp_remove_file(const char *path) {
    wchar_t wide[BS_PATH_MAX+1]; return windows_path(path,wide) && DeleteFileW(wide)?0:-1;
}
FILE *bsp_create_file(const char *path) {
    wchar_t wide[BS_PATH_MAX+1]; HANDLE handle; int fd; FILE *file;
    if(!windows_path(path,wide)) return NULL;
    handle=CreateFileW(wide,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(handle==INVALID_HANDLE_VALUE) return NULL;
    fd=_open_osfhandle((intptr_t)handle,_O_WRONLY|_O_BINARY);
    if(fd<0) { CloseHandle(handle); DeleteFileW(wide); return NULL; }
    file=_fdopen(fd,"wb"); if(!file) { _close(fd); DeleteFileW(wide); } return file;
}
int bsp_commit(const char *temp,const char *destination) {
    wchar_t a[BS_PATH_MAX+1],b[BS_PATH_MAX+1];
    if(!windows_path(temp,a) || !windows_path(destination,b)) return -1;
    return MoveFileExW(a,b,MOVEFILE_WRITE_THROUGH)?0:-1;
}
int bsp_size(const char *path,unsigned long *out) {
    wchar_t wide[BS_PATH_MAX+1]; struct _stat64 st;
    if(!windows_path(path,wide) || _wstat64(wide,&st) || (st.st_mode&_S_IFMT)!=_S_IFREG || st.st_size<0 || st.st_size>BS_FILE_MAX) return 0;
    *out=(unsigned long)st.st_size; return 1;
}
static void bsp_windows_broadcast(BSSocket s,const void *data,int length,unsigned short port) {
    ULONG size=16384,result; IP_ADAPTER_ADDRESSES *all,*adapter;
    all=(IP_ADAPTER_ADDRESSES *)malloc(size); if(!all) return;
    result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,all,&size);
    if(result==ERROR_BUFFER_OVERFLOW) {
        free(all); all=(IP_ADAPTER_ADDRESSES *)malloc(size); if(!all) return;
        result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,all,&size);
    }
    if(result==NO_ERROR) for(adapter=all;adapter;adapter=adapter->Next) {
        IP_ADAPTER_UNICAST_ADDRESS *u;
        if(adapter->OperStatus!=IfOperStatusUp || adapter->IfType==IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for(u=adapter->FirstUnicastAddress;u;u=u->Next) if(u->Address.lpSockaddr && u->Address.lpSockaddr->sa_family==AF_INET && u->OnLinkPrefixLength>0 && u->OnLinkPrefixLength<31) {
            struct sockaddr_in destination; unsigned long ip=ntohl(((struct sockaddr_in *)u->Address.lpSockaddr)->sin_addr.s_addr);
            memset(&destination,0,sizeof destination); destination.sin_family=AF_INET; destination.sin_port=htons(port);
            destination.sin_addr.s_addr=htonl(ip|((1UL<<(32-u->OnLinkPrefixLength))-1));
            sendto(s,(const char *)data,length,0,(struct sockaddr *)&destination,sizeof destination);
        }
    }
    free(all);
}
