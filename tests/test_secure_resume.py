"""Modern extension interoperability, authenticated TLS and owned checkpoints."""
import ctypes as C
import os, subprocess, shlex, shutil
from test_core import CoreTests as _CoreTests, ROOT, frame, hello
import unittest, socket, struct, time
from pathlib import Path

class ModernTests(unittest.TestCase):
 setUp = _CoreTests.setUp
 tearDown = _CoreTests.tearDown
 client = _CoreTests.client
 status = _CoreTests.status
 until = _CoreTests.until
 start = _CoreTests.start
 transfer = _CoreTests.transfer
 raw_peer = _CoreTests.raw_peer
 @classmethod
 def setUpClass(cls):
  _CoreTests.setUpClass.__func__(cls)
  target=ROOT/'build/libmodern.so'
  # Prefer the production pin; an explicit prefix makes alternate-version CI reproducible.
  override=os.environ.get('BITSKIM_OPENSSL_PREFIX')
  prefix=Path(override) if override else ROOT/'work/openssl-macos'
  if (prefix/'include/openssl/ssl.h').is_file():
   flags=['-I'+str(prefix/'include'),'-L'+str(prefix/'lib'),'-lssl','-lcrypto']
  elif override:
   raise RuntimeError('BITSKIM_OPENSSL_PREFIX must contain include/openssl/ssl.h and lib/')
  elif shutil.which('pkg-config') and subprocess.run(['pkg-config','--atleast-version=3.0.0','openssl'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0:
   flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','openssl'],text=True))
  else:
   raise unittest.SkipTest('OpenSSL 3+ development files required: run scripts/build-openssl.sh macos or set BITSKIM_OPENSSL_PREFIX')
  subprocess.run(['cc','-shared','-fPIC','-std=c89','-Wall','-Wextra','-Werror','-DBS_OPENSSL','tests/core_test_hooks.c','core/platform.c',*flags,'-o',str(target)],cwd=ROOT,check=True)
  # Reuse the fixture's signatures on a separately loaded modern engine.
  old=cls.lib;cls.lib=C.CDLL(str(target));cls.lib.bs_platform_init()
  for name in dir(old):
   if name.startswith('bs_'):
    a=getattr(old,name);b=getattr(cls.lib,name);b.argtypes=a.argtypes;b.restype=a.restype
  for name,args in {'bs_security_configure':[C.c_void_p,C.c_int,C.c_char_p],'bs_resume_enable':[C.c_void_p,C.c_int],'bs_accept_resumable':[C.c_void_p,C.c_char_p,C.c_int],'bs_resume_available':[C.c_void_p,C.c_char_p],'bs_transfer_encrypted':[C.c_void_p]}.items():getattr(cls.lib,name).argtypes=args
 def secure(self,key=b'12'*32):
  for c in (self.a,self.b):self.assertEqual(self.lib.bs_security_configure(c,2,key),1)
 def test_default_prefer_automatically_encrypts_manual_connection(self):
  self.transfer(b'automatic encryption without a pairing key')
  self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
  self.assertEqual(self.lib.bs_transfer_encrypted(self.b),1)
 def test_prefer_and_force_without_pairing(self):
  self.assertEqual(self.lib.bs_security_configure(self.b,2,b''),1)
  self.transfer(b'prefer sender to force receiver')
  self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
 def test_force_to_prefer_without_pairing(self):
  self.assertEqual(self.lib.bs_security_configure(self.a,2,b''),1)
  self.transfer(b'force sender to prefer receiver');self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
 def test_disabled_sender_remains_plaintext_to_prefer(self):
  self.assertEqual(self.lib.bs_security_configure(self.a,0,b''),1)
  self.transfer(b'explicit plaintext');self.assertEqual(self.lib.bs_transfer_encrypted(self.a),0)
 def test_prefer_sender_uses_plaintext_to_disabled_receiver(self):
  self.assertEqual(self.lib.bs_security_configure(self.b,0,b''),1)
  self.transfer(b'receiver disabled encryption');self.assertEqual(self.lib.bs_transfer_encrypted(self.a),0)
 def test_force_sender_does_not_fall_back_to_disabled_receiver(self):
  self.lib.bs_security_configure(self.a,2,b'');self.lib.bs_security_configure(self.b,0,b'')
  p=self.root/'secret';p.write_bytes(b'no plaintext fallback')
  self.assertTrue(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(p)))
  self.until(lambda:not self.status(self.a).active)
  self.assertFalse(self.status(self.b).awaiting_decision);self.assertNotEqual(self.lib.bs_transfer_result(self.a),1)
 def test_force_rejects_plaintext_resume_offer_before_approval(self):
  self.lib.bs_security_configure(self.b,2,b'');self.lib.bs_resume_enable(self.b,1)
  s=self.raw_peer();greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,9)+b'TestTest|BX03'
  from test_core import frame
  s.sendall(frame(1,greeting)+frame(10,struct.pack('>IIH',0,1,1)+b'x'))
  self.until(lambda:b'requires encryption' in self.status(self.b).message)
  self.assertFalse(self.status(self.b).awaiting_decision)
 def test_tls_support_and_disabled_policy_are_reported_separately(self):
  from test_capabilities import Caps
  self.lib.bs_local_capabilities.argtypes=[C.c_void_p,C.POINTER(Caps)]
  self.lib.bs_security_configure(self.a,0,b'');caps=Caps()
  self.lib.bs_local_capabilities(self.a,C.byref(caps))
  self.assertTrue(caps.send_features&2);self.assertTrue(caps.receive_features&2)
  self.assertEqual(caps.security_mode,0);self.assertFalse(caps.receive_features&32)
 def test_required_tls(self):
  self.secure();self.transfer(os.urandom(100000));self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
 def test_batch_single_consent_with_required_tls(self):
  self.secure();l=self.lib
  l.bs_batch_enable.argtypes=[C.c_void_p,C.c_int];l.bs_accept_batch.argtypes=[C.c_void_p,C.c_char_p]
  l.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p];l.bs_queue_count.argtypes=[C.c_void_p]
  l.bs_batch_enable(self.b,1)
  for c in (self.a,self.b):l.bs_resume_enable(c,1)
  data=[os.urandom(70000),os.urandom(90000)]
  for i,content in enumerate(data):
   p=self.root/('secure%d'%i);p.write_bytes(content);self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
  folder=self.root/'receive';folder.mkdir();self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(folder)),1)
  def done():
   self.assertFalse(self.status(self.b).awaiting_decision)
   return not l.bs_queue_count(self.a) and not self.status(self.b).active
  self.until(done)
  for i,content in enumerate(data):self.assertEqual((folder/('secure%d'%i)).read_bytes(),content)
  self.assertEqual(l.bs_transfer_encrypted(self.b),1)
 def test_mismatched_key_fails_closed(self):
  self.secure();self.lib.bs_security_configure(self.b,2,b'34'*32)
  p=self.root/'secret';p.write_bytes(b'secret')
  self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:not self.status(self.a).active)
  self.assertFalse(self.status(self.b).awaiting_decision)
  self.assertNotEqual(self.lib.bs_transfer_result(self.a),1)
 def resumable_start(self,secure=False):
  if secure:self.secure()
  for c in (self.a,self.b):self.lib.bs_resume_enable(c,1)
  data=os.urandom(2<<20);self.start(data);dest=self.root/'received'
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  self.until(lambda:self.status(self.b).transferred>65536)
  self.lib.bs_destroy(self.a);self.clients.remove(self.a)
  self.until(lambda:not self.status(self.b).active)
  self.assertTrue((self.root/'received.bsk-part').exists())
  self.a,self.pa=self.client(b'a'*32);self.lib.bs_resume_enable(self.a,1)
  if secure:self.lib.bs_security_configure(self.a,2,b'12'*32)
  self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(self.root/'test.bin')),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(self.lib.bs_resume_available(self.b,os.fsencode(dest)),1)
  return data,dest
 def test_resume_after_process_loss(self):
  data,dest=self.resumable_start()
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),data)
  self.assertFalse((self.root/'received.bsk-resume').exists())
 def test_resume_space_check_counts_only_remaining_payload(self):
  data,dest=self.resumable_start();part=self.root/'received.bsk-part';retained=part.stat().st_size
  self.lib.bs_test_space.argtypes=[C.c_int,C.c_ulong,C.c_ulong,C.c_ulong]
  self.lib.bs_test_space(1,0,0,4096)
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),0)
  self.assertEqual(part.stat().st_size,retained);self.assertTrue(self.status(self.b).awaiting_decision)
  self.lib.bs_test_space(1,0,len(data)-retained+100000,4096)
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),data)
 def test_corrupt_prefix_refused(self):
  _,dest=self.resumable_start();part=self.root/'received.bsk-part'
  with part.open('r+b') as f:f.write(b'corrupt')
  self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertFalse(dest.exists());self.assertTrue(part.exists())

 def test_required_rejects_plaintext(self):
  self.lib.bs_security_configure(self.a,0,b'')
  self.lib.bs_security_configure(self.b,2,b'12'*32)
  p=self.root/'secret';p.write_bytes(b'do not offer')
  self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(p))
  self.until(lambda:not self.status(self.a).active)
  self.assertFalse(self.status(self.b).awaiting_decision)
 def test_optional_accepts_legacy_plaintext(self):
  self.lib.bs_security_configure(self.a,0,b'')
  self.lib.bs_security_configure(self.b,1,b'12'*32)
  self.transfer(b'legacy compatible');self.assertEqual(self.lib.bs_transfer_encrypted(self.b),0)
 def test_optional_uses_advertised_tls(self):
  self.secure()
  self.lib.bs_security_configure(self.a,1,b'12'*32)
  packet=bytes.fromhex('b'*32)+struct.pack('>HBB',self.pb,4,9)+b'TestTest|BX02'
  udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);self.sockets.append(udp)
  udp.sendto((b'BSKD\x01\x02'+packet).ljust(160,b'\0'),('127.0.0.1',self.pa))
  self.until(lambda:self.lib.bs_peer_count(self.a)>0)
  self.transfer(b'encrypted opportunistic');self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
 def test_resume_restart_discards_only_matching_cache(self):
  data,dest=self.resumable_start()
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),1),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),data)
 def test_resume_metadata_mismatch_preserves_cache(self):
  _,dest=self.resumable_start();meta=self.root/'received.bsk-resume'
  original=meta.read_bytes();meta.write_bytes(b'bad'+original[3:])
  self.assertEqual(self.lib.bs_resume_available(self.b,os.fsencode(dest)),-1)
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),1),0)
  self.assertEqual(meta.read_bytes(),b'bad'+original[3:])
 def test_resume_symlink_preserved(self):
  _,dest=self.resumable_start();part=self.root/'received.bsk-part';outside=self.root/'outside';outside.write_bytes(part.read_bytes());part.unlink();part.symlink_to(outside)
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),0)
  self.assertTrue(part.is_symlink())
 def test_cancel_removes_current_checkpoint(self):
  for c in (self.a,self.b):self.lib.bs_resume_enable(c,1)
  self.start(os.urandom(2<<20));dest=self.root/'received'
  self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0)
  self.until(lambda:self.status(self.b).transferred>65536)
  self.lib.bs_cancel(self.b)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertFalse((self.root/'received.bsk-part').exists())
 def test_zero_length_resumable(self):
  for c in (self.a,self.b):self.lib.bs_resume_enable(c,1)
  self.secure();self.start(b'');dest=self.root/'empty'
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),b'')


 def test_resume_and_tls_together(self):
  data,dest=self.resumable_start(secure=True)
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
  self.assertEqual(dest.read_bytes(),data);self.assertEqual(self.lib.bs_transfer_encrypted(self.a),1)
 def test_modern_resume_capability_falls_back_for_base_peer(self):
  self.lib.bs_resume_enable(self.a,1);self.transfer(b'base sequence')
 def test_security_settings_cannot_downgrade_queued_files(self):
  self.secure();p=self.root/'secret';p.write_bytes(b'private')
  self.lib.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
  self.lib.bs_security_mode.argtypes=[C.c_void_p]
  self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(p)),1)
  self.assertEqual(self.lib.bs_security_configure(self.a,0,b''),0)
  self.assertEqual(self.lib.bs_security_mode(self.a),2)

 def test_verification_heartbeat_transition_is_ordered(self):
  for c in (self.a,self.b):self.lib.bs_resume_enable(c,1)
  self.start(b'x'*32768);dest=self.root/'timed'
  self.assertEqual(self.lib.bs_accept_resumable(self.b,os.fsencode(dest),0),1)
  for _ in range(100):
   self.lib.bs_test_advance(1)
   for c in self.clients:self.lib.bs_tick(c)
   time.sleep(.001)
   if not self.status(self.a).active and not self.status(self.b).active:break
  self.assertEqual(self.lib.bs_transfer_result(self.a),1,self.status(self.a).message)
  self.assertEqual(dest.read_bytes(),b'x'*32768)

def load_tests(loader, tests, pattern):
 return loader.loadTestsFromTestCase(ModernTests)
