"""Run the production shutdown helper with deterministic Exec task/port doubles."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class StopHelper(unittest.TestCase):
 def test_normal_task_exit_dedup_and_timeout(self):
  source=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define BS_STOP_TEST
#define SIGBREAKF_CTRL_C 4096
#define STRPTR char *
struct Node {struct Node *ln_Succ;};
struct Task {struct Node node;};
struct List {struct Node *lh_Head;};
struct ExecBase {struct Task *ThisTask;struct List TaskReady,TaskWait;};
struct MsgPort {struct Task *mp_SigTask;};
static struct ExecBase execbase,*SysBase=&execbase;
static struct Task tasks[2],self;
static struct Node end;
static struct MsgPort ports[3];
static int port_count,signals,delays,stuck,forbidden;
static void Forbid(void){assert(!forbidden);forbidden=1;}
static void Permit(void){assert(forbidden);forbidden=0;}
static struct MsgPort *FindPort(STRPTR n){int i;char name[32];assert(forbidden);for(i=0;i<port_count;i++){if(i)sprintf(name,"BITSKIM.%d",i);else strcpy(name,"BITSKIM");if(!strcmp(name,n))return &ports[i];}return 0;}
static void Signal(struct Task *t,unsigned long s){assert(forbidden);assert(t==&tasks[0] || t==&tasks[1]);assert(s==SIGBREAKF_CTRL_C);signals++;}
static void Delay(int n){assert(!forbidden && n==5);delays++;if(!stuck){if(delays==1)port_count=0;if(delays==3)execbase.TaskReady.lh_Head=&end;}}
#include "stop-bitskim.c"
static void reset(void){memset(&execbase,0,sizeof execbase);execbase.ThisTask=&self;execbase.TaskReady.lh_Head=execbase.TaskWait.lh_Head=&end;port_count=signals=delays=stuck=0;}
static void start(void){port_count=3;ports[0].mp_SigTask=ports[1].mp_SigTask=&tasks[0];ports[2].mp_SigTask=&tasks[1];tasks[0].node.ln_Succ=&tasks[1].node;tasks[1].node.ln_Succ=&end;execbase.TaskReady.lh_Head=&tasks[0].node;}
int main(void){reset();assert(stop_clients()==0);assert(!signals && !delays);
 reset();start();assert(stop_clients()==0);assert(signals==2 && delays==3);
 reset();start();stuck=1;assert(stop_clients()==20);assert(signals==2 && delays==100);
 assert(!forbidden);return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'installers/amiga'),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
