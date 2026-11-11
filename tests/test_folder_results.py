"""Folder results remain available after delivery; retry keeps only failed payloads."""
import ctypes as C,os,unittest
from test_folders import Folders
class Results(Folders):
 def setUp(self):
  super().setUp();l=self.lib
  for name,args in {'bs_batch_summary':[C.c_void_p,C.c_char_p,C.c_size_t],'bs_run_summary':[C.c_void_p,C.c_int,C.c_char_p,C.c_size_t],'bs_retry_failed':[C.c_void_p],'bs_test_space':[C.c_int,C.c_ulong,C.c_ulong,C.c_ulong],'bs_queue_skip':[C.c_void_p],'bs_queue_continue':[C.c_void_p],'bs_completion_notice':[C.c_void_p,C.POINTER(C.c_ulong)]}.items():getattr(l,name).argtypes=args
 def summary(self,client,incoming=None):
  out=C.create_string_buffer(220)
  if incoming is None:self.assertEqual(self.lib.bs_batch_summary(client,out,len(out)),1)
  else:self.assertEqual(self.lib.bs_run_summary(client,incoming,out,len(out)),1)
  return out.value
 def test_preview_sizes_and_final_summaries(self):
  src=self.root/'Source';(src/'Empty').mkdir(parents=True);(src/'Photo').write_bytes(b'x'*2048)
  result=self.receive(src,lambda _:self.assertEqual(self.summary(self.b),b'1 file, 2 folders, 2.0 KiB'))
  self.assertTrue((result/'Photo').exists());self.assertIn(b'1 completed files, 2 folders; 0 skipped; 0 failed',self.summary(self.a,0));self.assertIn(b'1 completed files, 2 folders; 0 skipped; 0 failed',self.summary(self.b,1))
  serial=C.c_ulong();self.assertEqual(self.lib.bs_completion_notice(self.a,C.byref(serial)),1);self.assertEqual(self.lib.bs_completion_notice(self.a,C.byref(serial)),0)
 def test_failed_nested_file_retry_does_not_repeat_completed_payload(self):
  l=self.lib;src=self.root/'Source';(src/'Nested').mkdir(parents=True)
  for name in ['a.bin','b.bin']:(src/'Nested'/name).write_bytes(name.encode()*2048)
  target=self.root/'Receive';target.mkdir();self.queue(src);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(target)),1)
  first=target/'Source'/'Nested';self.until(lambda:len(list(first.glob('*.bin')))==1)
  l.bs_test_space(1,0,0,4096);self.until(lambda:l.bs_queue_paused(self.a)!=0)
  self.assertIn(b'1 completed files',self.summary(self.a,0));self.assertIn(b'1 failed',self.summary(self.a,0))
  self.assertEqual(l.bs_retry_failed(self.a),0);l.bs_queue_clear(self.a);l.bs_test_space(-1,0,0,4096)
  self.assertEqual(l.bs_retry_failed(self.a),1);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertIn(b'1 file, 2 folders',self.summary(self.b));self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(target)),1)
  self.until(lambda:not l.bs_queue_count(self.a) and not self.status(self.b).active)
  second=target/'Source.1'/'Nested';self.assertEqual(len(list(second.glob('*.bin'))),1)
  self.assertEqual({p.name for p in first.glob('*.bin')}|{p.name for p in second.glob('*.bin')},{'a.bin','b.bin'})
  self.assertIn(b'2 completed files',self.summary(self.a,0));self.assertIn(b'0 failed',self.summary(self.a,0));self.assertEqual(l.bs_retry_failed(self.a),0)

 def test_skipped_files_are_reported_and_never_retried(self):
  l=self.lib;l.bs_conflict_policy.argtypes=[C.c_void_p,C.c_int];l.bs_conflict_policy(self.b,1)
  target=self.root/'Receive';target.mkdir();(target/'a.bin').write_bytes(b'keep')
  for name in ['a.bin','b.bin']:
   path=self.root/name;path.write_bytes(b'new');self.queue(path)
  self.until(lambda:self.status(self.b).awaiting_decision);self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(target)),1)
  self.until(lambda:not l.bs_queue_count(self.a) and not self.status(self.b).active)
  self.assertEqual((target/'a.bin').read_bytes(),b'keep');self.assertEqual((target/'b.bin').read_bytes(),b'new')
  self.assertIn(b'1 skipped; 0 failed',self.summary(self.a,0));self.assertIn(b'1 skipped; 0 failed',self.summary(self.b,1));self.assertEqual(l.bs_retry_failed(self.a),0)

def load_tests(loader,tests,pattern):return unittest.TestSuite(Results(n) for n in Results.__dict__ if n.startswith('test_'))
