"""Exercise the real platform adapter when atomic finalization primitives fail."""
import ctypes as C,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class SafeCommit(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();p=Path(cls.tmp.name)
  (p/'check.c').write_text(r'''
#define renamex_np unavailable_rename
#define syscall unavailable_syscall
#define link unavailable_link
#include "'''+str(ROOT/'core/platform.c')+r'''"
int unavailable_rename(const char *a,const char *b,unsigned int flags) {(void)a;(void)b;(void)flags;errno=ENOTSUP;return -1;}
#ifdef __APPLE__
int unavailable_syscall(int n,...) {(void)n;errno=ENOTSUP;return -1;}
#else
long unavailable_syscall(long n,...) {(void)n;errno=ENOTSUP;return -1;}
#endif
int unavailable_link(const char *a,const char *b) {(void)a;(void)b;errno=ENOTSUP;return -1;}
''')
  subprocess.run(['cc','-shared','-fPIC','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-std=c89',str(p/'check.c'),'-o',str(p/'check.so')],check=True)
  cls.lib=C.CDLL(str(p/'check.so'));cls.lib.bsp_commit.argtypes=[C.c_char_p,C.c_char_p]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_unsupported_filesystem_does_not_create_or_replace_destination(self):
  with tempfile.TemporaryDirectory() as d:
   src=Path(d)/'temp';dst=Path(d)/'received';src.write_bytes(b'payload')
   self.assertNotEqual(self.lib.bsp_commit(bytes(src),bytes(dst)),0)
   self.assertFalse(dst.exists());self.assertEqual(src.read_bytes(),b'payload')
   dst.write_bytes(b'another writer owns this')
   self.assertNotEqual(self.lib.bsp_commit(bytes(src),bytes(dst)),0)
   self.assertEqual(dst.read_bytes(),b'another writer owns this');self.assertTrue(src.exists())
