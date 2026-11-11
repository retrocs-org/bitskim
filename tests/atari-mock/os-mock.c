#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <unistd.h>
#include <string.h>
long Supexec(long (*fn)(void)){struct timespec t;(void)fn;clock_gettime(CLOCK_MONOTONIC,&t);return (long)(t.tv_sec*200UL+t.tv_nsec/5000000UL);}
static int rename_calls;
long Frename(int reserved,const char *a,const char *b){(void)reserved;rename_calls++;if(link(a,b))return -1;unlink(a);return 0;}
long Dgetdrv(void){return 2;}
long Dgetpath(char *out,int drive){(void)drive;strcpy(out,"\\BITSKIM");return 0;}
#include "mint/dcntl.h"
static int force_link_failure;static long fs_type;
long Flink(const char *a,const char *b){return force_link_failure?-1:link(a,b);}
long Fdelete(const char *p){return unlink(p);}
long Dcntl(int code,const char *path,long output){struct fs_info *info=(struct fs_info *)output;(void)code;(void)path;if(!fs_type)return -1;info->type=fs_type;return 0;}
void test_filesystem(int fail,long type){force_link_failure=fail;fs_type=type;rename_calls=0;}
int test_renames(void){return rename_calls;}

#include <sys/socket.h>
int test_socketpair(int fd[2]){return socketpair(AF_UNIX,SOCK_STREAM,0,fd);}
#include <fcntl.h>
long Fcreate(const char *name,short flags){(void)flags;return open(name,O_CREAT|O_TRUNC|O_WRONLY,0600);}
long Fwrite(short fd,long count,const void *data){return write(fd,data,(size_t)count);}
long Fclose(short fd){return close(fd);}
