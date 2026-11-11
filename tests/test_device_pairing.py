"""Separate processes verify persistent TLS identities, code agreement and pin refusal."""
from pathlib import Path
import subprocess,tempfile,unittest,os,hashlib
ROOT=Path(__file__).resolve().parents[1]
WORKER=r'''
import sys,ctypes as C,os
sys.path.insert(0,'tests')
from test_secure_resume import ModernTests
ModernTests.setUpClass()
t=ModernTests('test_required_tls');t.setUp();l=t.lib
for name,args in {'bs_trust_configure':[C.c_void_p,C.c_char_p],'bs_pairing_code':[C.c_void_p,C.c_char_p],'bs_trust_current':[C.c_void_p,C.c_char_p],'bs_trust_status':[C.c_void_p],'bs_forget_current':[C.c_void_p]}.items():getattr(l,name).argtypes=args
try:
 folder=os.fsencode(sys.argv[1]); mode=sys.argv[2]
 assert l.bs_trust_configure(t.a,folder)==1 and l.bs_trust_configure(t.b,folder)==1
 source=t.root/'source';source.write_bytes(b'pairing check')
 if mode=='plaintext':
  l.bs_security_configure(t.a,0,b'');l.bs_security_configure(t.b,0,b'')
 assert l.bs_send(t.a,b'127.0.0.1',t.pb,os.fsencode(source))
 if mode in ('reject','plaintext','badidentity'):
  t.until(lambda:not t.status(t.a).active)
  assert not t.status(t.b).awaiting_decision
  if mode!='badidentity':assert b'identity changed' in t.status(t.a).message or b'identity changed' in t.status(t.b).message
 else:
  t.until(lambda:t.status(t.b).awaiting_decision)
  a=C.create_string_buffer(20);b=C.create_string_buffer(20)
  assert l.bs_pairing_code(t.a,a)==1 and l.bs_pairing_code(t.b,b)==1
  assert a.value==b.value and len(a.value)==14
  assert l.bs_trust_current(t.a,b'0000-0000-0000')==0
  if mode=='pair':
   assert l.bs_trust_status(t.a)==0 and l.bs_trust_status(t.b)==0
   assert l.bs_trust_current(t.a,b.value)==1 and l.bs_trust_current(t.b,a.value)==1
  assert l.bs_trust_status(t.a)==1 and l.bs_trust_status(t.b)==1
  l.bs_decline(t.b);t.until(lambda:not t.status(t.a).active and not t.status(t.b).active)
 print('PASS '+mode)
finally:t.tearDown()
'''
class DevicePairing(unittest.TestCase):
 def test_codes_persistence_restart_and_changed_pin(self):
  with tempfile.TemporaryDirectory() as directory:
   def run(mode):
    p=subprocess.run(['python3','-c',WORKER,directory,mode],cwd=ROOT,capture_output=True,text=True)
    self.assertEqual(p.returncode,0,p.stdout+p.stderr);self.assertIn('PASS '+mode,p.stdout)
   run('pair');identity=Path(directory)/'identity.pem';before=identity.read_bytes()
   self.assertEqual(os.stat(identity).st_mode&0o777,0o600)
   run('matched');self.assertEqual(identity.read_bytes(),before)
   run('plaintext')
   identity.unlink();run('reject');self.assertNotEqual(identity.read_bytes(),before)
   (Path(directory)/('b'*32+'.pin')).write_bytes(bytes(32));run('reject')
   identity.write_bytes(b'invalid identity'*2000);run('badidentity');self.assertEqual(identity.read_bytes(),b'invalid identity'*2000)
