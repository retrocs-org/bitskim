"""Exercise production collision code and bounded engine service."""
from pathlib import Path
import ctypes as C
import os
import subprocess
import tempfile
import unittest
import test_core as core_tests
ROOT=core_tests.ROOT

class CollisionBounds(unittest.TestCase):
 def test_collision_suffixes_and_output_capacity(self):
  code=(ROOT/'core/batch.h').read_text()
  names=code[code.index('int bs_suggest_name('):code.index('int bs_batch_save_name(')]
  destination=code[code.index('int bs_suggest_destination('):code.index('#include "tree.h"')]
  harness=r'''
#include "bitskim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
static int collisions,calls;
static void copystr(char *d,const char *s,size_t n){if(n){strncpy(d,s,n-1);d[n-1]=0;}}
int bs_safe_name(const char *s){return s && *s;}
int bs_to_local(const char *s,char *out,size_t n){copystr(out,s,n);return 1;}
int bsp_path_kind(const char *p){(void)p;return calls++<collisions?1:0;}
'''+names+destination+r'''
int main(void){char out[BS_PATH_MAX+1];int i,j;size_t cap;
const char *cases[]={"a.abcdefgh","a.abcdefg","a.abcdef","abcdefghij","file.txt"};
int counts[]={1,9,10,99,100,999,1000,9999};
for(i=0;i<5;i++)for(j=0;j<8;j++){
 collisions=counts[j];calls=0;assert(bs_suggest_destination("/tmp",cases[i],0,out,sizeof out));
#ifdef __riscos__
 assert(strlen(strrchr(out,'/')+1)<=10);
#elif defined(__amigaos__)
 assert(strlen(strrchr(out,'/')+1)<=30);
#endif
}
/* A small caller buffer must remain untouched on every insufficient capacity. */
for(cap=0;cap<16;cap++){
 struct {char before[8],bytes[16],after[8];} guarded;
 memset(&guarded,'X',sizeof guarded);collisions=10;calls=0;
 if(bs_suggest_destination("/tmp","a",0,guarded.bytes,cap))assert(strlen(guarded.bytes)<cap);
 else for(i=0;i<16;i++)assert(guarded.bytes[i]=='X');
 for(i=0;i<8;i++){assert(guarded.before[i]=='X');assert(guarded.after[i]=='X');}
}
assert(!bs_suggest_destination("/tmp",NULL,0,out,sizeof out));
assert(!bs_suggest_destination("/tmp","a",0,NULL,sizeof out));
return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'bounds.c';source.write_text(harness)
   for flags in ([],['-D__riscos__'],['-D__amigaos__'],['-DBS_DOS']):
    target=Path(d)/'bounds'
    subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-Wno-deprecated-declarations','-g','-fsanitize=address,undefined',*flags,'-I'+str(ROOT/'core'),str(source),'-o',str(target)],check=True)
    subprocess.run([str(target)],check=True,capture_output=True)

class ServiceBudget(unittest.TestCase):
 setUp=core_tests.CoreTests.setUp
 tearDown=core_tests.CoreTests.tearDown
 client=core_tests.CoreTests.client
 status=core_tests.CoreTests.status
 until=core_tests.CoreTests.until
 start=core_tests.CoreTests.start
 @classmethod
 def setUpClass(cls):
  core_tests.CoreTests.setUpClass.__func__(cls)
  cls.lib.bs_service.argtypes=[C.c_void_p,C.c_ulong]
 def test_services_multiple_bursts_and_preserves_payload(self):
  data=os.urandom(2*1024*1024);self.start(data)
  destination=self.root/'received';self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(destination)),1)
  self.until(lambda:self.status(self.a).transferred>0)
  largest=0
  for _ in range(40):
   self.lib.bs_service(self.b,8)
   before=self.status(self.a).transferred
   self.lib.bs_service(self.a,8)
   largest=max(largest,self.status(self.a).transferred-before)
   if not self.status(self.a).active:break
  self.assertGreater(largest,32*4096)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(destination.read_bytes(),data)
 def test_zero_budget_is_one_tick_and_blocked_writer_returns(self):
  self.start(os.urandom(1024*1024))
  self.lib.bs_accept(self.b,os.fsencode(self.root/'received'));self.lib.bs_tick(self.b)
  self.lib.bs_service(self.a,0)
  self.assertLessEqual(self.status(self.a).transferred,32*4096)
  self.lib.bs_test_block_send(1)
  self.lib.bs_service(self.a,8)
  self.lib.bs_test_block_send(0)
  self.lib.bs_cancel(self.a)
