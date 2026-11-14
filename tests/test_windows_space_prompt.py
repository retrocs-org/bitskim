"""Exercise the production Windows free-space prompt policy with native UI stubs."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class SpacePrompt(unittest.TestCase):
 def test_sufficient_space_skips_confirmation_but_unknown_and_low_remain_explicit(self):
  source=(ROOT/'clients/shared/tools-win.h').read_text();function=source[source.index('static int bs_tools_space('):]
  code=r'''
#include <assert.h>
#include <string.h>
#include "bitskim.h"
typedef int HWND;
typedef char TCHAR;
#define TEXT(x) x
#define MB_OK 0
#define MB_ICONEXCLAMATION 16
#define MB_YESNO 4
#define MB_ICONQUESTION 32
#define IDYES 6
#define IDNO 7
static int known=1,enough=1,calls,answer=IDYES,policy,exists;
int bs_conflict_get(const BSClient *c){(void)c;return policy;}
static int bsp_path_kind(const char *path){(void)path;return exists;}
int bs_destination_space_report(const BSClient *c,const char *p,int collection,char *out,size_t cap){(void)c;(void)p;(void)collection;(void)cap;strcpy(out,"Disk space report");return enough?1:-1;}
int bs_destination_space_info(const BSClient *c,const char *p,int collection,BSStorage *out){(void)c;(void)p;(void)collection;memset(out,0,sizeof *out);out->known=known;out->enough=enough;return 1;}
static void bs_tools_convert(const char *s,TCHAR *out,int cap){(void)cap;strcpy(out,s);}
static int MessageBox(HWND owner,const char *text,const char *title,int flags){(void)owner;(void)text;(void)title;(void)flags;calls++;return answer;}
'''+function+r'''
int main(void){
 assert(bs_tools_space(1,0,"file",0,1)==1 && calls==0);
 assert(bs_tools_space(1,0,"folder",1,1)==1 && calls==0);
 known=0;assert(bs_tools_space(1,0,"file",0,1)==1 && calls==1);
 answer=IDNO;assert(bs_tools_space(1,0,"file",0,1)==0 && calls==2);
 assert(bs_tools_space(1,0,"file",0,0)==1 && calls==2);
 known=1;enough=0;assert(bs_tools_space(1,0,"file",0,1)==0 && calls==3);
 policy=BS_CONFLICT_SKIP;exists=1;assert(bs_tools_space(1,0,"file",0,1)==1 && calls==3);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   file=Path(folder)/'policy.c';binary=Path(folder)/'policy';file.write_text(code)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),str(file),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
