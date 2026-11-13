from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ReceiveName(unittest.TestCase):
 def test_suggestions(self):
  mapper=(ROOT/'core/batch.h').read_text();mapper=mapper[mapper.index('int bs_suggest_name('):mapper.index('int bs_batch_save_name(')]
  source=r'''
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
static void copystr(char *d,const char *s,size_t n){strncpy(d,s,n-1);d[n-1]=0;}
static int bs_safe_name(const char *s){return s && *s;}
static int bs_to_local(const char *s,char *o,unsigned n){if(strchr(s,'@'))return 0;strncpy(o,s,n);return 1;}
''' +mapper+ r'''
#include "receive_name.h"
int main(void){char o[256];
assert(!receive_name("short.zip",o));assert(!strcmp(o,"short.zip"));
assert(receive_name("BitSkim-0.2-macOS-arm64.dmg.zip",o));assert(strlen(o)==30);
assert(receive_name("BitSkim-0.2-Windows10-x64-Setup.exe.zip",o));assert(strlen(o)==30);assert(!strcmp(o+26,".zip"));
assert(receive_name("abcdefghijklmnopqrstuvwxyz123456789",o));assert(strlen(o)==30);
assert(receive_name("@unicode",o));assert(!strcmp(o,"@unicode"));return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(source)
   subprocess.run(['cc','-D__amigaos__','-Wall','-Werror','-I'+str(ROOT/'clients/amiga'),str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
