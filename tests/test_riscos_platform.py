"""Exercise the actual RISC OS C adapter policies on a host; not an OS test system."""
import ctypes as C,os,socket,subprocess,tempfile,time,unittest
from pathlib import Path
from test_core import Config,Status
ROOT=Path(__file__).resolve().parents[1]
class RISCOSPlatform(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.libs=[]
        for target,flags in [('riscos',['-D__riscos__']),('peer',[])]:
            library=ROOT/'build'/('lib-riscos-check-'+target+'.so')
            subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-std=c89']+flags+(['tests/riscos_file_host.c'] if target=='riscos' else [])+['core/bitskim.c','core/platform.c','-o',str(library)],cwd=ROOT,check=True)
            l=C.CDLL(str(library));l.bs_platform_init();cls.libs.append(l)
            l.bs_create.argtypes=[C.POINTER(Config),C.c_void_p,C.c_void_p];l.bs_create.restype=C.c_void_p
            for n in ('bs_tick','bs_destroy'):getattr(l,n).argtypes=[C.c_void_p]
            l.bs_status.argtypes=[C.c_void_p];l.bs_status.restype=C.POINTER(Status)
            l.bs_send.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p];l.bs_send.restype=C.c_int
            l.bs_accept.argtypes=[C.c_void_p,C.c_char_p];l.bs_accept.restype=C.c_int
            for n in ('bs_to_utf8','bs_to_local'):getattr(l,n).argtypes=[C.c_char_p,C.c_char_p,C.c_size_t]
            l.bsp_commit.argtypes=[C.c_char_p,C.c_char_p]
    def test_latin1_conversion_is_explicit(self):
        l=self.libs[0];out=C.create_string_buffer(64)
        self.assertEqual(l.bs_to_utf8(b'caf\xe9',out,64),1);self.assertEqual(out.value,'café'.encode())
        self.assertEqual(l.bs_to_local('café'.encode(),out,64),1);self.assertEqual(out.value,b'caf\xe9')
        self.assertEqual(l.bs_to_local('λ'.encode(),out,64),0)
    def test_receive_from_existing_engine_uses_short_temp_leaf(self):
        clients=[]
        with tempfile.TemporaryDirectory() as d:
            folder=Path(d);source=folder/'source.bin';data=os.urandom(32791);source.write_bytes(data);dest=folder/'received'
            try:
                for i,l in enumerate(self.libs):
                    for attempt in range(20):
                        s=socket.socket();s.bind(('127.0.0.1',0));port=s.getsockname()[1];s.close()
                        cfg=Config(b'Peer',b'Test',b'a'*32 if i==0 else b'b'*32,b'127.0.0.1',port,1,1,port)
                        c=l.bs_create(C.byref(cfg),None,None)
                        if c:clients.append((l,c,port));break
                    self.assertEqual(len(clients),i+1)
                receiver,rc,port=clients[0];sender,sc,_=clients[1]
                self.assertEqual(sender.bs_send(sc,b'127.0.0.1',port,os.fsencode(source)),1)
                def pump(predicate):
                    until=time.monotonic()+5
                    while time.monotonic()<until:
                        for l,c,_ in clients:l.bs_tick(c)
                        if predicate():return
                        time.sleep(.001)
                    self.fail('Loopback transfer did not reach expected state')
                pump(lambda:receiver.bs_status(rc).contents.awaiting_decision)
                self.assertEqual(receiver.bs_accept(rc,os.fsencode(dest)),1)
                temporary=list(folder.glob('BS*'));self.assertEqual(len(temporary),1);self.assertEqual(len(temporary[0].name),10)
                pump(lambda:not receiver.bs_status(rc).contents.active and not sender.bs_status(sc).contents.active)
                self.assertEqual(dest.read_bytes(),data);self.assertFalse(temporary[0].exists())
                self.assertIn(b'delivered successfully',sender.bs_status(sc).contents.message)
            finally:
                for l,c,_ in clients:l.bs_destroy(c)
    def test_existing_destination_is_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            a=Path(d)/'temp';b=Path(d)/'existing';a.write_bytes(b'new');b.write_bytes(b'old')
            self.assertLess(self.libs[0].bsp_commit(os.fsencode(a),os.fsencode(b)),0)
            self.assertEqual(b.read_bytes(),b'old');self.assertTrue(a.exists())
