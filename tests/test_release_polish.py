"""Real checkpoint cleanup boundaries and recovery progress, with production APIs."""
import ctypes as C
import fcntl,os,struct,time,unittest
import test_secure_resume as modern
from test_core import Status
class RetainedItem(C.Structure):
 _fields_=[('path',C.c_char*1024),('bytes',C.c_ulong),('total',C.c_ulong),('removed',C.c_int)]
class Progress(C.Structure):
 _fields_=[('phase',C.c_int),*[(n,C.c_ulong) for n in ('verified','resumed','received','remaining','total','checked_bytes','check_total')]]
class ReleasePolish(unittest.TestCase):
 setUpClass=classmethod(modern.ModernTests.setUpClass.__func__)
 setUp=modern.ModernTests.setUp;tearDown=modern.ModernTests.tearDown;client=modern.ModernTests.client;status=modern.ModernTests.status;until=modern.ModernTests.until;start=modern.ModernTests.start
 def setUp(self):
  modern.ModernTests.setUp(self);l=self.lib
  signatures={'bs_retained_scan':([C.c_void_p,C.c_char_p],C.c_ulong),'bs_retained_step':([C.c_void_p],C.c_int),'bs_retained_count':([C.c_void_p],C.c_int),'bs_retained_phase':([C.c_void_p],C.c_int),'bs_retained_close':([C.c_void_p],None),'bs_retained_remove':([C.c_void_p,C.c_ulong],C.c_int),'bs_retained_item':([C.c_void_p,C.c_int,C.POINTER(RetainedItem)],C.c_int),'bs_retained_summary':([C.c_void_p,C.c_char_p,C.c_size_t],C.c_int),'bs_recovery_progress':([C.c_void_p,C.POINTER(Progress)],C.c_int),'bs_recovery_summary':([C.c_void_p,C.c_char_p,C.c_size_t],C.c_int),'bs_batch_enable':([C.c_void_p,C.c_int],None),'bs_accept_batch_resume':([C.c_void_p,C.c_char_p],C.c_int),'bs_accept_batch':([C.c_void_p,C.c_char_p],C.c_int),'bs_queue_address':([C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],C.c_int),'bs_limits_configure':([C.c_void_p,C.c_ulong,C.c_ulong],C.c_int)}
  for name,(args,result) in signatures.items():getattr(l,name).argtypes=args;getattr(l,name).restype=result
  l.bs_test_checkpoint_mark.argtypes=[C.c_char_p]
  self.receive=self.root/'received';self.receive.mkdir()
 def checkpoint(self,name,content=b'partial',wire=b'file.bin',owned=True):
  path=self.receive/name;path.parent.mkdir(parents=True,exist_ok=True);part=path.with_name(path.name+'.bsk-part');meta=path.with_name(path.name+'.bsk-resume');part.write_bytes(content)
  if owned:self.assertEqual(self.lib.bs_test_checkpoint_mark(os.fsencode(part)),1)
  meta.write_bytes(b'BSRSM01\0'+b'a'*32+struct.pack('>IH',100,len(wire))+wire);return part,meta
 def scan(self):
  serial=self.lib.bs_retained_scan(self.b,os.fsencode(self.receive));self.assertTrue(serial)
  for _ in range(1000):
   if self.lib.bs_retained_step(self.b)!=1:break
  self.assertEqual(self.lib.bs_retained_phase(self.b),2);return serial
 def clean(self,serial):
  self.assertEqual(self.lib.bs_retained_remove(self.b,serial),1)
  for _ in range(200):
   if self.lib.bs_retained_step(self.b)!=3:break
  self.assertEqual(self.lib.bs_retained_phase(self.b),4)
 def test_only_reviewed_owned_pairs_removed_final_and_unknown_files_preserved(self):
  a=self.checkpoint('root/a');b=self.checkpoint('b');final=self.receive/'root/a';final.write_bytes(b'completed stays');orphan=self.receive/'orphan.bsk-part';orphan.write_bytes(b'not a checkpoint');invalid=self.checkpoint('invalid');invalid[1].write_bytes(b'not BitSkim');serial=self.scan();self.assertEqual(self.lib.bs_retained_count(self.b),2)
  report=C.create_string_buffer(1000);self.lib.bs_retained_summary(self.b,report,len(report));self.assertIn(b'bytes',report.value);self.clean(serial)
  for file in (*a,*b):self.assertFalse(file.exists())
  self.assertEqual(final.read_bytes(),b'completed stays');self.assertEqual(orphan.read_bytes(),b'not a checkpoint');self.assertTrue(all(p.exists() for p in invalid))
 def test_change_after_review_preserves_pair(self):
  p,m=self.checkpoint('changed');serial=self.scan();stamp=p.stat();p.write_bytes(b'changed');os.utime(p,ns=(stamp.st_atime_ns,stamp.st_mtime_ns));self.clean(serial);self.assertTrue(p.exists());self.assertTrue(m.exists());item=RetainedItem();self.assertEqual(self.lib.bs_retained_item(self.b,0,C.byref(item)),1);self.assertEqual(item.removed,-1)
 def test_links_and_swapped_parent_cannot_delete_outside_receive_root(self):
  p,m=self.checkpoint('nested/one');serial=self.scan();outside=self.root/'outside';(self.receive/'nested').rename(outside);(self.receive/'nested').symlink_to(outside,target_is_directory=True);self.clean(serial);self.assertTrue((outside/p.name).exists());self.assertTrue((outside/m.name).exists())
 def test_locked_partial_is_not_a_cleanup_candidate(self):
  p,m=self.checkpoint('locked')
  with p.open('rb') as lock:
   fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);self.scan();self.assertEqual(self.lib.bs_retained_count(self.b),0)
  self.assertTrue(p.exists());self.assertTrue(m.exists())
 def test_old_confirmation_and_close_never_delete(self):
  p,m=self.checkpoint('kept');old=self.scan();new=self.scan();self.assertNotEqual(old,new);self.assertFalse(self.lib.bs_retained_remove(self.b,old));self.lib.bs_retained_close(self.b);self.assertFalse(self.lib.bs_retained_remove(self.b,new));self.assertTrue(p.exists());self.assertTrue(m.exists())
 def test_busy_engine_rejects_confirmed_cleanup(self):
  p,m=self.checkpoint('busy');serial=self.scan();self.start(b'new transfer');self.assertFalse(self.lib.bs_retained_remove(self.b,serial));self.assertTrue(p.exists());self.assertTrue(m.exists())
 def test_bounded_review_only_removes_listed_files(self):
  for i in range(130):self.checkpoint('part%d'%i)
  serial=self.scan();self.assertEqual(self.lib.bs_retained_count(self.b),128);self.clean(serial);self.assertEqual(len(list(self.receive.glob('*.bsk-part'))),2)
 def test_every_existing_folder_file_advances_once_and_reports_verified_count(self):
  l=self.lib
  for c in (self.a,self.b):l.bs_batch_enable(c,1);l.bs_resume_enable(c,1);l.bs_security_configure(c,2,b'12'*32)
  source=self.root/'Tree';source.mkdir();target=self.receive/'Tree';target.mkdir()
  for i in range(3):
   data=('existing %d'%i).encode();(source/str(i)).write_bytes(data);(target/str(i)).write_bytes(data)
  self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1);self.until(lambda:self.status(self.b).awaiting_decision);self.assertEqual(l.bs_accept_batch_resume(self.b,os.fsencode(self.receive)),1)
  phases=set()
  def done():
   p=Progress();l.bs_recovery_progress(self.b,C.byref(p));phases.add(p.phase);self.assertFalse(self.status(self.b).awaiting_decision);return l.bs_queue_count(self.a)==0
  self.until(done);p=Progress();self.assertTrue(l.bs_recovery_progress(self.b,C.byref(p)));self.assertEqual((p.phase,p.verified,p.resumed,p.received,p.remaining,p.total),(4,3,0,0,0,3));self.assertIn(2,phases)

 def test_resume_counts_completed_partial_and_missing_files_separately(self):
  l=self.lib
  for c in (self.a,self.b):l.bs_batch_enable(c,1);l.bs_resume_enable(c,1);l.bs_security_configure(c,2,b'12'*32)
  source=self.root/'Mixed';source.mkdir();target=self.receive/'Mixed';target.mkdir()
  (source/'existing').write_bytes(b'known');(target/'existing').write_bytes(b'known');data=b'x'*70000;(source/'partial').write_bytes(data);(source/'missing').write_bytes(b'new payload'*1000)
  part=target/'partial.bsk-part';part.write_bytes(data[:30000]);wire=b'Mixed/partial';(target/'partial.bsk-resume').write_bytes(b'BSRSM01\0'+b'a'*32+struct.pack('>IH',len(data),len(wire))+wire)
  self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1);self.until(lambda:self.status(self.b).awaiting_decision);self.assertEqual(l.bs_accept_batch_resume(self.b,os.fsencode(self.receive)),1);self.until(lambda:l.bs_queue_count(self.a)==0)
  p=Progress();self.assertTrue(l.bs_recovery_progress(self.b,C.byref(p)));self.assertEqual((p.phase,p.verified,p.resumed,p.received,p.remaining,p.total),(4,1,1,1,0,3));self.assertEqual((target/'partial').read_bytes(),data)

 def test_valid_looking_completed_pair_without_local_marker_is_preserved(self):
  p,m=self.checkpoint('completed-looking',owned=False);self.scan();self.assertEqual(self.lib.bs_retained_count(self.b),0);self.assertTrue(p.exists());self.assertTrue(m.exists())
