"""Real streamed folder manifests, empty directories, safe paths and old peers."""
import ctypes as C,os,unittest,socket
from test_core import CoreTests,frame,hello
class Folders(CoreTests):
 def setUp(self):
  super().setUp();l=self.lib
  for name,args in {'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],'bs_queue_count':[C.c_void_p],'bs_queue_paused':[C.c_void_p],'bs_batch_enable':[C.c_void_p,C.c_int],'bs_batch_count':[C.c_void_p],'bs_batch_is_tree':[C.c_void_p],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_clear':[C.c_void_p],'bs_safe_relative':[C.c_char_p]}.items():getattr(l,name).argtypes=args
  l.bs_batch_enable(self.b,1)
 def queue(self,path):self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(path)),1)
 def receive(self,source,expected=None):
  self.queue(source);self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_batch_is_tree(self.b),1)
  folder=self.root/'incoming';folder.mkdir()
  if expected:expected(folder)
  self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(folder)),1)
  self.until(lambda:not self.lib.bs_queue_count(self.a) and not self.status(self.b).active,timeout=10)
  return folder/source.name
 def test_nested_files_and_empty_directories(self):
  source=self.root/'My Folder';(source/'sub'/'empty').mkdir(parents=True);(source/'root.txt').write_bytes(b'root');(source/'sub'/'zero').write_bytes(b'');(source/'sub'/'binary.bin').write_bytes(bytes(range(256))*40)
  out=self.receive(source)
  self.assertEqual((out/'root.txt').read_bytes(),b'root');self.assertTrue((out/'sub'/'empty').is_dir());self.assertEqual((out/'sub'/'binary.bin').read_bytes(),bytes(range(256))*40);self.assertEqual((out/'sub'/'zero').read_bytes(),b'')
 def test_empty_folder_uses_collection_approval(self):
  source=self.root/'Empty';source.mkdir();out=self.receive(source);self.assertTrue(out.is_dir());self.assertEqual(list(out.iterdir()),[])
 def test_existing_root_is_preserved_and_new_root_named(self):
  source=self.root/'Folder';source.mkdir();(source/'file.txt').write_bytes(b'new')
  out=self.receive(source,lambda folder:(folder/'Folder').mkdir());self.assertEqual(list(out.iterdir()),[]);self.assertEqual((out.parent/'Folder.1'/'file.txt').read_bytes(),b'new')
 def test_link_in_folder_aborts_listing_without_partial_send(self):
  source=self.root/'Unsafe';source.mkdir();(source/'good').write_bytes(b'ok');(source/'link').symlink_to(self.root)
  self.queue(source);self.until(lambda:self.lib.bs_queue_paused(self.a)!=0)
  self.assertFalse(self.status(self.b).awaiting_decision);self.assertIn(b'Cannot list folder',self.lib.bs_last_error(self.a))
 def test_multiple_folders_and_regular_file_keep_fifo_order(self):
  folders=[]
  for name in ('First','Second'):
   source=self.root/name;source.mkdir();(source/'file').write_bytes(name.encode());folders.append(source);self.queue(source)
  ordinary=self.root/'plain.txt';ordinary.write_bytes(b'plain');self.queue(ordinary)
  target=self.root/'receive';target.mkdir()
  for source in folders:
   self.until(lambda:self.status(self.b).awaiting_decision);self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(target)),1)
   self.until(lambda:(target/source.name/'file').exists())
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_accept(self.b,os.fsencode(target/'plain.txt'));self.until(lambda:not self.lib.bs_queue_count(self.a) and not self.status(self.b).active)
  self.assertEqual((target/'plain.txt').read_bytes(),b'plain')
 def test_relative_path_validation(self):
  for path in [b'Folder/',b'Folder/sub/file.txt',b'Folder/empty/']:self.assertEqual(self.lib.bs_safe_relative(path),1,path)
  for path in [b'/absolute',b'../escape',b'Folder/../escape',b'Folder//bad',b'Folder/./bad',b'C:/bad',b'Folder\\bad',b'Folder/evil\x00ignored'[:0],b'a/'*17+b'f']:
   self.assertEqual(self.lib.bs_safe_relative(path),0,path)
 def test_old_peer_is_refused_without_flattening(self):
  source=self.root/'Folder';source.mkdir();(source/'file').write_bytes(b'content')
  listener=socket.socket();listener.bind(('127.0.0.1',0));listener.listen();listener.settimeout(2);self.sockets.append(listener)
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',listener.getsockname()[1],os.fsencode(source)),1)
  self.until(lambda:self.status(self.a).active);peer,_=listener.accept();self.sockets.append(peer);peer.sendall(frame(1,hello()))
  self.until(lambda:self.lib.bs_queue_paused(self.a)!=0);self.assertIn(b'does not support folder',self.lib.bs_last_error(self.a))
  peer.settimeout(1);data=peer.recv(4096);self.assertNotIn(b'Folder/',data)
 def test_cancel_listing_leaves_source_and_sends_nothing(self):
  self.lib.bs_queue_scanning.argtypes=[C.c_void_p]
  source=self.root/'Many';source.mkdir()
  for i in range(50):(source/str(i)).write_bytes(b'original')
  self.queue(source);self.until(lambda:self.lib.bs_queue_scanning(self.a)!=0);self.lib.bs_cancel(self.a)
  self.assertEqual(self.lib.bs_queue_scanning(self.a),0);self.assertEqual(self.lib.bs_queue_count(self.a),0);self.assertFalse(self.status(self.b).awaiting_decision);self.assertEqual(len(list(source.iterdir())),50)
 def test_invalid_wire_tree_is_rejected_before_approval(self):
  import struct
  for bad in [b'Folder/../escape',b'other/file',b'Folder/sub/file',b'/absolute']:
   s=self.raw_peer();g=hello();g=g[:19]+bytes([9])+g[20:]+b'|BX5c'
   caps=b'BXC1'+bytes([1,0,0,0])+struct.pack('>IIIIHHHHI',92,92,2147483647,2147483647,255,255,1023,0,32760)
   def entry(name):return struct.pack('>IH',0,len(name))+name
   s.sendall(frame(1,g)+frame(21,caps)+frame(27,b'x'*16+struct.pack('>II',0,2))+frame(28,entry(b'Folder/'))+frame(28,entry(bad))+frame(18))
   self.until(lambda:not self.status(self.b).active and b'Invalid collection' in self.status(self.b).message)
   self.assertFalse(self.status(self.b).awaiting_decision)
def load_tests(loader,tests,pattern):return unittest.TestSuite(Folders(n) for n in Folders.__dict__ if n.startswith('test_'))
