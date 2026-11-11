"""Exercise production menu dispatch without a queue window or requester."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class QueueMenu(unittest.TestCase):
 def test_actions_and_idle_only_skip(self):
  controller=(ROOT/'clients/amiga/queue_menu.h').read_text().split('static void queue_menu_command',1)[1]
  source=r'''
#include <assert.h>
typedef unsigned short UWORD;
#define MENUNULL 65535
#define MENUNUM(code) ((code)>>8)
#define ITEMNUM(code) ((code)&255)
static int calls;
static void transfer_tools(void){calls++;}
'''
  source+='static void queue_menu_command'+controller
  source+='int main(void){queue_menu_command(0);assert(calls==1);queue_menu_command(1);queue_menu_command(256);queue_menu_command(MENUNULL);assert(calls==1);return 0;}'
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'menu.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),str(p/'menu.c'),'-o',str(p/'menu')],check=True)
   subprocess.run([str(p/'menu')],check=True)
