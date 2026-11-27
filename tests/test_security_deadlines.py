"""A peer cannot keep negotiation alive by trickling partial frames."""
import ctypes as C,os,time,unittest
import test_core as base
class Deadlines(unittest.TestCase):
 setUpClass=classmethod(base.CoreTests.setUpClass.__func__)
 setUp=base.CoreTests.setUp;tearDown=base.CoreTests.tearDown
 client=base.CoreTests.client;status=base.CoreTests.status;until=base.CoreTests.until;raw_peer=base.CoreTests.raw_peer;start=base.CoreTests.start
 def test_partial_header_activity_does_not_extend_deadline(self):
  self.lib.bs_error_code.argtypes=[C.c_void_p]
  peer=self.raw_peer()
  for byte in b'BSK1\x00\x01':
   peer.sendall(bytes([byte]))
   for _ in range(3):self.lib.bs_tick(self.b);time.sleep(.002)
   self.lib.bs_test_advance(3);self.lib.bs_tick(self.b)
  self.assertFalse(self.status(self.b).active);self.assertIn(b'Handshake deadline exceeded',self.status(self.b).message)
  self.assertEqual(self.lib.bs_error_code(self.b),7)
  self.start(b'legitimate sender after stalled peer');destination=self.root/'received'
  self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(destination)),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(destination.read_bytes(),b'legitimate sender after stalled peer')

 def test_manifest_partial_item_cannot_keep_connection_busy(self):
  import struct
  self.lib.bs_batch_enable.argtypes=[C.c_void_p,C.c_int];self.lib.bs_batch_enable(self.b,1)
  peer=self.raw_peer();peer.setsockopt(__import__('socket').IPPROTO_TCP,__import__('socket').TCP_NODELAY,1)
  greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX40'
  peer.sendall(base.frame(1,greeting)+base.frame(16,b'x'*16+struct.pack('>II',0,2)))
  for _ in range(20):self.lib.bs_tick(self.b);time.sleep(.002)
  self.assertIn(b'file list',self.status(self.b).message)
  for byte in base.frame(17)[:6]:
   peer.sendall(bytes([byte]))
   for _ in range(3):self.lib.bs_tick(self.b);time.sleep(.002)
   self.lib.bs_test_advance(3);self.lib.bs_tick(self.b)
  self.assertFalse(self.status(self.b).active);self.assertIn(b'File-list deadline',self.status(self.b).message)
