/* Package-only updater helper. Request normal Ctrl-C shutdown and wait for the
   owning tasks to exit before Installer replaces executables. No forced kills. */
#ifndef BS_STOP_TEST
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/execbase.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#endif
#include <stdio.h>
#include <string.h>
long __stack=32768;
const char bitskim_stop_version[]="$VER: StopBitSkim 0.1 (02.11.2026)";
static int task_present(struct Task *task) {
    struct Node *node;
    if(SysBase->ThisTask==task)return 1;
    for(node=SysBase->TaskReady.lh_Head;node->ln_Succ;node=node->ln_Succ)if((struct Task *)node==task)return 1;
    for(node=SysBase->TaskWait.lh_Head;node->ln_Succ;node=node->ln_Succ)if((struct Task *)node==task)return 1;
    return 0;
}
static int stop_clients(void) {
    struct Task *tasks[100];struct MsgPort *port;char name[32];int count=0,i,j,tick,busy;
    Forbid();
    for(i=0;i<100;i++) {
        if(i)sprintf(name,"BITSKIM.%d",i);else strcpy(name,"BITSKIM");
        port=FindPort((STRPTR)name);
        if(port && port->mp_SigTask) {
            for(j=0;j<count;j++)if(tasks[j]==port->mp_SigTask)break;
            if(j==count)tasks[count++]=port->mp_SigTask;
        }
    }
    for(i=0;i<count;i++)Signal(tasks[i],SIGBREAKF_CTRL_C);
    Permit();
    /* 10 seconds at the DOS-standard 50 ticks/second. Waiting is bounded even
       if an old client is stuck in a requester or the network stack. */
    for(tick=0;tick<=100;tick++) {
        busy=0;Forbid();
        for(i=0;i<count;i++)if(task_present(tasks[i]))busy=1;
        for(i=0;i<100;i++) {
            if(i)sprintf(name,"BITSKIM.%d",i);else strcpy(name,"BITSKIM");
            if(FindPort((STRPTR)name))busy=1;
        }
        Permit();
        if(!busy)return 0;
        if(tick<100)Delay(5);
    }
    return 20;
}
#ifndef BS_STOP_TEST
int main(void) {return stop_clients();}
#endif
