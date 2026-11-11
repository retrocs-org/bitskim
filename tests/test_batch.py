"""Exercise negotiated manifests and narrowly scoped multi-file consent."""
import ctypes as C,os,socket,struct,time,unittest
from test_core import CoreTests,frame
class Item(C.Structure):
 _fields_=[('filename',C.c_char*256),('total',C.c_ulong)]
class BatchTests(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_batch_enable.argtypes=[C.c_void_p,C.c_int]
  for n in ('bs_batch_count','bs_batch_index','bs_queue_count','bs_queue_paused'):getattr(l,n).argtypes=[C.c_void_p]
  l.bs_batch_item.argtypes=[C.c_void_p,C.c_int];l.bs_batch_item.restype=C.POINTER(Item)
  l.bs_accept_batch.argtypes=[C.c_void_p,C.c_char_p]
  l.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
  for n in ('bs_queue_pause','bs_queue_continue','bs_queue_clear','bs_queue_skip'):getattr(l,n).argtypes=[C.c_void_p]
  l.bs_batch_enable(self.b,1)
 def queue(self,names):
  self.paths=[]
  for i,name in enumerate(names):
   directory=self.root/str(i);directory.mkdir();p=directory/name;p.write_bytes(bytes([i])*31)
   self.paths.append(p);self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
 def drain(self):
  end=time.monotonic()+15
  while time.monotonic()<end:
   for c in self.clients:self.lib.bs_tick(c)
   self.assertFalse(self.status(self.b).awaiting_decision,'Unexpected second approval')
   if not self.lib.bs_queue_count(self.a) and not self.status(self.b).active:return
   if self.lib.bs_queue_paused(self.a):self.fail(self.lib.bs_last_error(self.a).decode())
  self.fail('Batch did not finish')
 def test_full_manifest_and_single_consent(self):
  self.queue(['one.txt','two.txt','three.txt'])
  self.assertEqual(self.lib.bs_batch_count(self.b),3)
  self.assertEqual([self.lib.bs_batch_item(self.b,i).contents.filename for i in range(3)],[b'one.txt',b'two.txt',b'three.txt'])
  self.assertFalse(self.lib.bs_batch_item(self.b,3))
  folder=self.root/'receive';folder.mkdir();self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(folder)),1)
  self.drain()
  for p in self.paths:self.assertEqual((folder/p.name).read_bytes(),p.read_bytes())
 def test_duplicate_names_and_existing_file_are_preserved(self):
  self.queue(['same.txt','same.txt']);folder=self.root/'receive';folder.mkdir();(folder/'same.txt').write_bytes(b'keep')
  self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(folder)),1);self.drain()
  self.assertEqual((folder/'same.txt').read_bytes(),b'keep')
  self.assertEqual((folder/'same.1.txt').read_bytes(),self.paths[0].read_bytes());self.assertEqual((folder/'same.2.txt').read_bytes(),self.paths[1].read_bytes())
 def test_old_receiver_falls_back_to_per_file(self):
  self.lib.bs_batch_enable(self.b,0);self.queue(['one','two']);self.assertEqual(self.lib.bs_batch_count(self.b),0)
  self.lib.bs_accept(self.b,os.fsencode(self.root/'first'));self.until(lambda:self.lib.bs_queue_count(self.a)==1)
  self.until(lambda:self.status(self.b).awaiting_decision);self.assertEqual(self.status(self.b).filename,b'two')
 def test_decline_stops_entire_batch(self):
  self.queue(['one','two']);self.lib.bs_decline(self.b);self.until(lambda:self.lib.bs_queue_paused(self.a))
  self.assertEqual(self.lib.bs_queue_count(self.a),2)
 def test_one_file_accept_does_not_grant_other_files(self):
  self.queue(['one','two']);self.lib.bs_accept(self.b,os.fsencode(self.root/'one'))
  self.until(lambda:self.lib.bs_queue_count(self.a)==1);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_batch_index(self.b),1)
 def test_approval_expires_between_files(self):
  self.queue(['one','two']);self.lib.bs_queue_pause(self.a);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder))
  self.until(lambda:self.lib.bs_queue_count(self.a)==1 and not self.status(self.b).active)
  self.lib.bs_test_advance(121);self.lib.bs_queue_continue(self.a);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertFalse((folder/'two').exists())
 def test_new_files_do_not_join_an_approved_snapshot(self):
  self.queue(['one','two']);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder))
  p=self.root/'later';p.write_bytes(b'later');self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.lib.bs_queue_count(self.a)==1);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.status(self.b).filename,b'later');self.assertEqual(self.lib.bs_batch_count(self.b),0)
 def test_changed_source_size_is_not_sent_under_old_approval(self):
  self.queue(['one','two']);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder));self.paths[1].write_bytes(b'changed')
  self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertFalse((folder/'two').exists())
 def test_cancel_revokes_the_batch(self):
  self.queue(['one','two']);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder));self.lib.bs_cancel(self.b)
  self.until(lambda:not self.status(self.a).active);self.assertEqual(self.lib.bs_queue_count(self.a),0);self.assertFalse(self.status(self.b).awaiting_decision)
 def test_invalid_name_anywhere_rejects_before_any_acceptance(self):
  s=socket.create_connection(('127.0.0.1',self.pb));self.sockets.append(s)
  greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,4)+b'TestTest'
  items=[(b'good',1),(b'../escape',1)]
  packet=b'x'*16+bytes([0,2,0])+b''.join(struct.pack('>IH',size,len(name))+name for name,size in items)
  s.sendall(frame(1,greeting)+frame(15,packet));self.until(lambda:b'Invalid batch' in self.status(self.b).message)
  self.assertFalse(self.status(self.b).awaiting_decision)

 def prepare_approved_second(self):
  self.queue(['one','two']);token=(C.c_ubyte*16)();self.lib.bs_test_batch_token.argtypes=[C.c_void_p,C.c_void_p]
  self.assertEqual(self.lib.bs_test_batch_token(self.b,token),1)
  self.lib.bs_queue_pause(self.a);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder))
  self.until(lambda:self.lib.bs_queue_count(self.a)==1 and not self.status(self.b).active)
  return bytes(token),folder
 def raw_batch(self,token,index=1,names=(b'one',b'two'),identity='a',port=None):
  s=socket.create_connection(('127.0.0.1',self.pb));self.sockets.append(s)
  greeting=bytes.fromhex(identity*32)+struct.pack('>HBB',self.pa if port is None else port,4,4)+b'TestTest'
  packet=token+bytes([index,2,0])+b''.join(struct.pack('>IH',31,len(name))+name for name in names)
  s.sendall(frame(1,greeting)+frame(15,packet));self.until(lambda:self.status(self.b).awaiting_decision)
 def test_changed_manifest_requires_fresh_consent(self):
  token,folder=self.prepare_approved_second();self.raw_batch(token,names=(b'one',b'other'))
  self.assertEqual(self.status(self.b).filename,b'other');self.assertFalse((folder/'other').exists())
 def test_other_sender_cannot_use_approval(self):
  token,folder=self.prepare_approved_second();self.raw_batch(token,identity='d');self.assertFalse((folder/'two').exists())
 def test_other_port_cannot_use_approval(self):
  token,folder=self.prepare_approved_second();self.raw_batch(token,port=1 if self.pa!=1 else 2);self.assertFalse((folder/'two').exists())
 def test_reordered_or_replayed_entry_requires_consent(self):
  token,folder=self.prepare_approved_second();self.raw_batch(token,index=0)
  self.assertEqual(self.status(self.b).filename,b'one');self.assertEqual(len(list(folder.iterdir())),1)
 def test_different_batch_token_requires_consent(self):
  token,folder=self.prepare_approved_second();self.raw_batch(b'z'*16);self.assertFalse((folder/'two').exists())
 def test_eight_maximum_length_names_fit_manifest(self):
  self.queue(['a'*251+'.txt']*8);self.assertEqual(self.lib.bs_batch_count(self.b),8)
  folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder));self.drain()
  self.assertEqual(len(list(folder.iterdir())),8)
 def test_different_recipient_is_not_in_manifest(self):
  c,pc=self.client(b'd'*32);self.lib.bs_queue_pause(self.a)
  for i,port in enumerate((self.pb,self.pb,pc)):
   p=self.root/str(i);p.write_bytes(b'x');self.lib.bs_queue_address(self.a,b'127.0.0.1',port,os.fsencode(p))
  self.lib.bs_queue_continue(self.a);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_batch_count(self.b),2)

 def test_destination_appearing_after_approval_is_not_overwritten(self):
  self.queue(['one','two']);folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder))
  (folder/'two').write_bytes(b'keep this')
  self.drain()
  self.assertEqual((folder/'two').read_bytes(),b'keep this')
  self.assertEqual((folder/'two.1').read_bytes(),self.paths[1].read_bytes())

 def test_large_collection_needs_only_one_approval(self):
  folder=self.root/'receive';folder.mkdir();names=[]
  for i in range(300):
   name='file%04d.txt'%i;names.append(name.encode());p=self.root/name;p.write_bytes(bytes([i%256])*7)
   self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_batch_count(self.b),300)
  for i in [0,7,8,255,299]:self.assertEqual(self.lib.bs_batch_item(self.b,i).contents.filename,names[i])
  self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(folder)),1);self.drain()
  for i,name in enumerate(names):self.assertEqual((folder/os.fsdecode(name)).read_bytes(),bytes([i%256])*7)

 def test_clear_active_collection_does_not_reuse_old_manifest(self):
  self.queue(['one','two']);self.lib.bs_queue_clear(self.a)
  folder=self.root/'receive';folder.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(folder))
  self.until(lambda:self.lib.bs_queue_count(self.a)==0 and not self.status(self.b).active);self.lib.bs_queue_continue(self.a)
  p=self.root/'newfile';p.write_bytes(b'new');self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.status(self.b).filename,b'newfile');self.assertEqual(self.lib.bs_batch_count(self.b),0)

 def stream_socket(self):
  s=socket.create_connection(('127.0.0.1',self.pb));self.sockets.append(s)
  platform=b'Test|BX0c';greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,len(platform))+b'Test'+platform
  s.sendall(frame(1,greeting));return s
 def test_incomplete_stream_never_prompts_or_creates_files(self):
  s=self.stream_socket();s.sendall(frame(16,b'x'*16+struct.pack('>II',0,2))+frame(17,struct.pack('>IH',1,4)+b'good')+frame(18,b''))
  self.until(lambda:b'Invalid collection' in self.status(self.b).message);self.assertFalse(self.status(self.b).awaiting_decision)
 def test_oversized_stream_count_rejected(self):
  s=self.stream_socket();s.sendall(frame(16,b'x'*16+struct.pack('>II',0,32761)))
  self.until(lambda:b'Invalid collection' in self.status(self.b).message);self.assertFalse(self.status(self.b).awaiting_decision)
 def test_stream_bad_filename_rejected_before_consent(self):
  s=self.stream_socket();s.sendall(frame(16,b'x'*16+struct.pack('>II',0,2))+frame(17,struct.pack('>IH',1,9)+b'../escape'))
  self.until(lambda:b'Invalid collection' in self.status(self.b).message);self.assertFalse(self.status(self.b).awaiting_decision)
 def test_temporary_storage_failure_is_reported(self):
  self.lib.bs_test_fail_temp.argtypes=[C.c_int];self.lib.bs_test_fail_temp(1)
  p=self.root/'source';p.write_bytes(b'x')
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),0)
  self.assertIn(b'temporary disk space',self.lib.bs_last_error(self.a));self.lib.bs_test_fail_temp(0)
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)

def load_tests(loader,tests,pattern):
 return unittest.TestSuite(BatchTests(n) for n in BatchTests.__dict__ if n.startswith('test_'))
