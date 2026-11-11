"""Production page capture: no access to controls owned by another page."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PreferencesLayout(unittest.TestCase):
 def test_general_and_network_keep_independent_draft_values(self):
  code=(ROOT/'clients/amiga/preferences.h').read_text()
  function=code[code.index('static void pref_capture('):code.index('static int pref_build(')]
  draft=code[code.index('typedef struct {'):code.index('} PrefDraft;')+len('} PrefDraft;')]
  limits=(ROOT/'core/rate.h').read_text();parser=limits[limits.index('int bs_limit_parse('):limits.index('int bs_limits_configure(')]
  source=r'''
#include <assert.h>
#include <string.h>
#include <stddef.h>
#define BS_RATE_KIB_MAX 1048575UL
#define GFLG_SELECTED 1
struct StringInfo {char *Buffer;};
struct Gadget {int Flags;void *SpecialInfo;};
'''+draft+r'''
typedef struct {int section;struct Gadget *fields[15];int invalid_limits;PrefDraft draft;} PrefEditor;
static void copy(char *d,const char *s,size_t n){strncpy(d,s,n-1);d[n-1]=0;}
static int ui_cycle_value(struct Gadget *g){assert(g);return g->Flags;}
'''+parser+function+r'''
int main(void){
 PrefEditor e;struct Gadget g[15];struct StringInfo name={"My Amiga"},folder={"Work:Inbox"},up={"128"},down={"256"};
 memset(&e,0,sizeof e);memset(g,0,sizeof g);
 e.draft.visible=1;e.draft.receiving=0;e.draft.theme=3;
 e.fields[1]=&g[1];g[1].SpecialInfo=&name;
 e.fields[6]=&g[6];g[6].SpecialInfo=&folder;
 e.fields[4]=&g[4];g[4].Flags=GFLG_SELECTED;e.fields[5]=&g[5];
 e.fields[10]=&g[10];e.fields[11]=&g[11];g[11].Flags=GFLG_SELECTED;e.fields[14]=&g[14];g[14].Flags=GFLG_SELECTED;
 pref_capture(&e);
 assert(!strcmp(e.draft.name,"My Amiga") && !strcmp(e.draft.folder,"Work:Inbox"));
 assert(e.draft.automatic && !e.draft.ask && !e.draft.start && e.draft.close_iconify && e.draft.sound);
 assert(e.draft.visible && !e.draft.receiving && e.draft.theme==3);
 memset(e.fields,0,sizeof e.fields);e.section=2;
 e.fields[12]=&g[12];g[12].SpecialInfo=&up;e.fields[13]=&g[13];g[13].SpecialInfo=&down;
 e.fields[2]=&g[2];e.fields[3]=&g[3];g[3].Flags=GFLG_SELECTED;
 pref_capture(&e);
 assert(!e.draft.visible && e.draft.receiving && !e.invalid_limits);assert(e.draft.send_limit==128UL*1024 && e.draft.receive_limit==256UL*1024);
 assert(e.draft.automatic && !e.draft.ask && !e.draft.start && e.draft.close_iconify && e.draft.sound);
 assert(!strcmp(e.draft.folder,"Work:Inbox"));
 memset(e.fields,0,sizeof e.fields);e.section=1;e.fields[9]=&g[9];g[9].Flags=5;
 pref_capture(&e);assert(e.draft.theme==5 && e.draft.receiving && e.draft.automatic);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);(p/'capture.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror',str(p/'capture.c'),'-o',str(p/'capture')],check=True)
   subprocess.run([str(p/'capture')],check=True)
