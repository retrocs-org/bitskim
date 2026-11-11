"""Compile the actual click-dispatch functions with host Wimp/core doubles."""
from pathlib import Path
import ctypes as C,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class RiscosQueueControls(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();folder=Path(cls.temp.name)
  source=(ROOT/'clients/riscos/main.c').read_text()
  start=source.index('static int clicked(View *v,int icon) {');end=source.index('static void click_preferences(',start)
  harness=r'''
#include "bitskim.h"
#include <string.h>
typedef struct{int id;char text[512];}Icon;
typedef struct{int count;Icon icons[40];}View;
static View main_view;
static int tools_button=-1;
static void show_tools(void){}
static BSClient *engine;
static int offset,selected,peers_dirty,path_icon,running,action;
static char selected_id[33];
static int ip_icon;
int bs_peer_count(const BSClient *c){(void)c;return 0;}
const BSPeer *bs_peer(const BSClient *c,int i){(void)c;(void)i;return 0;}
void bs_refresh(BSClient *c){(void)c;action=2;}
void bs_cancel(BSClient *c){(void)c;action=3;}
void bs_queue_continue(BSClient *c){(void)c;action=1;}
void bs_queue_pause(BSClient *c){(void)c;action=2;}
void bs_queue_skip(BSClient *c){(void)c;action=3;}
void bs_queue_clear(BSClient *c){(void)c;action=4;}
int bs_retry_failed(BSClient *c){(void)c;action=5;return 1;}
const char *bs_last_error(const BSClient *c){(void)c;return "failed";}

static void copy(char *out,const char *in,int n){(void)n;strcpy(out,in);}
static void icon_set(View *v,int id,const char *s){(void)v;(void)id;(void)s;}
static void send_file(const char *s){(void)s;action=1;}
static void show_preferences(void){}
static void hide_main(void){}

'''

  harness+=source[start:source.index('static void tools_update(',start)]
  harness+=source[source.index('static void click_main(',start):end]
  harness+='\nint dispatch(int n){int i;main_view.count=30;for(i=0;i<30;i++)main_view.icons[i].id=1000+i*7;action=0;click_main(main_view.icons[n].id);return action;}\n'
  (folder/'host.c').write_text(harness);library=folder/'host.so'
  subprocess.run(['cc','-shared','-fPIC','-I'+str(ROOT/'core'),str(folder/'host.c'),'-o',str(library)],check=True)
  cls.lib=C.CDLL(str(library));cls.lib.dispatch.argtypes=[C.c_int]
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def test_current_actions_resolve_wimp_handles(self):
  self.assertEqual(self.lib.dispatch(12),1)
  self.assertEqual(self.lib.dispatch(13),2)
  self.assertEqual(self.lib.dispatch(15),3)
