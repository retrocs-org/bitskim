"""Run the actual WBStartup helper with host substitutes; never launch a guest."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class StartupHelper(unittest.TestCase):
 def test_literal_launch_duplicate_host_and_failure_cleanup(self):
  source=r'''
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
typedef long BPTR;
typedef char *STRPTR;
#define BS_STARTUP_TEST
#define SYS_Asynch 1
#define SYS_Input 2
#define SYS_Output 3
#define NP_StackSize 4
#define TAG_DONE 0
#define TRUE 1
#define ACCESS_READ 1
#define MODE_OLDFILE 1
#define MODE_NEWFILE 2
static int running,launches,fail_launch,closed;
static char launched[2100];
static void Forbid(void){}
static void Permit(void){}
static void *FindPort(STRPTR s){assert(!strcmp(s,"BITSKIM"));return running?(void *)1:NULL;}
static BPTR Lock(const char *s,int mode){(void)s;(void)mode;return 1;}
static void UnLock(BPTR p){(void)p;}
static BPTR Open(const char *s,int mode){assert(!strcmp(s,"NIL:"));return mode;}
static void Close(BPTR p){(void)p;closed++;}
static int SystemTags(const char *command,...){strcpy(launched,command);launches++;return fail_launch?-1:0;}
#define main startup_main
#include "'''+str(ROOT/'installers/amiga/start-bitskim.c')+r'''"
#undef main
int main(void){
 FILE *f=fopen("ENV:BitSkim/app.path","w");assert(f);fputs("Work:My \"Tools\"/Bit*Skim",f);fclose(f);
 assert(startup_main()==0);assert(launches==1 && closed==0);
 assert(!strcmp(launched,"\"Work:My *\"Tools*\"/Bit**Skim\""));
 running=1;assert(startup_main()==0);assert(launches==1);
 running=0;fail_launch=1;assert(startup_main()==20);assert(closed==2);
 remove("ENV:BitSkim/app.path");assert(startup_main()==20);assert(launches==2);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);(p/'ENV:BitSkim').mkdir();(p/'test.c').write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],cwd=p,check=True)
