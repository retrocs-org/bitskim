import ctypes as C,os
from test_core import CoreTests
class QueueTests(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
  l.bs_queue_count.argtypes=[C.c_void_p];l.bs_queue_paused.argtypes=[C.c_void_p]
  l.bs_queue_waiting.argtypes=[C.c_void_p]
  l.bs_queue_limit.argtypes=[C.c_void_p]
  for n in ('bs_queue_continue','bs_queue_skip','bs_queue_clear'):getattr(l,n).argtypes=[C.c_void_p]
 def test_fifo_and_per_file_consent(self):
  for n in range(3):
   p=self.root/('q%d.bin'%n);p.write_bytes(bytes([n])*17);self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  for n in range(3):
   self.until(lambda:self.status(self.b).awaiting_decision)
   self.assertEqual(self.status(self.b).filename,('q%d.bin'%n).encode())
   destination=self.root/('r%d'%n);self.lib.bs_accept(self.b,os.fsencode(destination))
   self.until(lambda:destination.exists() and not self.status(self.b).active)
   self.assertEqual(destination.read_bytes(),bytes([n])*17)
  self.until(lambda:self.lib.bs_queue_count(self.a)==0)
 def test_decline_pauses_and_explicit_skip_continues(self):
  p=self.root/'q';p.write_bytes(b'x')
  for _ in range(2):self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision);self.lib.bs_decline(self.b)
  self.until(lambda:self.lib.bs_queue_paused(self.a))
  self.assertEqual(self.lib.bs_queue_count(self.a),2)
  self.lib.bs_queue_skip(self.a);self.lib.bs_queue_continue(self.a)
  self.until(lambda:self.status(self.b).awaiting_decision)
 def test_bounded_queue_and_unknown_file(self):
  p=self.root/'q';p.write_bytes(b'x')
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,b'/no/such/file'),0)
  for _ in range(self.lib.bs_queue_limit(self.a)):self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),0)

 def test_stable_recipient_disappears_without_redirection(self):
  self.lib.bs_queue_peer.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p]
  p=self.root/'queued';p.write_bytes(b'only for b')
  self.advertise(b'b'*32,self.pb);self.advertise(b'd'*32,self.pb)
  self.until(lambda:self.lib.bs_peer_count(self.a)==2)
  self.lib.bs_queue_pause.argtypes=[C.c_void_p];self.lib.bs_queue_pause(self.a)
  self.assertEqual(self.lib.bs_queue_peer(self.a,b'b'*32,os.fsencode(p)),1)
  self.lib.bs_test_advance(91);self.lib.bs_tick(self.a)
  self.advertise(b'd'*32,self.pb);self.until(lambda:self.lib.bs_peer_count(self.a)==1)
  self.lib.bs_queue_continue(self.a);self.lib.bs_tick(self.a)
  self.assertEqual(self.lib.bs_queue_paused(self.a),1)
  self.assertFalse(self.status(self.b).active)
 def test_clear_waiting_preserves_active_file(self):
  p=self.root/'queued';p.write_bytes(b'x')
  for _ in range(3):self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_queue_clear(self.a)
  self.assertEqual(self.lib.bs_queue_count(self.a),1)
  self.assertTrue(self.status(self.a).active)
  dest=self.root/'received';self.lib.bs_accept(self.b,os.fsencode(dest))
  self.until(lambda:self.lib.bs_queue_count(self.a)==0)
  self.assertEqual(dest.read_bytes(),b'x')

 def test_pause_finishes_current_then_holds_next(self):
  self.lib.bs_queue_pause.argtypes=[C.c_void_p]
  p=self.root/'pause-file';p.write_bytes(b'payload'*10000)
  for _ in range(2):self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_queue_pause(self.a)
  destination=self.root/'first';self.lib.bs_accept(self.b,os.fsencode(destination))
  self.until(lambda:destination.exists() and not self.status(self.a).active)
  for _ in range(20):self.lib.bs_tick(self.a);self.lib.bs_tick(self.b)
  self.assertEqual(destination.read_bytes(),p.read_bytes())
  self.assertEqual(self.lib.bs_queue_count(self.a),1)
  self.assertFalse(self.status(self.b).awaiting_decision)
  self.lib.bs_queue_continue(self.a)
  self.until(lambda:self.status(self.b).awaiting_decision)

 def test_queue_snapshot_is_bounded_and_copied(self):
  class Item(C.Structure):
   _fields_=[('path',C.c_char*1024),('total',C.c_ulong),('active',C.c_int),('paused',C.c_int)]
  self.lib.bs_queue_item.argtypes=[C.c_void_p,C.c_int,C.POINTER(Item)]
  p=self.root/'snapshot';p.write_bytes(b'three')
  self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  item=Item();self.assertEqual(self.lib.bs_queue_item(self.a,0,C.byref(item)),1)
  self.assertEqual(item.path,os.fsencode(p));self.assertEqual(item.total,5)
  item.path=b'changed copy'
  self.assertEqual(self.lib.bs_queue_item(self.a,0,C.byref(item)),1)
  self.assertEqual(item.path,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_queue_item(self.a,0,C.byref(item));self.assertEqual(item.active,1)
  self.assertEqual(self.lib.bs_queue_item(self.a,-1,C.byref(item)),0)
  self.assertEqual(item.path,b'')
  self.assertEqual(self.lib.bs_queue_item(self.a,8,C.byref(item)),0)

 def test_queue_file_ordinal_and_reset(self):
  self.lib.bs_queue_progress.argtypes=[C.c_void_p,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong)]
  position=C.c_ulong();total=C.c_ulong()
  def progress():
   self.lib.bs_queue_progress(self.a,C.byref(position),C.byref(total));return position.value,total.value
  self.assertEqual(progress(),(0,0))
  p=self.root/'ordinal';p.write_bytes(b'x')
  for _ in range(2):self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.assertEqual(progress(),(1,2));self.assertEqual(self.lib.bs_queue_waiting(self.a),2)
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_queue_waiting(self.a),1)
  self.lib.bs_accept(self.b,os.fsencode(self.root/'first'))
  self.until(lambda:self.lib.bs_queue_count(self.a)==1)
  self.assertEqual(progress(),(2,2))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_accept(self.b,os.fsencode(self.root/'second'))
  self.until(lambda:self.lib.bs_queue_count(self.a)==0)
  self.assertEqual(progress(),(2,2))
  self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.assertEqual(progress(),(1,1))


def load_tests(loader, tests, pattern):
 return loader.loadTestsFromTestCase(QueueTests)
