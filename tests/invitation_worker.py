"""Connection invitations pin the encrypted endpoint before file approval."""
import ctypes as C
import os,time,unittest
from test_secure_resume import ModernTests

class Invitation(C.Structure):
    _fields_=[('address',C.c_char*48),('id',C.c_char*33),('pin',C.c_ubyte*32),('token',C.c_ubyte*16),('port',C.c_ushort),('expires',C.c_ulong)]

class Invitations(unittest.TestCase):
    setUpClass=classmethod(ModernTests.setUpClass.__func__)
    client=ModernTests.client
    status=ModernTests.status
    until=ModernTests.until
    tearDown=ModernTests.tearDown
    def setUp(self):
        ModernTests.setUp(self)
        for name,args in {'bs_trust_configure':[C.c_void_p,C.c_char_p],'bs_invitation_create':[C.c_void_p,C.c_char_p,C.c_char_p,C.c_size_t], 'bs_invitation_parse':[C.c_char_p,C.POINTER(Invitation)], 'bs_queue_invitation':[C.c_void_p,C.c_char_p,C.c_char_p], 'bs_queue_paused':[C.c_void_p]}.items():getattr(self.lib,name).argtypes=args
        trust=self.root/'trust';trust.mkdir()
        self.assertEqual(self.lib.bs_trust_configure(self.a,os.fsencode(trust)),1);self.assertEqual(self.lib.bs_trust_configure(self.b,os.fsencode(trust)),1)
        self.source=self.root/'source';self.source.write_bytes(b'checked endpoint')
        out=C.create_string_buffer(256);self.assertEqual(self.lib.bs_invitation_create(self.b,b'127.0.0.1',out,256),1);self.uri=out.value
    def test_fresh_invitation_encrypted_send(self):
        self.assertEqual(self.lib.bs_queue_invitation(self.a,self.uri,os.fsencode(self.source)),1);self.until(lambda:self.status(self.b).awaiting_decision)
        self.assertTrue(self.lib.bs_transfer_encrypted(self.a));dest=self.root/'received';self.lib.bs_accept(self.b,os.fsencode(dest));self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(dest.read_bytes(),self.source.read_bytes())
    def test_wrong_certificate_rejected_before_offer(self):
        parts=self.uri.split(b'/');parts[-3]=b'00'*32;bad=b'/'.join(parts)
        self.assertEqual(self.lib.bs_queue_invitation(self.a,bad,os.fsencode(self.source)),1);self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertFalse(self.status(self.b).awaiting_decision);self.assertIn(b'identity',self.status(self.a).message)
    def test_wrong_device_id_rejected_before_offer(self):
        parts=self.uri.split(b'/');parts[-4]=b'c'*32
        self.assertEqual(self.lib.bs_queue_invitation(self.a,b'/'.join(parts),os.fsencode(self.source)),1);self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertFalse(self.status(self.b).awaiting_decision)
    def test_expired_and_noncanonical_addresses_rejected(self):
        parsed=Invitation()
        for value in [b'/'.join(self.uri.split(b'/')[:-2]+[str(int(time.time())-2).encode(),self.uri.split(b'/')[-1]]),self.uri.replace(b'127.0.0.1',b'127.000.0.1'),self.uri.replace(b'127.0.0.1',b'+127.0.0.1'),self.uri+b'/extra',self.uri.replace(b'127.0.0.1',b'127.0.0.1@evil')]:self.assertEqual(self.lib.bs_invitation_parse(value,C.byref(parsed)),0,value)
    def test_plaintext_cannot_be_used_for_invitation(self):
        self.lib.bs_security_configure(self.a,0,b'');self.assertEqual(self.lib.bs_queue_invitation(self.a,self.uri,os.fsencode(self.source)),0);self.assertEqual(self.lib.bs_queue_count(self.a),0)
    def test_altered_expiration_is_rejected_by_receiver(self):
        parts=self.uri.split(b'/');parts[-2]=str(int(time.time())+500).encode()
        self.assertEqual(self.lib.bs_queue_invitation(self.a,b'/'.join(parts),os.fsencode(self.source)),1);self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertFalse(self.status(self.b).awaiting_decision);self.assertIn(b'Invitation',self.status(self.a).message)
    def test_replacement_revokes_previous_invitation(self):
        out=C.create_string_buffer(256);self.assertEqual(self.lib.bs_invitation_create(self.b,b'127.0.0.1',out,256),1);self.assertNotEqual(out.value,self.uri)
        self.assertEqual(self.lib.bs_queue_invitation(self.a,self.uri,os.fsencode(self.source)),1);self.until(lambda:self.lib.bs_queue_paused(self.a));self.assertFalse(self.status(self.b).awaiting_decision)
    def test_shared_key_cannot_create_invitation(self):
        self.lib.bs_security_configure(self.b,1,b'12'*32);out=C.create_string_buffer(256);self.assertEqual(self.lib.bs_invitation_create(self.b,b'127.0.0.1',out,256),0)

if __name__=="__main__":unittest.main()
