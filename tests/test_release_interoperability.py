"""Real socket transfers between the tagged 0.1 engine and current source."""
import ctypes as C
import io,os,socket,subprocess,tarfile,tempfile,time,unittest
from pathlib import Path
from test_core import Config,Status
ROOT=Path(__file__).resolve().parents[1]
class ReleaseInterop(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.build=tempfile.TemporaryDirectory();base=Path(cls.build.name);old=base/'old';old.mkdir()
  ref=os.environ.get('BITSKIM_BASELINE_REF','0.1')
  result=subprocess.run(['git','archive',ref,'core','tests/core_test_hooks.c'],cwd=ROOT,capture_output=True)
  if result.returncode:cls.build.cleanup();raise unittest.SkipTest('Tagged 0.1 checkout required for cross-release tests')
  with tarfile.open(fileobj=io.BytesIO(result.stdout)) as archive:
   for member in archive.getmembers():
    path=Path(member.name)
    if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir()):raise RuntimeError('Unsafe baseline archive entry')
    dest=old/path
    if member.isdir():dest.mkdir(parents=True,exist_ok=True)
    else:dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(archive.extractfile(member).read())
  prefix=Path(os.environ.get('BITSKIM_OPENSSL_PREFIX',str(ROOT/'work/openssl-macos')))
  if not (prefix/'include/openssl/ssl.h').is_file():cls.build.cleanup();raise unittest.SkipTest('OpenSSL development files required')
  cls.engines=[]
  for version,source in [('0.1',old),('0.2',ROOT)]:
   target=base/('engine-'+version+'.so')
   subprocess.run(['cc','-shared','-fPIC','-std=c89','-Wall','-Wextra','-Werror','-DBS_OPENSSL',str(source/'tests/core_test_hooks.c'),str(source/'core/platform.c'),'-I'+str(prefix/'include'),'-L'+str(prefix/'lib'),'-lssl','-lcrypto','-o',str(target)],check=True)
   lib=C.CDLL(str(target));lib.bs_platform_init()
   signatures={'bs_create':([C.POINTER(Config),C.c_void_p,C.c_void_p],C.c_void_p),'bs_destroy':([C.c_void_p],None),'bs_tick':([C.c_void_p],None),'bs_status':([C.c_void_p],C.POINTER(Status)),'bs_send':([C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],C.c_int),'bs_accept':([C.c_void_p,C.c_char_p],C.c_int),'bs_cancel':([C.c_void_p],None),'bs_security_configure':([C.c_void_p,C.c_int,C.c_char_p],C.c_int),'bs_transfer_encrypted':([C.c_void_p],C.c_int),'bs_transfer_result':([C.c_void_p],C.c_int),'bs_batch_enable':([C.c_void_p,C.c_int],C.c_int),'bs_queue_address':([C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],C.c_int),'bs_queue_count':([C.c_void_p],C.c_int),'bs_accept_batch':([C.c_void_p,C.c_char_p],C.c_int)}
   for name,(args,result_type) in signatures.items():f=getattr(lib,name);f.argtypes=args;f.restype=result_type
   cls.engines.append(lib)
 @classmethod
 def tearDownClass(cls):
  if hasattr(cls,'build'):cls.build.cleanup()
 def setUp(self):self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.clients=[]
 def tearDown(self):
  for lib,c,port in self.clients:lib.bs_destroy(c)
  self.temp.cleanup()
 def endpoint(self,lib,identity):
  for _ in range(30):
   with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
   cfg=Config(b'Interop',b'Test',identity,b'127.0.0.1',port,0,1,port)
   client=lib.bs_create(C.byref(cfg),None,None)
   if client:self.clients.append((lib,client,port));return self.clients[-1]
  self.fail('Cannot create endpoint')
 def wait(self,predicate):
  end=time.monotonic()+20
  while time.monotonic()<end:
   for lib,c,_ in self.clients:lib.bs_tick(c)
   if predicate():return
   time.sleep(.0005)
  self.fail('Transfer timed out: '+repr([lib.bs_status(c).contents.message for lib,c,_ in self.clients]))
 def pair(self,reverse=False,mode=0):
  a=self.endpoint(self.engines[int(reverse)],b'a'*32);b=self.endpoint(self.engines[int(not reverse)],b'b'*32)
  for lib,c,_ in (a,b):self.assertEqual(lib.bs_security_configure(c,mode,b'12'*32 if mode==2 else b''),1)
  return a,b
 def send(self,a,b,data,name):
  la,ca,_=a;lb,cb,pb=b;source=self.root/(name+'.source');dest=self.root/name;source.write_bytes(data)
  self.assertEqual(la.bs_send(ca,b'127.0.0.1',pb,os.fsencode(source)),1)
  self.wait(lambda:lb.bs_status(cb).contents.awaiting_decision)
  self.assertEqual(lb.bs_accept(cb,os.fsencode(dest)),1)
  self.wait(lambda:not la.bs_status(ca).contents.active and not lb.bs_status(cb).contents.active)
  self.assertEqual(la.bs_transfer_result(ca),1);self.assertEqual(lb.bs_transfer_result(cb),1);self.assertEqual(dest.read_bytes(),data)
  self.assertEqual(la.bs_transfer_encrypted(ca),int(bool(self.mode)));self.assertEqual(list(self.root.glob('*.part')),[])
 def test_plaintext_and_paired_tls_both_directions(self):
  for reverse in (False,True):
   for mode in (0,2):
    with self.subTest(reverse=reverse,security=mode):
     self.mode=mode;a,b=self.pair(reverse,mode)
     self.send(a,b,b'',f'empty-{reverse}-{mode}')
     self.send(a,b,bytes(range(256))*131072,f'binary-{reverse}-{mode}')
 def test_automatic_tls_both_directions(self):
  for reverse in (False,True):
   with self.subTest(reverse=reverse):
    self.mode=1;a,b=self.pair(reverse,1);self.send(a,b,b'automatic TLS',f'auto-{reverse}')
 def test_folders_with_empty_files_and_directories_both_directions(self):
  for reverse in (False,True):
   with self.subTest(reverse=reverse):
    a,b=self.pair(reverse,2);la,ca,_=a;lb,cb,pb=b;lb.bs_batch_enable(cb,1)
    source=self.root/('tree-'+str(reverse));(source/'sub'/'empty').mkdir(parents=True);(source/'sub'/'zero').touch();(source/'binary').write_bytes(bytes(range(256))*400)
    dest=self.root/('received-'+str(reverse));dest.mkdir()
    self.assertEqual(la.bs_queue_address(ca,b'127.0.0.1',pb,os.fsencode(source)),1);self.wait(lambda:lb.bs_status(cb).contents.awaiting_decision)
    self.assertEqual(lb.bs_accept_batch(cb,os.fsencode(dest)),1);self.wait(lambda:not la.bs_queue_count(ca) and not lb.bs_status(cb).contents.active)
    self.assertEqual((dest/source.name/'binary').read_bytes(),(source/'binary').read_bytes());self.assertEqual((dest/source.name/'sub'/'zero').stat().st_size,0);self.assertTrue((dest/source.name/'sub'/'empty').is_dir())
 def test_cancellation_from_either_end_both_directions(self):
  for reverse in (False,True):
   for cancel_receiver in (False,True):
    with self.subTest(reverse=reverse,cancel_receiver=cancel_receiver):
     a,b=self.pair(reverse,2);la,ca,_=a;lb,cb,pb=b;source=self.root/'large';source.write_bytes(b'x'*(4<<20));dest=self.root/'cancelled'
     self.assertEqual(la.bs_send(ca,b'127.0.0.1',pb,os.fsencode(source)),1);self.wait(lambda:lb.bs_status(cb).contents.awaiting_decision);self.assertEqual(lb.bs_accept(cb,os.fsencode(dest)),1)
     self.wait(lambda:lb.bs_status(cb).contents.transferred>0)
     (lb if cancel_receiver else la).bs_cancel(cb if cancel_receiver else ca)
     self.wait(lambda:not la.bs_status(ca).contents.active and not lb.bs_status(cb).contents.active)
     self.assertNotEqual(la.bs_transfer_result(ca),1);self.assertFalse(dest.exists());self.assertEqual(list(self.root.glob('*.part')),[])
