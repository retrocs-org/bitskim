"""Run the production edit-update routine against scroll/selection Win32 stubs."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ToolsRefresh(unittest.TestCase):
 def test_live_text_updates_preserve_viewport_and_selection(self):
  text=(ROOT/'clients/shared/tools-win.h').read_text()
  function=text[text.index('static void bs_tools_update_report('):text.index('static const WinTheme *bs_tools_palette;')]
  source=r'''
#include <assert.h>
#include <string.h>
#include <stdint.h>
typedef char TCHAR;typedef unsigned long DWORD;typedef uintptr_t WPARAM;typedef intptr_t LPARAM;
#define _tcscmp strcmp
#define FALSE 0
#define TRUE 1
#define NULL ((void *)0)
#define EM_GETFIRSTVISIBLELINE 1
#define EM_GETSEL 2
#define WM_SETREDRAW 3
#define EM_SETSEL 4
#define EM_LINESCROLL 5
static int bs_tools_report=1,first=31,redraw=1,updates,painted,maxline=100;
static DWORD selected_start=7,selected_end=18;static char content[4096]="initial";
static void GetWindowText(int w,char *out,int cap){(void)w;(void)cap;strcpy(out,content);}
static void SetWindowText(int w,const char *text){(void)w;assert(!redraw);strcpy(content,text);first=0;selected_start=selected_end=0;updates++;}
static long SendMessage(int w,int message,WPARAM a,LPARAM b){(void)w;
 if(message==EM_GETFIRSTVISIBLELINE)return first;
 if(message==EM_GETSEL){*(DWORD *)a=selected_start;*(DWORD *)b=selected_end;}
 if(message==WM_SETREDRAW)redraw=(int)a;
 if(message==EM_SETSEL){selected_start=(DWORD)a;selected_end=(DWORD)b;}
 if(message==EM_LINESCROLL){first+=(int)b;if(first>maxline)first=maxline;if(first<0)first=0;}
 return 0;
}
static void InvalidateRect(int w,void *r,int erase){(void)w;(void)r;assert(!erase && redraw);painted++;}
'''.replace('#define NULL ((void *)0)\n','')+function+r'''
int main(void){
 bs_tools_update_report("updated status");assert(first==31 && selected_start==7 && selected_end==18 && redraw && updates==1 && painted==1);
 bs_tools_update_report("updated status");assert(first==31 && updates==1 && painted==1);
 first=67;selected_start=3;selected_end=5;
 bs_tools_update_report("another update");assert(first==67 && selected_start==3 && selected_end==5 && updates==2);
 maxline=8;bs_tools_update_report("shorter");assert(first==8 && redraw);
 return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   file=Path(folder)/'refresh.c';binary=Path(folder)/'refresh';file.write_text(source)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror',str(file),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
