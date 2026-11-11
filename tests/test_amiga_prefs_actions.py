"""Exercise actual Preferences Use/Save actions and their write destinations."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class PreferencesActions(unittest.TestCase):
 def test_use_writes_live_only_save_writes_both_and_failure_keeps_editor_open(self):
  text=(ROOT/'clients/amiga/preferences.h').read_text()
  function=text[text.index('static int pref_save('):text.index('static void preferences(')]
  source=r'''
#include <stddef.h>
#include <string.h>
#include <assert.h>
typedef char *STRPTR;
struct MsgPort {void *mp_SigTask;};
typedef struct {char name[64];int value;} PrefDraft;
typedef struct {PrefDraft draft,initial;int invalid_limits;} PrefEditor;
static char *live_prefs="ENV:BitSkim/BitSkim.prefs",*archive_prefs="ENVARC:BitSkim/BitSkim.prefs";
static int live_writes,archive_writes,fail_archive,applied,signals;
#define SIGBREAKF_CTRL_E 16
static int bs_to_utf8(const char *in,char *out,size_t n){(void)n;strcpy(out,in);return 1;}
static void alert(const char *text){(void)text;}
static void pref_to_globals(const PrefDraft *p){applied=p->value;}
static int write_prefs(const char *path){if(!strcmp(path,live_prefs)){live_writes++;return 1;}assert(!strcmp(path,archive_prefs));archive_writes++;return !fail_archive;}
static void Forbid(void){}
static void Permit(void){}
static struct MsgPort *FindPort(STRPTR name){static struct MsgPort port;assert(!strcmp(name,"BITSKIM"));return &port;}
static void Signal(void *task,int mask){(void)task;assert(mask==16);signals++;}
'''+function+r'''
int main(void){
 PrefEditor e;memset(&e,0,sizeof e);strcpy(e.draft.name,"Amiga");e.draft.value=7;e.initial.value=3;
 assert(pref_save(&e,0)==1);assert(live_writes==1 && archive_writes==0 && applied==7 && signals==1);
 assert(pref_save(&e,1)==1);assert(live_writes==2 && archive_writes==1 && applied==7 && signals==2);
 fail_archive=1;assert(pref_save(&e,1)==0);assert(applied==3 && signals==2);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);(p/'test.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
