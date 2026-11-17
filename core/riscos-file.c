/* Native FileSwitch commit. UnixLib rename() can unlink an existing target;
   OS_FSControl 25 instead refuses an existing object, including races. */
#include "bitskim.h"
#include <kernel.h>
#include <swis.h>
#include <unixlib/local.h>
#include <string.h>
int bsp_riscos_commit(const char *temp,const char *destination) {
    char from[BS_PATH_MAX+1],to[BS_PATH_MAX+1]; _kernel_swi_regs r;
    if(!__riscosify_std(temp,0,from,sizeof(from),NULL) || !__riscosify_std(destination,0,to,sizeof(to),NULL)) return -1;
    memset(&r,0,sizeof(r)); r.r[0]=25; r.r[1]=(int)from; r.r[2]=(int)to;
    return _kernel_swi(OS_FSControl,&r,&r)?-1:0;
}
/* OS_File 17 reads catalogue data without path expansion; 18 writes type only. */
int bsp_riscos_metadata_get(const char *path,BSMetadata *out){char native[BS_PATH_MAX+1];_kernel_swi_regs r;if(!__riscosify_std(path,0,native,sizeof native,NULL))return 0;memset(&r,0,sizeof r);r.r[0]=17;r.r[1]=(int)native;if(_kernel_swi(OS_File,&r,&r) || r.r[0]!=1 || ((unsigned long)r.r[2]&0xfff00000UL)!=0xfff00000UL)return 0;memset(out,0,sizeof *out);out->kind=2;out->filetype=((unsigned long)r.r[2]>>8)&4095UL;return 1;}
int bsp_riscos_metadata_apply(const char *path,const BSMetadata *in){char native[BS_PATH_MAX+1];_kernel_swi_regs r;if(in->kind!=2 || in->filetype>4095UL || !__riscosify_std(path,0,native,sizeof native,NULL))return 0;memset(&r,0,sizeof r);r.r[0]=18;r.r[1]=(int)native;r.r[2]=(int)in->filetype;return !_kernel_swi(OS_File,&r,&r);}
