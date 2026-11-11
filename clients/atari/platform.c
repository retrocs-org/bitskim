#define _GNU_SOURCE 1
/* Atari-local filesystem/clock boundary, shared by STinG and MiNTNet builds. */
#include "bitskim.h"
#include "platform.h"
#include "atari.h"
#include <mint/osbind.h>
#include <mint/mintbind.h>
#ifndef BS_ATARI_STING
#include <mint/dcntl.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
int bs_at_ascii(const char *s) {const unsigned char *p=(const unsigned char *)s;if(!s)return 0;for(;*p;p++)if(*p<32 || *p>126)return 0;return 1;}
int bs_to_utf8(const char *s,char *out,size_t cap) {if(!bs_at_ascii(s) || strlen(s)>=cap)return 0;strcpy(out,s);return 1;}
int bs_to_local(const char *s,char *out,size_t cap) {return bs_to_utf8(s,out,cap);}
static int native_path(const char *s) {
    if(!s || !*s || strlen(s)>BS_PATH_MAX || !bs_at_ascii(s))return 0;
#ifdef BS_ATARI_STING
    {const char *p=s;int base=0,ext=0,dot=0;
     if(isalpha((unsigned char)p[0]) && p[1]==':')p+=2;
     if(*p=='\\' || *p=='/')p++;
     for(;*p;p++) {
        if(*p=='\\' || *p=='/') {if(!base)return 0;base=ext=dot=0;continue;}
        if(*p=='.') {if(!base || dot)return 0;dot=1;continue;}
        if(strchr(" :*?\"<>|",*p))return 0;
        if(dot){if(++ext>3)return 0;}else if(++base>8)return 0;
     }
     if(!base || (dot && !ext))return 0;
    }
#endif
    return 1;
}
int bs_at_valid_path(const char *p){return native_path(p);}
int bs_at_absolute(const char *input,char *out,size_t capacity){
    char current[BS_PATH_MAX+1],path[BS_PATH_MAX+1],prefix[4];const char *leaf;size_t i,n;int drive;
    if(!input || !*input || strlen(input)>BS_PATH_MAX || !bs_at_ascii(input))return 0;
    strcpy(path,input);for(i=0;path[i];i++)if(path[i]=='/')path[i]='\\';
    drive=(int)Dgetdrv();leaf=path;
    if(isalpha((unsigned char)path[0]) && path[1]==':'){drive=toupper((unsigned char)path[0])-'A';leaf=path+2;}
    if(drive<0 || drive>25)return 0;
    prefix[0]=(char)('A'+drive);prefix[1]=':';prefix[2]=0;
    if(*leaf=='\\')current[0]=0;
    else if(Dgetpath(current,drive+1)<0)return 0;
    n=strlen(current);if(strlen(leaf)+n+4>capacity)return 0;
    strcpy(out,prefix);strcat(out,current);if(*leaf!='\\' && (!n || current[n-1]!='\\'))strcat(out,"\\");strcat(out,leaf);
    return native_path(out);
}
FILE *bsp_open_file(const char *p,const char *mode){return native_path(p)?fopen(p,mode):NULL;}
int bsp_remove_file(const char *p){return native_path(p)?remove(p):-1;}
int bsp_size(const char *p,unsigned long *size){struct stat st;if(!native_path(p) || stat(p,&st) || !S_ISREG(st.st_mode) || st.st_size<0 || (unsigned long)st.st_size>BS_FILE_MAX)return 0;*size=(unsigned long)st.st_size;return 1;}
FILE *bsp_create_file(const char *p){int fd;FILE *f;if(!native_path(p))return NULL;fd=open(p,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,0600);if(fd<0)return NULL;f=fdopen(fd,"wb");if(!f){close(fd);remove(p);}return f;}
int bsp_commit(const char *temp,const char *dest){
    if(!native_path(temp) || !native_path(dest))return -1;
#ifndef BS_ATARI_STING
    /* Hard-link creation is no-replace. For filesystems without links, only
       use the documented no-replace FAT driver, not arbitrary XFS rename. */
    if(Flink(temp,dest)==0){Fdelete(temp);return 0;}
    {struct fs_info info;memset(&info,0,sizeof info);
     if(Dcntl(FS_INFO,temp,(long)&info)<0)return -1;
     if(info.type!=FS_FAT12 && info.type!=FS_FAT16 && info.type!=FS_FAT32 && info.type!=FS_VFAT12 && info.type!=FS_VFAT16 && info.type!=FS_VFAT32)return -1;
    }
#endif
    /* MiNTLib rename() may delete a destination; never call that wrapper. */
#ifdef BS_ATARI_STING
    {char from[BS_PATH_MAX+1],to[BS_PATH_MAX+1];size_t i;for(i=0;temp[i];i++)from[i]=temp[i]=='/'?'\\':temp[i];from[i]=0;for(i=0;dest[i];i++)to[i]=dest[i]=='/'?'\\':dest[i];to[i]=0;return Frename(0,from,to)==0?0:-1;}
#else
    return Frename(0,temp,dest)==0?0:-1;
#endif
}
static long clock_read(void){return *(volatile unsigned long *)0x4ba;}
unsigned long bsp_millis(void){return (unsigned long)Supexec(clock_read)*5UL;}
unsigned long bsp_seconds(void){static unsigned long last,seconds,remainder;unsigned long now=(unsigned long)Supexec(clock_read),delta=now-last;static int initialized;if(!initialized){last=now;initialized=1;return seconds;}last=now;seconds+=delta/200;remainder+=delta%200;seconds+=remainder/200;remainder%=200;return seconds;}
#ifndef BS_ATARI_STING
#include <errno.h>
#include <netinet/tcp.h>
#include <signal.h>
static void (*previous_sigpipe)(int);
static int signal_ready;
int bs_platform_init(void){int probe=socket(AF_INET,SOCK_STREAM,0);if(probe<0)return 0;close(probe);if(!signal_ready){previous_sigpipe=signal(SIGPIPE,SIG_IGN);signal_ready=1;}return 1;}
void bs_platform_cleanup(void){if(signal_ready){if(previous_sigpipe!=SIG_ERR)signal(SIGPIPE,previous_sigpipe);signal_ready=0;}}
void bs_hostname(char *out,size_t cap){int rc;if(!cap)return;rc=gethostname(out,(int)cap);out[cap-1]=0;if(rc || !bs_at_ascii(out))strncpy(out,"Atari-MiNT",cap);out[cap-1]=0;}
int bsp_close(BSSocket s){return close(s);}
int bsp_nonblock(BSSocket s){long yes=1;return ioctl(s,FIONBIO,&yes);}
void bsp_watch_connection(BSSocket s){int yes=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof yes);}
int bsp_wouldblock(void){return errno==EWOULDBLOCK || errno==EAGAIN || errno==EINPROGRESS || errno==EALREADY || errno==EINTR;}
int bsp_connected(BSSocket s){fd_set w;struct timeval t;int error=0;socklen_t length=sizeof error;FD_ZERO(&w);FD_SET(s,&w);t.tv_sec=t.tv_usec=0;if(select(s+1,NULL,&w,NULL,&t)<=0)return 0;if(getsockopt(s,SOL_SOCKET,SO_ERROR,&error,&length) || error)return -1;return 1;}
int bsp_send(BSSocket s,const void *p,int n){return send(s,p,n,0);}
int bsp_recv(BSSocket s,void *p,int n){return recv(s,p,n,0);}
unsigned long bsp_wait(const BSSocket *r,int count,BSSocket w,unsigned long signals,unsigned long timeout){(void)r;(void)count;(void)w;(void)signals;(void)timeout;return 0;}
void bsp_broadcast(BSSocket s,const void *p,int n,unsigned short port){struct sockaddr_in a;memset(&a,0,sizeof a);a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=INADDR_BROADCAST;sendto(s,p,n,0,(struct sockaddr *)&a,sizeof a);}
#endif

#include "fs.h"
