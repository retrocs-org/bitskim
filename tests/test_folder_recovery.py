"""Folder recovery re-approves a manifest, checks existing bytes, and retains originals."""
import ctypes as C,os,unittest
import test_secure_resume as modern
class FolderRecovery(unittest.TestCase):
 setUpClass=classmethod(modern.ModernTests.setUpClass.__func__)
 tearDown=modern.ModernTests.tearDown;client=modern.ModernTests.client;status=modern.ModernTests.status;until=modern.ModernTests.until;secure=modern.ModernTests.secure
 def setUp(self):
  modern.ModernTests.setUp(self);l=self.lib
  for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_accept_batch_resume':[C.c_void_p,C.c_char_p],'bs_batch_can_resume':[C.c_void_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],'bs_queue_paused':[C.c_void_p]}.items():getattr(l,name).argtypes=args
  self.secure()
  for c in (self.a,self.b):l.bs_batch_enable(c,1);self.assertEqual(l.bs_resume_enable(c,1),1)
  self.source=self.root/'Tree';self.source.mkdir();(self.source/'A.bin').write_bytes(b'first');(self.source/'B.bin').write_bytes(b'payload'*16000)
  self.dest=self.root/'received';self.dest.mkdir()
 def offer(self):
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(self.source)),1);self.until(lambda:self.status(self.b).awaiting_decision)
 def finish(self):self.until(lambda:self.lib.bs_queue_count(self.a)==0,timeout=10)
 def test_matching_existing_file_is_verified_and_missing_files_are_received(self):
  existing=self.dest/'Tree';existing.mkdir();(existing/'A.bin').write_bytes(b'first');(existing/'unlisted').write_bytes(b'keep')
  self.offer();self.assertTrue(self.lib.bs_batch_can_resume(self.b));self.assertEqual(self.lib.bs_accept_batch_resume(self.b,os.fsencode(self.dest)),1);self.finish()
  self.assertEqual((existing/'B.bin').read_bytes(),(self.source/'B.bin').read_bytes());self.assertEqual((existing/'unlisted').read_bytes(),b'keep');self.assertFalse(list(existing.glob('*.bsk-*')))
 def test_same_size_different_content_is_never_overwritten(self):
  existing=self.dest/'Tree';existing.mkdir();(existing/'A.bin').write_bytes(b'other')
  self.offer();self.assertEqual(self.lib.bs_accept_batch_resume(self.b,os.fsencode(self.dest)),1)
  self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertEqual((existing/'A.bin').read_bytes(),b'other');self.assertIn(b'does not match',self.status(self.a).message)
 def test_interrupted_folder_resumes_after_sender_restart(self):
  self.lib.bs_limits_configure.argtypes=[C.c_void_p,C.c_ulong,C.c_ulong];self.lib.bs_limits_configure(self.a,16384,0)
  self.offer();self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(self.dest)),1)
  self.until(lambda:self.status(self.b).filename.endswith(b'B.bin') and 0<self.status(self.b).transferred<self.status(self.b).total)
  self.lib.bs_destroy(self.a);self.clients.remove(self.a);self.until(lambda:not self.status(self.b).active)
  partial=self.dest/'Tree/B.bin.bsk-part';self.assertTrue(partial.is_file());self.assertGreater(partial.stat().st_size,0)
  self.a,self.pa=self.client(b'a'*32);self.lib.bs_security_configure(self.a,2,b'12'*32);self.lib.bs_batch_enable(self.a,1);self.lib.bs_resume_enable(self.a,1)
  self.offer();self.assertEqual(self.lib.bs_accept_batch_resume(self.b,os.fsencode(self.dest)),1);self.finish()
  self.assertEqual((self.dest/'Tree/B.bin').read_bytes(),(self.source/'B.bin').read_bytes());self.assertFalse(partial.exists());self.assertEqual((self.dest/'Tree/A.bin').read_bytes(),b'first')
 def test_unpaired_peer_cannot_enable_existing_folder_recovery(self):
  for c in (self.a,self.b):self.lib.bs_security_configure(c,1,b'')
  self.offer();self.assertFalse(self.lib.bs_batch_can_resume(self.b));self.assertEqual(self.lib.bs_accept_batch_resume(self.b,os.fsencode(self.dest)),0)
  self.assertTrue(self.status(self.b).awaiting_decision);self.assertFalse((self.dest/'Tree').exists())

 def test_existing_link_target_is_preserved(self):
  existing=self.dest/'Tree';existing.mkdir();outside=self.root/'outside';outside.write_bytes(b'first');(existing/'A.bin').symlink_to(outside)
  self.offer();self.assertEqual(self.lib.bs_accept_batch_resume(self.b,os.fsencode(self.dest)),1);self.until(lambda:self.lib.bs_queue_paused(self.a))
  self.assertEqual(outside.read_bytes(),b'first');self.assertTrue((existing/'A.bin').is_symlink());self.assertFalse((existing/'A.bin.bsk-part').exists())
