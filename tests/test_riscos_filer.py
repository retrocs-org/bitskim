"""Run the production Filer message handler with a host Wimp boundary."""
import subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class FilerDrops(unittest.TestCase):
 def test_targeted_drops_only_acknowledge_accepted_durable_paths(self):
  source=(ROOT/'clients/riscos/main.c').read_text();handler=source[source.index('static void message(int *b)'):source.index('int main(void)')]
  harness=r'''
#include "bitskim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#define PTR(p) ((long)(p))
#define Wimp_SendMessage 7
typedef struct {long r[10];} _kernel_swi_regs;
typedef struct {int id;} Icon;
typedef struct {int handle;Icon icons[8];} View;
static View main_view={1,{{0},{10},{11},{12},{13},{14}}},pref_view={2,{{20}}},offer_view={3,{{30}}};
static int iconbar=5,running=1,peer_icons[5]={1,2,3,4,5},offset,ip_icon,peers_dirty,pref_folder,offer_open=1,offer_path;
static int queued,accept_send=1,shown,ack,paused;static char selected_id[33],field[256];static BSClient *engine;static BSPeer peer;
static int call(int swi,_kernel_swi_regs *r){assert(swi==Wimp_SendMessage && r->r[0]==17 && r->r[2]==99);ack++;return 1;}
static void show_main(void){shown++;}
static void copy(char *out,const char *in,size_t n){strncpy(out,in,n-1);out[n-1]=0;}
static void icon_set(View *v,int i,const char *s){(void)v;(void)i;copy(field,s,sizeof field);}
static void report(const char *s){(void)s;}

static int native_to_unix(const char *s,char *out,size_t n){copy(out,s,n);return 1;}
static int send_file(const char *s){assert(!strcmp(s,"test.txt"));queued++;return accept_send;}
const BSPeer *bs_peer(const BSClient *c,int i){(void)c;return i==1?&peer:NULL;}
int bs_queue_paused(const BSClient *c){(void)c;return paused;}
void bs_queue_pause(BSClient *c){(void)c;paused=1;}
int bs_batch_count(const BSClient *c){(void)c;return 3;}
int bs_batch_is_tree(const BSClient *c){(void)c;return 1;}
'''+handler+r'''
static void drop(int *b,int w,int i,const char *name){memset(b,0,256);b[0]=256;b[1]=99;b[2]=42;b[4]=3;b[5]=w;b[6]=i;strcpy((char *)b+44,name);}
int main(void){int b[64];strcpy(peer.id,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
 drop(b,1,11,"test.txt");message(b);assert(ack==1 && b[3]==42 && b[4]==4 && queued==1 && !strcmp(selected_id,peer.id));
 accept_send=0;drop(b,1,11,"test.txt");message(b);assert(ack==1 && queued==2 && b[4]==3);
 accept_send=1;drop(b,-2,5,"test.txt");message(b);assert(shown==1 && ack==2);
 drop(b,1,11,"test.txt");b[3]=12;message(b);assert(ack==2 && queued==3);
 drop(b,2,20,".");message(b);assert(ack==3 && !strcmp(field,"."));
 drop(b,3,30,".");message(b);assert(ack==4);
 drop(b,3,30,"missing-directory");message(b);assert(ack==4);
 drop(b,1,11,"test.txt");b[0]=260;message(b);assert(ack==4 && queued==3);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);(p/'test.c').write_text(harness)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],cwd=p,check=True)
