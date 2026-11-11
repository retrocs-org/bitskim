"""Exercise the actual single progress summary and drawing bounds on the host."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class TransferPanel(unittest.TestCase):
 def test_filename_progress_reset_and_waiting_count(self):
  source=r'''
#include "bitskim.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef unsigned long ULONG;
typedef unsigned short UWORD;
typedef char *STRPTR;
struct RastPort {int unused;};
struct Window {struct RastPort *RPort;};
static struct RastPort port;
static struct Window fake_window={&port},*window=&fake_window;
static BSClient *client;
static BSStatus status;
static BSResult result;
static unsigned long serial;
static int queued;
static unsigned short main_pens[7]={0,1,2,3,4,5,6};
static int pen,bar_fills;static char last_text[200];
#define JAM1 1
static void copy(char *d,const char *s,size_t n){strncpy(d,s,n-1);d[n-1]=0;}
const BSStatus *bs_status(const BSClient *c){(void)c;return &status;}
BSResult bs_transfer_result(const BSClient *c){(void)c;return result;}
unsigned long bs_transfer_serial(const BSClient *c){(void)c;return serial;}
unsigned long bs_transfer_rate(const BSClient *c){(void)c;return 10;}
const char *bs_last_error(const BSClient *c){(void)c;return "";}
void bs_queue_progress(const BSClient *c,unsigned long *p,unsigned long *t){(void)c;*p=2;*t=3;}
int bs_batch_is_tree(const BSClient *c){(void)c;return 0;}
int bs_batch_count(const BSClient *c){(void)c;return 0;}
int bs_batch_index(const BSClient *c){(void)c;return 0;}
int bs_queue_count(const BSClient *c){(void)c;return queued;}
int bs_queue_paused(const BSClient *c){(void)c;return 0;}
int bs_queue_item(const BSClient *c,int i,BSQueueItem *out){(void)c;memset(out,0,sizeof *out);strcpy(out->path,"SYS:queued.txt");out->total=90;out->active=i==0 && status.active && !status.incoming;return 1;}
int bs_to_local(const char *s,char *d,size_t n){copy(d,s,n);return 1;}

static void transfer_details(unsigned long d,unsigned long t,unsigned long r,char *out){sprintf(out,"%lu / %lu | %lu/s",d,t,r);}
static void SetAPen(struct RastPort *p,int n){(void)p;pen=n;}
static void RectFill(struct RastPort *p,int x,int y,int right,int bottom){(void)p;if(pen==4)bar_fills++;assert(x>=0 && right<600 && x<=right);assert(y>=0 && bottom<400 && y<=bottom);}
static void Move(struct RastPort *p,int x,int y){(void)p;assert(x>=0 && x<600 && y>=0 && y<400);}
static void Text(struct RastPort *p,STRPTR s,unsigned long n){(void)p;assert(n<=strlen(s));memcpy(last_text,s,n);last_text[n]=0;}
static void SetDrMd(struct RastPort *p,int n){(void)p;(void)n;}
static void CurrentTime(ULONG *s,ULONG *m){*s=1;*m=0;}
int bs_progress_summary(const BSClient *c,char *out,size_t n){(void)c;snprintf(out,n,"%d files waiting",queued);return 1;}
#include "transfer_panel.h"
int main(void){char title[100],detail[100];unsigned long done,total;
 status.active=1;status.incoming=0;status.total=100;status.transferred=20;strcpy(status.filename,"first.txt");queued=3;
 transfer_summary(title,detail,&done,&total);assert(strstr(title,"file 2/3"));assert(strstr(title,"first.txt"));assert(done==20 && total==100);
 summary_draw(1);assert(strstr(last_text,"3 files waiting"));assert(bar_fills==1);
 /* New incoming file starts at zero, never showing the old file's bar. */
 status.incoming=1;status.transferred=0;strcpy(status.filename,"second.txt");queued=0;bar_fills=0;
 transfer_summary(title,detail,&done,&total);assert(strstr(title,"Receiving: second.txt"));assert(done==0);summary_draw(1);assert(!bar_fills);
 result=BS_RESULT_COMPLETE;serial=1;status.transferred=100;transfers_observe();assert(summary_last_transfer_valid);
 status.active=0;transfer_summary(title,detail,&done,&total);assert(done==100 && total==100);
 main_shift=17;summary_draw(1);(void)peer_slots;return 0;}

'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'panel.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),'-I'+str(ROOT/'clients/amiga'),str(p/'panel.c'),'-o',str(p/'panel')],check=True)
   subprocess.run([str(p/'panel')],check=True)
