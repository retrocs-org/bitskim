"""Actual Atari MiNTNet C boundary on host sockets; GEMDOS traps are mocked.
No claim of MiNT kernel, AES, XIF or physical driver runtime coverage.
"""
import ctypes as C,subprocess,tempfile,unittest
from pathlib import Path
from test_core import CoreTests as _Core, ROOT
class MintNetAdapter(unittest.TestCase):
 def setUp(self):
  _Core.setUp(self);self.lib.test_filesystem(0,0)
 tearDown=_Core.tearDown
 client=_Core.client
 status=_Core.status
 until=_Core.until
 start=_Core.start
 transfer=_Core.transfer
 @classmethod
 def setUpClass(cls):
  _Core.setUpClass.__func__(cls);cls.temp=tempfile.TemporaryDirectory();path=Path(cls.temp.name)/'mint.so'
  subprocess.run(['cc','-shared','-fPIC','-DBS_ATARI','-Icore','-Iclients/atari','-Itests/atari-mock','tests/core_test_hooks.c','clients/atari/platform.c','tests/atari-mock/os-mock.c','tests/atari-mock/mint-cookie.c','-o',str(path)],cwd=ROOT,check=True)
  cls.library_path=path
  old=cls.lib;cls.lib=C.CDLL(str(path));cls.lib.bs_platform_init()
  for name in dir(old):
   if name.startswith('bs_'):
    a=getattr(old,name);b=getattr(cls.lib,name);b.argtypes=a.argtypes;b.restype=a.restype
  cls.lib.bsp_commit.argtypes=[C.c_char_p,C.c_char_p]
  cls.lib.test_filesystem.argtypes=[C.c_int,C.c_long]
  cls.lib.bs_at_absolute.argtypes=[C.c_char_p,C.c_char_p,C.c_size_t]
 @classmethod
 def tearDownClass(cls):cls.lib.bs_platform_cleanup();cls.temp.cleanup()
 def test_payload_both_directions(self):
  self.transfer(bytes(range(256))*31);self.a,self.b=self.b,self.a;self.pa,self.pb=self.pb,self.pa
  # transfer helper uses a fixed destination; remove only the test-owned output.
  (self.root/'received.bin').unlink();self.transfer(b'back to Atari')
 def test_empty_file(self):self.transfer(b'')
 def test_absolute_queue_path_preserves_drive(self):
  out=C.create_string_buffer(128)
  self.assertEqual(self.lib.bs_at_absolute(b'FILE.TXT',out,128),1);self.assertEqual(out.value,b'C:\\BITSKIM\\FILE.TXT')
  self.assertEqual(self.lib.bs_at_absolute(b'D:\\TEST\\FILE.TXT',out,128),1);self.assertEqual(out.value,b'D:\\TEST\\FILE.TXT')

 def test_unknown_filesystem_does_not_fall_back_to_replace(self):
  old=self.root/'part';old.write_bytes(b'new');dest=self.root/'saved'
  self.lib.test_filesystem(1,0x50000)
  self.assertEqual(self.lib.bsp_commit(bytes(old),bytes(dest)),-1)
  self.assertEqual(self.lib.test_renames(),0);self.assertTrue(old.exists())
 def test_fat_fallback_preserves_existing_destination(self):
  old=self.root/'part';old.write_bytes(b'new');dest=self.root/'saved';dest.write_bytes(b'sentinel')
  self.lib.test_filesystem(1,0x10001)
  self.assertEqual(self.lib.bsp_commit(bytes(old),bytes(dest)),-1)
  self.assertEqual(dest.read_bytes(),b'sentinel');self.assertEqual(old.read_bytes(),b'new')
 def test_temporary_leaf_does_not_alias_destination(self):
  self.start(b'payload');dest=self.root/'BS000000.T00'
  self.assertEqual(self.lib.bs_accept(self.b,bytes(dest)),1)
  self.assertFalse(dest.exists())
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),b'payload')

 def test_sigpipe_policy_is_restored_after_cleanup(self):
  import sys,signal
  code="import ctypes,signal,os; signal.signal(signal.SIGPIPE,signal.SIG_DFL); l=ctypes.CDLL("+repr(str(self.library_path))+"); assert l.bs_platform_init(); os.kill(os.getpid(),signal.SIGPIPE); l.bs_platform_cleanup(); os.kill(os.getpid(),signal.SIGPIPE)"
  result=subprocess.run([sys.executable,'-c',code])
  self.assertEqual(result.returncode,-signal.SIGPIPE)

def load_tests(loader,tests,pattern):return loader.loadTestsFromTestCase(MintNetAdapter)
