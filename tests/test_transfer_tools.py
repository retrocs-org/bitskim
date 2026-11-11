"""Real socket transfers: collision decisions, readback failure and bounded history."""
import ctypes as C,os,unittest
from test_core import CoreTests,hello,frame,offer
class Tools(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_conflict_policy.argtypes=[C.c_void_p,C.c_int]
  l.bs_accept_conflict.argtypes=[C.c_void_p,C.c_char_p]
  l.bs_history_count.argtypes=[C.c_void_p]
  l.bs_history_clear.argtypes=[C.c_void_p]
  l.bs_history_text.argtypes=[C.c_void_p,C.c_int,C.c_char_p,C.c_size_t]
  l.bs_diagnostics.argtypes=[C.c_void_p,C.c_char_p,C.c_size_t]
  l.bs_retry_history.argtypes=[C.c_void_p,C.c_int]
  l.bs_suggest_destination.argtypes=[C.c_char_p,C.c_char_p,C.c_int,C.c_char_p,C.c_size_t]
 def done(self):self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
 def report(self,client,index=0):
  out=C.create_string_buffer(2048);self.assertEqual(self.lib.bs_history_text(client,index,out,len(out)),1);return out.value
 def test_rename_preserves_extension_and_existing_file(self):
  dest=self.root/'report.pdf';dest.write_bytes(b'original');self.start(b'new')
  self.assertEqual(self.lib.bs_accept_conflict(self.b,os.fsencode(dest)),1);self.done()
  self.assertEqual(dest.read_bytes(),b'original');self.assertEqual((self.root/'report.1.pdf').read_bytes(),b'new')
 def test_replace_is_explicit_and_only_after_verification(self):
  dest=self.root/'replace.txt';dest.write_bytes(b'original');self.start(b'replacement');self.lib.bs_conflict_policy(self.b,2)
  self.assertEqual(self.lib.bs_accept_conflict(self.b,os.fsencode(dest)),1)
  self.assertEqual(dest.read_bytes(),b'original');self.done();self.assertEqual(dest.read_bytes(),b'replacement')
  self.assertFalse(list(self.root.glob('BK*')))
 def test_skip_negotiated_file_preserves_destination(self):
  dest=self.root/'exists';dest.write_bytes(b'original');self.start(b'ignored');self.lib.bs_conflict_policy(self.b,1)
  self.assertEqual(self.lib.bs_accept_conflict(self.b,os.fsencode(dest)),1);self.done()
  self.assertEqual(self.lib.bs_transfer_result(self.a),5);self.assertEqual(dest.read_bytes(),b'original');self.assertIn(b'Skipped',self.report(self.b))
 def test_legacy_skip_is_refused_without_sending_unknown_frame(self):
  dest=self.root/'exists';dest.write_bytes(b'original');s=self.raw_peer();s.sendall(frame(1,hello())+frame(2,offer(b'exists',3)))
  self.until(lambda:self.status(self.b).awaiting_decision);self.lib.bs_conflict_policy(self.b,1)
  self.assertEqual(self.lib.bs_accept_conflict(self.b,os.fsencode(dest)),0);self.assertTrue(self.status(self.b).awaiting_decision)
  self.assertIn(b'cannot skip',self.lib.bs_last_error(self.b))
 def test_corrupt_written_file_is_not_committed_or_reported_delivered(self):
  dest=self.root/'verified';self.start(b'contents'*2000);self.lib.bs_conflict_policy(self.b,2);dest.write_bytes(b'original')
  self.assertEqual(self.lib.bs_accept_conflict(self.b,os.fsencode(dest)),1)
  self.until(lambda:b'Checking written' in self.status(self.b).message)
  parts=list(self.root.glob('BS*.part'));self.assertEqual(len(parts),1);parts[0].write_bytes(b'corrupted')
  self.done();self.assertEqual(dest.read_bytes(),b'original');self.assertNotEqual(self.lib.bs_transfer_result(self.a),1)
  self.assertIn(b'integrity check',self.report(self.b))
 def test_history_retains_success_and_failure_and_is_bounded(self):
  for i in range(10):
   self.start(bytes([i])*100);dest=self.root/str(i)
   if i%2:self.lib.bs_decline(self.b)
   else:self.lib.bs_accept(self.b,os.fsencode(dest))
   self.done()
  self.assertEqual(self.lib.bs_history_count(self.b),8);self.assertIn(b'declined',self.report(self.b).lower());self.assertIn(b'written file verified',self.report(self.b,1))
  self.lib.bs_history_clear(self.b);self.assertEqual(self.lib.bs_history_count(self.b),0)
 def test_diagnostics_are_bounded_and_do_not_expose_secrets(self):
  out=C.create_string_buffer(2048);self.assertEqual(self.lib.bs_diagnostics(self.a,out,len(out)),1)
  self.assertIn(b'TCP ',out.value);self.assertIn(b'Negotiation:',out.value);self.assertNotIn(b'pairing_key',out.value)
  tiny=C.create_string_buffer(8);self.assertEqual(self.lib.bs_diagnostics(self.a,tiny,len(tiny)),0);self.assertEqual(tiny.raw[-1],0)
 def test_failed_outgoing_retry_uses_saved_stable_peer(self):
  self.start(b'retry me');self.lib.bs_decline(self.b);self.done();self.assertEqual(self.lib.bs_retry_history(self.a,0),1)
  self.assertEqual(self.lib.bs_retry_history(self.b,0),0)
 def test_collection_skip_continues_remaining_files_without_new_consent(self):
  l=self.lib
  l.bs_batch_enable.argtypes=[C.c_void_p,C.c_int];l.bs_accept_batch.argtypes=[C.c_void_p,C.c_char_p]
  l.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p];l.bs_queue_count.argtypes=[C.c_void_p]
  l.bs_batch_enable(self.b,1);l.bs_conflict_policy(self.b,1)
  folder=self.root/'batch';folder.mkdir();(folder/'first.txt').write_bytes(b'original')
  for name in ('first.txt','second.txt','third.txt'):
   source=self.root/name;source.write_bytes(name.encode());self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(folder)),1)
  self.until(lambda:not l.bs_queue_count(self.a) and not self.status(self.b).active)
  self.assertEqual((folder/'first.txt').read_bytes(),b'original')
  self.assertEqual((folder/'second.txt').read_bytes(),b'second.txt');self.assertEqual((folder/'third.txt').read_bytes(),b'third.txt')
 def test_legacy_sender_gets_original_receipt_protocol(self):
  import struct
  s=self.raw_peer();data=b'legacy contents';s.sendall(frame(1,hello())+frame(2,offer(b'legacy.txt',len(data))))
  self.until(lambda:self.status(self.b).awaiting_decision)
  dest=self.root/'legacy.txt';self.lib.bs_accept(self.b,os.fsencode(dest))
  s.sendall(frame(5,data)+frame(6,struct.pack('>I',self.lib.bs_crc32(0,data,len(data)))))
  self.until(lambda:dest.exists());self.assertEqual(dest.read_bytes(),data);self.assertNotIn(b'written file verified',self.report(self.b))
def load_tests(loader,tests,pattern):return unittest.TestSuite(Tools(n) for n in Tools.__dict__ if n.startswith('test_'))
