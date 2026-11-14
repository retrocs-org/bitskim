"""Deterministic volume availability; real receive/manifest code and bounded 64-bit sums."""
import ctypes as C,os,unittest
from test_core import CoreTests
class Storage(C.Structure):
 _fields_=[(n,C.c_ulong) for n in ('required_high','required_low','available_high','available_low','allocation_unit')]+[('known',C.c_int),('enough',C.c_int)]
class DiskSpace(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_queue_paused.argtypes=[C.c_void_p]
  l.bs_test_space.argtypes=[C.c_int,C.c_ulong,C.c_ulong,C.c_ulong]
  l.bs_storage_info.argtypes=[C.c_void_p,C.c_char_p,C.c_int,C.POINTER(Storage)]
  l.bs_storage_report.argtypes=[C.c_void_p,C.c_char_p,C.c_int,C.c_char_p,C.c_size_t]
  l.bs_accept_batch.argtypes=[C.c_void_p,C.c_char_p];l.bs_batch_enable.argtypes=[C.c_void_p,C.c_int]
  l.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
 def test_known_insufficient_space_refuses_before_creating_temp(self):
  self.start(b'x'*500);self.lib.bs_test_space(1,0,100,4096)
  self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(self.root/'target')),0)
  self.assertTrue(self.status(self.b).awaiting_decision);self.assertEqual(list(self.root.glob('BS*.part')),[])
  self.assertIn(b'Not enough disk space',self.lib.bs_last_error(self.b))
 def test_overhead_is_included_and_64bit_availability_is_not_truncated(self):
  self.start(b'x'*500);self.lib.bs_test_space(1,2,42,65536);info=Storage()
  self.assertEqual(self.lib.bs_storage_info(self.b,os.fsencode(self.root),0,C.byref(info)),1)
  self.assertEqual((info.available_high,info.available_low),(2,42));self.assertGreater(info.required_low,500);self.assertTrue(info.enough)
 def test_unknown_space_is_explicit_and_streamed_write_checks_remain(self):
  self.start(b'unknown');self.lib.bs_test_space(0,0,0,4096);text=C.create_string_buffer(256)
  self.assertEqual(self.lib.bs_storage_report(self.b,os.fsencode(self.root),0,text,len(text)),1);self.assertIn(b'unavailable',text.value)
  self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(self.root/'target')),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active);self.assertEqual((self.root/'target').read_bytes(),b'unknown')
 def test_collection_is_checked_before_any_approval_or_output(self):
  l=self.lib;l.bs_batch_enable(self.b,1)
  for i in range(3):
   p=self.root/str(i);p.write_bytes(b'x'*10000);self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.until(lambda:self.status(self.b).awaiting_decision);target=self.root/'receive';target.mkdir();l.bs_test_space(1,0,20000,4096)
  self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(target)),0);self.assertEqual(list(target.iterdir()),[]);self.assertIn(b'Not enough disk space',l.bs_last_error(self.b))
 def test_collection_totals_above_four_gibibytes_do_not_wrap(self):
  l=self.lib;l.bs_batch_enable(self.b,1)
  for i in range(3):
   p=self.root/str(i)
   with p.open('wb') as f:f.truncate(2147483647)
   self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.until(lambda:self.status(self.b).awaiting_decision);l.bs_test_space(1,1,0,4096);info=Storage()
  self.assertEqual(l.bs_storage_info(self.b,os.fsencode(self.root),1,C.byref(info)),1)
  self.assertEqual(info.required_high,1);self.assertGreater(info.required_low,2147483647);self.assertFalse(info.enough)
  self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(self.root)),0)
 def test_space_is_checked_again_after_collection_approval(self):
  l=self.lib;l.bs_batch_enable(self.b,1)
  for i in range(2):
   p=self.root/str(i);p.write_bytes(b'x'*10000);l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision);target=self.root/'receive';target.mkdir();self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(target)),1)
  l.bs_test_space(1,0,0,4096);self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.until(lambda:l.bs_queue_paused(self.a)!=0);self.assertEqual((target/'0').read_bytes(),b'x'*10000);self.assertFalse((target/'1').exists())
 def test_destination_info_distinguishes_known_space_from_unknown(self):
  self.lib.bs_destination_space_info.argtypes=[C.c_void_p,C.c_char_p,C.c_int,C.POINTER(Storage)]
  self.start(b'small');info=Storage();destination=os.fsencode(self.root/'target')
  for known,available,expected in ((1,1000000,1),(1,1,0),(0,0,1)):
   self.lib.bs_test_space(known,0,available,4096)
   self.assertEqual(self.lib.bs_destination_space_info(self.b,destination,0,C.byref(info)),1)
   self.assertEqual(info.known,known);self.assertEqual(info.enough,expected)
  self.assertEqual(self.lib.bs_destination_space_info(self.b,None,0,C.byref(info)),0);self.assertFalse(info.known)

def load_tests(loader,tests,pattern):return unittest.TestSuite(DiskSpace(n) for n in DiskSpace.__dict__ if n.startswith('test_'))
