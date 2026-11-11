/* ANSI Win32 + Winsock 1.1, including stock Windows 95. No Winsock 2 dependency. */
#include <io.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
/* Exclusive, automatically deleted spool in the user's temporary directory. */
FILE *bsp_temp_file(void) {
    static unsigned long sequence;int attempt,fd;FILE *file;
    char folder[MAX_PATH],path[MAX_PATH];DWORD length=GetTempPathA(MAX_PATH,folder);
    if(!length || length>=MAX_PATH-40)return NULL;
    for(attempt=0;attempt<100;attempt++) {
        sprintf(path,"%.219sBS%08lx%08lx.tmp",folder,(unsigned long)GetCurrentProcessId(),++sequence);
        fd=_open(path,_O_CREAT|_O_EXCL|_O_RDWR|_O_BINARY|_O_TEMPORARY|_O_NOINHERIT,_S_IREAD|_S_IWRITE);
        if(fd>=0){file=_fdopen(fd,"w+b");if(!file)_close(fd);return file;}
        if(errno!=EEXIST)return NULL;
    }return NULL;
}
static int legacy_convert(const char *s,char *out,size_t cap,UINT from,UINT to) {
    wchar_t wide[BS_PATH_MAX+1],check[BS_PATH_MAX+1]; BOOL substituted=FALSE; int n;
    if(!s || !cap || cap>2147483647UL) return 0;
    if(!MultiByteToWideChar(from,0,s,-1,wide,BS_PATH_MAX+1)) return 0;
    n=WideCharToMultiByte(to,0,wide,-1,out,(int)cap,NULL,to==CP_ACP?&substituted:NULL);
    if(!n || substituted) return 0;
    if(to==CP_ACP) {
        int i;
        if(!MultiByteToWideChar(CP_ACP,0,out,-1,check,BS_PATH_MAX+1)) return 0;
        for(i=0;wide[i] || check[i];i++) if(wide[i]!=check[i]) return 0;
    }
    return 1;
}
static int legacy_path(const char *utf8,char *out) {
    const char *name,*p; char stem[16]; int i;
    if(!legacy_convert(utf8,out,BS_PATH_MAX+1,CP_UTF8,CP_ACP) || strlen(out)>=MAX_PATH) return 0;
    name=out; for(p=out;*p;p++) if(*p=='/' || *p=='\\') name=p+1;
    if(!*name || name[strlen(name)-1]=='.' || name[strlen(name)-1]==' ') return 0;
    for(p=name;*p;p++) if((unsigned char)*p<32 || strchr("<>:\"|?*",*p)) return 0;
    for(i=0;name[i] && name[i]!='.' && i<15;i++) stem[i]=name[i]; stem[i]=0;
    if(!lstrcmpiA(stem,"CON") || !lstrcmpiA(stem,"PRN") || !lstrcmpiA(stem,"AUX") || !lstrcmpiA(stem,"NUL")) return 0;
    if(strlen(stem)==4 && (!strnicmp(stem,"COM",3) || !strnicmp(stem,"LPT",3)) && stem[3]>='1' && stem[3]<='9') return 0;
    return 1;
}
int bs_platform_init(void) { WSADATA d; return WSAStartup(MAKEWORD(1,1),&d)==0; }
void bs_platform_cleanup(void) { WSACleanup(); }
static unsigned long raw_seconds(void) {
    static DWORD previous; static unsigned long seconds,remainder; DWORD now=GetTickCount(),delta=now-previous;
    previous=now; seconds+=delta/1000; remainder+=delta%1000;
    seconds+=remainder/1000; remainder%=1000; return seconds;
}
unsigned long bsp_wait(const BSSocket *reads,int count,BSSocket writer,unsigned long signals,unsigned long ms) {
    fd_set r,w,e; struct timeval tv; int i; (void)signals;
    FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e);
    for(i=0;i<count;i++) if(reads[i]!=INVALID_SOCKET) FD_SET(reads[i],&r);
    if(writer!=INVALID_SOCKET) { FD_SET(writer,&w); FD_SET(writer,&e); }
    tv.tv_sec=ms/1000; tv.tv_usec=ms%1000*1000;
    if(!count && writer==INVALID_SOCKET) Sleep(ms); else select(0,&r,&w,&e,&tv);
    return 0;
}
int bsp_close(BSSocket s) { return closesocket(s); }
int bsp_nonblock(BSSocket s) { u_long one=1; return ioctlsocket(s,FIONBIO,&one); }
int bsp_wouldblock(void) { int e=WSAGetLastError(); return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS || e==WSAEALREADY || e==WSAEINTR; }
int bsp_send(BSSocket s,const void *p,int n) { return send(s,p,n,0); }
int bsp_recv(BSSocket s,void *p,int n) { return recv(s,p,n,0); }
int bsp_connected(BSSocket s) {
    fd_set w,e; struct timeval tv={0,0}; int rc,error=0,n=sizeof error;
    FD_ZERO(&w); FD_ZERO(&e); FD_SET(s,&w); FD_SET(s,&e);
    rc=select(0,NULL,&w,&e,&tv); if(rc<=0) return rc;
    return getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&error,&n) || error ? -1:1;
}
void bs_hostname(char *out,size_t cap) { char name[256]; DWORD n=sizeof name; if(!GetComputerNameA(name,&n) || !legacy_convert(name,out,cap,CP_ACP,CP_UTF8)) { strncpy(out,"BitSkim",cap); out[cap-1]=0; } }
FILE *bsp_open_file(const char *path,const char *mode) { char local[BS_PATH_MAX+1]; return legacy_path(path,local)?fopen(local,mode):NULL; }
int bsp_remove_file(const char *path) { char local[BS_PATH_MAX+1]; return legacy_path(path,local) && DeleteFileA(local)?0:-1; }
FILE *bsp_create_file(const char *path) {
    char local[BS_PATH_MAX+1]; HANDLE h; int fd; FILE *f;
    if(!legacy_path(path,local)) return NULL;
    h=CreateFileA(local,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL); if(h==INVALID_HANDLE_VALUE) return NULL;
    fd=_open_osfhandle((intptr_t)h,_O_WRONLY|_O_BINARY); if(fd<0) { CloseHandle(h); DeleteFileA(local); return NULL; }
    f=_fdopen(fd,"wb"); if(!f) { _close(fd); DeleteFileA(local); } return f;
}
int bsp_commit(const char *temp,const char *dest) { char a[BS_PATH_MAX+1],b[BS_PATH_MAX+1]; return legacy_path(temp,a) && legacy_path(dest,b) && MoveFileA(a,b)?0:-1; }
int bsp_size(const char *path,unsigned long *out) { char local[BS_PATH_MAX+1]; struct _stat st; if(!legacy_path(path,local) || _stat(local,&st) || (st.st_mode&_S_IFMT)!=_S_IFREG || st.st_size<0 || (unsigned long)st.st_size>BS_FILE_MAX) return 0; *out=st.st_size; return 1; }
/* Limited broadcast is emitted by platform.c; no IP Helper DLL on Windows 95. */
static void bsp_windows_broadcast(BSSocket s,const void *p,int n,unsigned short port) { (void)s;(void)p;(void)n;(void)port; }
