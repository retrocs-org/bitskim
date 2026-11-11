"""Actual loopback engine, controlled monotonic time, bounded payload pacing."""
import ctypes as C,os,unittest
from test_core import CoreTests,frame,hello,offer
class RateLimits(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_limits_configure.argtypes=[C.c_void_p,C.c_ulong,C.c_ulong]
  l.bs_limits_get.argtypes=[C.c_void_p,C.POINTER(C.c_ulong),C.POINTER(C.c_ulong)]
  l.bs_limit_parse.argtypes=[C.c_char_p,C.POINTER(C.c_ulong)]
  l.bs_test_advance_ms.argtypes=[C.c_ulong]
 def limited(self,send=0,receive=0,size=32768):
  self.assertEqual(self.lib.bs_limits_configure(self.a,send,0),1)
  self.assertEqual(self.lib.bs_limits_configure(self.b,0,receive),1)
  data=os.urandom(size);self.start(data);dest=self.root/'limited.bin'
  self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
  rate=min([n for n in (send,receive) if n]);last=0
  for second in range(1,80):
   self.lib.bs_test_advance(1)
   for _ in range(16):self.lib.bs_tick(self.a);self.lib.bs_tick(self.b)
   current=self.status(self.b).transferred
   self.assertLessEqual(current,rate*second+128)
   self.assertGreaterEqual(current,last);last=current
   self.assertNotIn(b'timed out',self.status(self.a).message)
   if not self.status(self.a).active and not self.status(self.b).active:break
  else:self.fail('Paced transfer did not finish')
  self.assertEqual(dest.read_bytes(),data)
  self.assertEqual(self.lib.bs_transfer_result(self.a),1)
 def test_upload_limit_and_timeout_safety(self):self.limited(send=1024)
 def test_receiver_limit_paces_the_sender(self):self.limited(receive=1024)
 def test_lower_of_send_and_receive_limits_wins(self):self.limited(send=4096,receive=1024)
 def test_default_limits_are_unlimited_and_parser_is_strict(self):
  tx=C.c_ulong(1);rx=C.c_ulong(1);self.lib.bs_limits_get(self.a,C.byref(tx),C.byref(rx));self.assertEqual((tx.value,rx.value),(0,0))
  for text,okay,expected in [(b'0',1,0),(b'128',1,131072),(b'1048575',1,1073740800),(b'1048576',0,0),(b'-1',0,0),(b'1.5',0,0),(b'',0,0),(b'999999999999999999',0,0)]:
   result=C.c_ulong();self.assertEqual(self.lib.bs_limit_parse(text,C.byref(result)),okay)
   if okay:self.assertEqual(result.value,expected)
  self.assertEqual(self.lib.bs_limits_configure(self.a,1073740801,0),0)
 def test_receive_limit_refuses_legacy_sender_before_approval(self):
  self.lib.bs_limits_configure(self.b,0,1024);s=self.raw_peer();s.sendall(frame(1,hello())+frame(2,offer(b'old.bin',1)))
  self.until(lambda:b'updated sender' in self.status(self.b).message)
  self.assertFalse(self.status(self.b).awaiting_decision)
 def test_cancel_remains_responsive_while_paced(self):
  self.lib.bs_limits_configure(self.a,1024,0);self.start(b'x'*8192);dest=self.root/'cancelled'
  self.lib.bs_accept(self.b,os.fsencode(dest));self.until(lambda:b'Sending' in self.status(self.a).message)
  self.lib.bs_cancel(self.a);self.until(lambda:not self.status(self.b).active)
  self.assertFalse(dest.exists())
 def test_new_configuration_applies_to_next_connection(self):
  self.lib.bs_limits_configure(self.a,1024,0);self.start(b'x'*8192)
  self.assertEqual(self.lib.bs_limits_configure(self.a,8192,0),1)
  tx=C.c_ulong();self.lib.bs_limits_get(self.a,C.byref(tx),None);self.assertEqual(tx.value,8192)
  dest=self.root/'snapshot';self.lib.bs_accept(self.b,os.fsencode(dest));self.lib.bs_test_advance(1)
  for _ in range(16):self.lib.bs_tick(self.a);self.lib.bs_tick(self.b)
  self.assertLessEqual(self.status(self.b).transferred,1152)
 def test_partial_writes_do_not_spend_credit_twice(self):
  self.lib.bs_test_send_limit(19);self.limited(send=1024,size=4096)
def load_tests(loader,tests,pattern):return unittest.TestSuite(RateLimits(n) for n in RateLimits.__dict__ if n.startswith('test_'))
