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
