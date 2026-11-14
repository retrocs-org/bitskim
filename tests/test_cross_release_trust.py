"""Persistent remembered-device trust between the actual 0.1 and 0.2 engines."""
from pathlib import Path
import hashlib,os,subprocess,sys,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
WORKER=r'''
import ctypes as C,os,sys
from pathlib import Path
sys.path.insert(0,'tests')
from test_release_interoperability import ReleaseInterop
ReleaseInterop.setUpClass()
t=ReleaseInterop('test_automatic_tls_both_directions');t.setUp()
try:
 stage=sys.argv[2];a,b=t.pair(False,1)
 for index,(lib,client,port) in enumerate((a,b)):
  for name,args in {'bs_trust_configure':[C.c_void_p,C.c_char_p],'bs_pairing_code':[C.c_void_p,C.c_char_p],'bs_trust_current':[C.c_void_p,C.c_char_p],'bs_trust_status':[C.c_void_p]}.items():getattr(lib,name).argtypes=args
  folder=Path(sys.argv[1])/('old' if index==0 else 'new')
  assert lib.bs_trust_configure(client,os.fsencode(folder))==1
  if stage=='plaintext':assert lib.bs_security_configure(client,0,b'')==1
 for reverse in (False,True):
  sender,receiver=(b,a) if reverse else (a,b)
  ls,cs,_=sender;lr,cr,port=receiver
  payload=bytes(range(251))*523;source=t.root/('source-'+str(reverse));dest=t.root/('received-'+str(reverse));source.write_bytes(payload)
  assert ls.bs_send(cs,b'127.0.0.1',port,os.fsencode(source))==1
  if stage in ('changed','plaintext'):
   t.wait(lambda:not ls.bs_status(cs).contents.active)
   assert not lr.bs_status(cr).contents.awaiting_decision
   assert ls.bs_transfer_result(cs)!=1
   assert b'identity changed' in ls.bs_status(cs).contents.message or b'identity changed' in lr.bs_status(cr).contents.message
   break
  t.wait(lambda:lr.bs_status(cr).contents.awaiting_decision)
  left=C.create_string_buffer(20);right=C.create_string_buffer(20)
  assert ls.bs_pairing_code(cs,left)==1 and lr.bs_pairing_code(cr,right)==1 and left.value==right.value
  if stage=='pair' and not reverse:
   assert ls.bs_trust_status(cs)==0 and lr.bs_trust_status(cr)==0
   assert ls.bs_trust_current(cs,b'0000-0000-0000')==0
   assert ls.bs_trust_current(cs,right.value)==1 and lr.bs_trust_current(cr,left.value)==1
  assert ls.bs_trust_status(cs)==1 and lr.bs_trust_status(cr)==1
  assert lr.bs_accept(cr,os.fsencode(dest))==1
  t.wait(lambda:not ls.bs_status(cs).contents.active and not lr.bs_status(cr).contents.active)
  assert dest.read_bytes()==payload and ls.bs_transfer_result(cs)==1 and lr.bs_transfer_result(cr)==1
 print('PASS '+stage)
finally:
 t.tearDown();ReleaseInterop.tearDownClass()
'''
class CrossReleaseTrust(unittest.TestCase):
 def test_pair_restart_both_directions_and_refuse_downgrade_or_changed_identity(self):
  if subprocess.run(['git','rev-parse','--verify','0.1^{commit}'],cwd=ROOT,capture_output=True).returncode:self.skipTest('Tagged 0.1 checkout required')
  prefix=Path(os.environ.get('BITSKIM_OPENSSL_PREFIX',str(ROOT/'work/openssl-macos')))
  if not (prefix/'include/openssl/ssl.h').is_file():self.skipTest('OpenSSL development files required')
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder)
   for name in ('old','new'):(root/name).mkdir(mode=0o700)
   def run(stage):
    result=subprocess.run([sys.executable,'-c',WORKER,str(root),stage],cwd=ROOT,capture_output=True,text=True,timeout=90)
    self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('PASS '+stage,result.stdout)
   run('pair')
   saved={name:hashlib.sha256((root/name/'identity.pem').read_bytes()).digest() for name in ('old','new')}
   pins={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.pin')};self.assertEqual(len(pins),2)
   run('restart')
   for name,digest in saved.items():self.assertEqual(hashlib.sha256((root/name/'identity.pem').read_bytes()).digest(),digest)
   self.assertEqual({str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*.pin')},pins)
   run('plaintext')
   (root/'new/identity.pem').unlink();run('changed')
