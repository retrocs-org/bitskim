"""Discovery hints, live limits and the versioned profile contract."""
import ctypes as C,os,socket,struct,unittest
from test_core import CoreTests,frame
class Caps(C.Structure):
 _fields_=[('send_features',C.c_ulong),('receive_features',C.c_ulong),('max_send_file',C.c_ulong),('max_receive_file',C.c_ulong),('wire_name_bytes',C.c_ushort),('local_name_bytes',C.c_ushort),('path_bytes',C.c_ushort),('code_page',C.c_ushort),('security_mode',C.c_ubyte),('name_policy',C.c_ubyte),('paired',C.c_ubyte),('collection_files',C.c_ulong)]
def profile(max_receive=2147483647,local_name=30):
 return struct.pack('>4sBBBBIIIIHHHHI',b'BXC1',1,0,1,0,28,28,2147483647,max_receive,255,local_name,1023,0,32760)
class CapabilityTests(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_local_capabilities.argtypes=[C.c_void_p,C.POINTER(Caps)]
  l.bs_transfer_capabilities.argtypes=[C.c_void_p,C.POINTER(Caps)]
  l.bs_peer_capabilities.argtypes=[C.c_void_p,C.c_char_p,C.POINTER(Caps)]
 def test_local_directional_profile(self):
  caps=Caps();self.lib.bs_local_capabilities(self.a,C.byref(caps))
  self.assertEqual(caps.send_features&28,28);self.assertEqual(caps.receive_features,1808)
  self.assertEqual(caps.max_receive_file,2147483647);self.assertEqual(caps.wire_name_bytes,255)
  self.assertEqual(caps.collection_files,1);self.assertEqual(caps.security_mode,0)
  self.assertEqual(caps.send_features&64,64);self.assertEqual(caps.send_features&128,0)
 def test_live_handshake_reports_receiver_limits(self):
  self.start(b'hello');caps=Caps()
  self.assertEqual(self.lib.bs_transfer_capabilities(self.a,C.byref(caps)),1)
  self.assertEqual(caps.max_receive_file,2147483647);self.assertEqual(caps.collection_files,1)
  self.assertEqual(caps.receive_features&16,16)
 def advertise_profile(self,payload):
  udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.sockets.append(udp)
  greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX1c'
  udp.sendto((b'BSKD\x01\x02'+greeting+payload).ljust(160,b'\0'),('127.0.0.1',self.pa))
 def test_discovery_includes_sizes_names_roles_and_count(self):
  self.advertise_profile(profile());self.until(lambda:self.lib.bs_peer_count(self.a)==1)
  caps=Caps();self.assertEqual(self.lib.bs_peer_capabilities(self.a,b'c'*32,C.byref(caps)),1)
  self.assertEqual(caps.local_name_bytes,30);self.assertEqual(caps.name_policy,1);self.assertEqual(caps.collection_files,32760)
 def test_invalid_discovery_profile_is_not_a_claim(self):
  self.advertise_profile(b'BXC1'+b'\xff'*32);self.until(lambda:self.lib.bs_peer_count(self.a)==1)
  self.assertEqual(self.lib.bs_peer_capabilities(self.a,b'c'*32,C.byref(Caps())),0)
 def test_incomplete_live_profile_never_offers_files(self):
  s=self.raw_peer();greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX10'
  s.sendall(frame(1,greeting)+frame(21,b'BXC1'))
  self.until(lambda:b'Invalid capability' in self.status(self.b).message);self.assertFalse(self.status(self.b).awaiting_decision)
 def test_reported_sender_size_limit_is_enforced(self):
  s=self.raw_peer();greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX10'
  caps=bytearray(profile());caps[16:20]=struct.pack('>I',1)
  s.sendall(frame(1,greeting)+frame(21,caps)+frame(2,struct.pack('>IIH',0,2,1)+b'x'))
  self.until(lambda:b'reported file limit' in self.status(self.b).message);self.assertFalse(self.status(self.b).awaiting_decision)
def load_tests(loader,tests,pattern):
 return unittest.TestSuite(CapabilityTests(n) for n in CapabilityTests.__dict__ if n.startswith('test_'))
