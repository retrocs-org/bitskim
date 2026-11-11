"""Compile production batch save-name adaptation for the classic targets."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class BatchNames(unittest.TestCase):
 def test_classic_filename_adaptation(self):
  code=(ROOT/'core/batch.h').read_text();function=code[code.index('int bs_suggest_name('):code.index('int bs_suggest_destination(')]
  prefix=r'''
#include "bitskim.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
typedef struct {int tree;} TestBatch;struct BSClient{TestBatch *in_batch;};
static BSBatchItem item;
static void copystr(char *d,const char *s,size_t n){strncpy(d,s,n-1);d[n-1]=0;}
const BSBatchItem *bs_batch_item(const BSClient *c,int i){(void)c;return i==0?&item:0;}
int bs_safe_name(const char *s){return s && *s;}
int bs_to_local(const char *s,char *out,size_t n){if(strchr(s,'@'))return 0;copystr(out,s,n);return 1;}
'''
  main=r'''
int main(void){char out[256];strcpy(item.filename,"photos with spaces and a long filename.txt");assert(bs_batch_save_name(0,0,out,sizeof out));
#ifdef __amigaos__
assert(strlen(out)==30 && !strcmp(out+26,".txt"));
#elif defined(__riscos__)
assert(strlen(out)==10);
#else
assert(!strcmp(out,"PHOTOS_W.TXT"));strcpy(item.filename,"CON.txt");assert(bs_batch_save_name(0,0,out,sizeof out));assert(!strcmp(out,"F0000001.TXT"));
#endif
strcpy(item.filename,"@unsupported");assert(bs_batch_save_name(0,0,out,sizeof out));assert(out[0]);
assert(!bs_batch_save_name(0,1,out,sizeof out));assert(!bs_batch_save_name(0,0,out,2));return 0;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'names.c').write_text(prefix+function+main)
   for flag in ('__amigaos__','__riscos__','BS_DOS'):
    subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-D'+flag,'-I'+str(ROOT/'core'),str(p/'names.c'),'-o',str(p/'names')],check=True)
    subprocess.run([str(p/'names')],check=True)
