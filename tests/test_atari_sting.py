"""Run production STinG adapter with a host-only transport test double.
Actual m68k short-argument ABI is compiled separately against the external SDK.
"""
import ctypes as C,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Address(C.Structure):
 _fields_=[('family',C.c_ushort),('port',C.c_ushort),('ip',C.c_ulong)]
class StingAdapter(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();lib=Path(cls.temp.name)/'sting.so'
  subprocess.run(['cc','-shared','-fPIC','-DBS_ATARI','-DBS_ATARI_STING','-Icore','-Itests/atari-mock','clients/atari/sting.c','clients/atari/platform.c','tests/atari-mock/os-mock.c','tests/atari-mock/sting-mock.c','-o',str(lib)],cwd=ROOT,check=True)
  cls.lib=l=C.CDLL(str(lib))
  for n in ('bs_at_bind','bs_at_connect'):getattr(l,n).argtypes=[C.c_int,C.POINTER(Address),C.c_int]
  l.bs_at_accept.argtypes=[C.c_int,C.POINTER(Address),C.POINTER(C.c_int)]
  l.bsp_send.argtypes=[C.c_int,C.c_char_p,C.c_int];l.bsp_recv.argtypes=[C.c_int,C.c_char_p,C.c_int]
  l.bs_at_recvfrom.argtypes=[C.c_int,C.c_char_p,C.c_int,C.c_int,C.POINTER(Address),C.POINTER(C.c_int)]
  l.bs_at_sendto.argtypes=[C.c_int,C.c_char_p,C.c_int,C.c_int,C.POINTER(Address),C.c_int]
  l.bs_at_valid_path.argtypes=[C.c_char_p]
  l.test_source.argtypes=[C.c_int,C.c_ulong,C.c_ushort];l.test_data.argtypes=[C.c_int,C.c_char_p,C.c_int]
 def test_gem_startup_initializes_stack_before_hostname(self):
  executable=Path(self.temp.name)/'gem-test'
  subprocess.run(['cc','-DBS_ATARI','-DBS_ATARI_STING','-Icore','-Iclients/atari','-Itests/atari-mock','core/bitskim.c','clients/atari/main.c','clients/atari/platform.c','clients/atari/sting.c','tests/atari-mock/os-mock.c','tests/atari-mock/gem-mock.c','tests/atari-mock/sting-mock.c','-o',str(executable)],cwd=ROOT,check=True)
  subprocess.run([str(executable)],cwd=self.temp.name,check=True,timeout=5)
  self.assertTrue(any(p.name.endswith('BITSKIM.CFG') for p in Path(self.temp.name).iterdir()))
 def test_preferences_and_compact_mode_state(self):
  executable=Path(self.temp.name)/'ui-test'
  subprocess.run(['cc','-DBS_ATARI','-DBS_ATARI_STING','-Icore','-Iclients/atari','-Itests/atari-mock','core/bitskim.c','tests/atari-ui-check.c','clients/atari/platform.c','clients/atari/sting.c','tests/atari-mock/os-mock.c','tests/atari-mock/gem-mock.c','tests/atari-mock/sting-mock.c','-o',str(executable)],cwd=ROOT,check=True)
  subprocess.run([str(executable)],cwd=self.temp.name,check=True,timeout=5)
 def setUp(self):self.lib.bs_platform_init();self.lib.test_send_error(0)
 def tearDown(self):self.lib.bs_platform_cleanup()
 @classmethod
 def tearDownClass(cls):cls.temp.cleanup()
 def listener(self,kind):
  l=self.lib;s=l.bs_at_socket(2,kind,0);a=Address(2,46440,0)
  self.assertEqual(l.bs_at_bind(s,C.byref(a),C.sizeof(a)),0)
  if kind==1:self.assertEqual(l.bs_at_listen(s,4),0)
  return s
 def test_accept_rearms_and_preserves_recipient(self):
  l=self.lib;s=self.listener(1);old=l.test_tcp();l.test_source(old,0x0a000001,1234);l.test_state(old,4);out=Address();size=C.c_int(C.sizeof(out))
  accepted=l.bs_at_accept(s,C.byref(out),C.byref(size));self.assertGreaterEqual(accepted,0)
  self.assertEqual((out.ip,out.port),(0x0a000001,1234));self.assertNotEqual(l.test_tcp(),old)
  l.test_data(old,b'abc',3);buf=C.create_string_buffer(10);self.assertEqual(l.bsp_recv(accepted,buf,10),3);self.assertEqual(buf.value,b'abc')
 def test_udp_second_peer_is_not_first_address(self):
  l=self.lib;s=self.listener(2);buf=C.create_string_buffer(512)
  for ip in (0x0a000001,0x0a000002):
   h=l.test_udp();l.test_source(h,ip,46440);l.test_data(h,b'BSKD',4);out=Address();size=C.c_int(C.sizeof(out))
   self.assertEqual(l.bs_at_recvfrom(s,buf,512,0,C.byref(out),C.byref(size)),4)
   self.assertEqual(out.ip,ip);self.assertNotEqual(l.test_udp(),h)
 def test_send_backpressure_and_payload_bound(self):
  l=self.lib;s=l.bs_at_socket(2,1,0);a=Address(2,46440,0x0a000001)
  self.assertEqual(l.bs_at_connect(s,C.byref(a),C.sizeof(a)),-1);self.assertEqual(l.bsp_wouldblock(),1)
  l.test_state(l.test_tcp(),4);self.assertEqual(l.bsp_connected(s),1)
  l.test_send_error(-1);self.assertEqual(l.bsp_send(s,b'x',1),-1);self.assertEqual(l.bsp_wouldblock(),1)
  l.test_send_error(0);self.assertEqual(l.bsp_send(s,b'x'*5000,5000),4096)
 def test_locked_ndb_is_not_dereferenced(self):
  l=self.lib;s=self.listener(2);l.test_locked(l.test_udp(),1);out=Address();size=C.c_int(C.sizeof(out));buf=C.create_string_buffer(10)
  self.assertEqual(l.bs_at_recvfrom(s,buf,10,0,C.byref(out),C.byref(size)),-1);self.assertEqual(l.bsp_wouldblock(),1)
 def test_udp_reply_uses_listener_source_port(self):
  l=self.lib;s=self.listener(2);a=Address(2,53535,0x0a000001)
  self.assertEqual(l.bs_at_sendto(s,b'BSKD',4,0,C.byref(a),C.sizeof(a)),4);self.assertEqual(l.test_port(),46440)

 def test_tos_83_paths_and_names_are_explicit(self):
  for p in (b'C:\\BIN\\FILE.TXT',b'FILE.TXT'):self.assertEqual(self.lib.bs_at_valid_path(p),1)
  for p in (b'C:\\TOOLONGNAME.TXT',b'C:\\A.LONG',b'C:\\SPACE NAME.TXT',b'C:\\NONASCII\xff.TXT'):self.assertEqual(self.lib.bs_at_valid_path(p),0)
