#include "bitskim.h"
#include "platform.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <dos.h>
/* Deliberately ASCII/8.3: never silently truncate or map a remote filename. */
static int dos_path(const char *s) {
    int base=0,extension=0,dot=0; const char *p=s;
    if(!s || !*s || strlen(s)>127) return 0;
    if(isalpha((unsigned char)p[0]) && p[1]==':') p+=2;
    if(*p=='\\' || *p=='/') p++;
    while(*p) {
        unsigned char c=*p++;
        if(c=='/' || c=='\\') { if(!base) return 0; base=extension=dot=0; continue; }
        if(c=='.') { if(!base || dot) return 0; dot=1; continue; }
        if(c<33 || c>126 || strchr("\"*+,/:;<=>?[\\]|",c)) return 0;
        if(dot) { if(++extension>3) return 0; } else if(++base>8) return 0;
    }
    if(!base || (dot && !extension)) return 0;
    /* DOS device names are special even with extensions. */
    p=s; { const char *q; char stem[9]; int i=0; for(q=s;*q;q++) if(*q=='/' || *q=='\\' || *q==':') p=q+1;
      while(*p && *p!='.' && i<8) stem[i++]=(char)toupper((unsigned char)*p++); stem[i]=0;
      if(!strcmp(stem,"CON") || !strcmp(stem,"PRN") || !strcmp(stem,"AUX") || !strcmp(stem,"NUL")) return 0;
      if(strlen(stem)==4 && (!strncmp(stem,"COM",3) || !strncmp(stem,"LPT",3)) && stem[3]>='1' && stem[3]<='9') return 0;
    } return 1;
}
FILE *bsp_open_file(const char *path,const char *mode) { return dos_path(path)?fopen(path,mode):NULL; }
int bsp_remove_file(const char *path) { return dos_path(path)?remove(path):-1; }
FILE *bsp_create_file(const char *path) {
    int fd; FILE *f; if(!dos_path(path)) return NULL;
    fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IREAD|S_IWRITE); if(fd<0) return NULL;
    f=fdopen(fd,"wb"); if(!f) { close(fd); remove(path); } return f;
}
int bsp_commit(const char *temp,const char *dest) {
    union REGS in,out; struct SREGS seg;
    if(!dos_path(temp) || !dos_path(dest)) return -1;
    /* DOS rename (INT 21h/56h) fails if destination exists; no unlink fallback. */
    memset(&in,0,sizeof in); segread(&seg); in.h.ah=0x56;
    seg.ds=FP_SEG(temp); in.x.dx=FP_OFF(temp); seg.es=FP_SEG(dest); in.x.di=FP_OFF(dest);
    intdosx(&in,&out,&seg); return out.x.cflag?-1:0;
}
int bsp_size(const char *path,unsigned long *size) { struct stat st; if(!dos_path(path) || stat(path,&st) || (st.st_mode&S_IFMT)!=S_IFREG || st.st_size<0 || (unsigned long)st.st_size>BS_FILE_MAX) return 0; *size=(unsigned long)st.st_size; return 1; }
static int ascii(const char *s,char *out,size_t cap) { size_t n=0; if(!s || !cap) return 0; while(*s) { unsigned char c=*s++; if(c>126 || n+1>=cap) return 0; out[n++]=c; } out[n]=0;return 1; }
int bs_to_utf8(const char *s,char *out,size_t cap) { return ascii(s,out,cap); }
int bs_to_local(const char *s,char *out,size_t cap) { return ascii(s,out,cap); }

#include "fs.h"
