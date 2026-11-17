"""Exercise durable queues and opt-in metadata using the real socket engine."""
import ctypes as C
import os, unittest
from test_core import CoreTests as Fixture

class Metadata(C.Structure):
    _fields_=[('protection',C.c_ulong),('filetype',C.c_ulong),('kind',C.c_ubyte),('comment',C.c_char*160)]

class SavedWork(unittest.TestCase):
    setUpClass=classmethod(Fixture.setUpClass.__func__)
    client=Fixture.client
    status=Fixture.status
    until=Fixture.until
    transfer=Fixture.transfer
    start=Fixture.start
    tearDown=Fixture.tearDown
    def setUp(self):
        Fixture.setUp(self)
        signatures={'bs_state_configure':[C.c_void_p,C.c_char_p],'bs_state_flush':[C.c_void_p], 'bs_state_restored':[C.c_void_p], 'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p], 'bs_queue_pause':[C.c_void_p], 'bs_queue_continue':[C.c_void_p], 'bs_queue_paused':[C.c_void_p], 'bs_history_count':[C.c_void_p], 'bs_history_clear':[C.c_void_p], 'bs_metadata_enable':[C.c_void_p,C.c_int], 'bs_metadata_seed':[C.c_char_p,C.POINTER(Metadata)]}
        for name,args in signatures.items():getattr(self.lib,name).argtypes=args
        self.lib.bs_state_error.argtypes=[C.c_void_p];self.lib.bs_state_error.restype=C.c_char_p
    def close(self,client):self.lib.bs_destroy(client);self.clients.remove(client)
    def test_queue_restarts_paused_and_requires_continue(self):
        state=self.root/'transfers';source=self.root/'queued';source.write_bytes(b'durable queue')
        self.assertEqual(self.lib.bs_state_configure(self.a,os.fsencode(state)),1)
        self.lib.bs_queue_pause(self.a)
        self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        self.close(self.a);self.a,self.pa=self.client(b'a'*32)
        self.assertEqual(self.lib.bs_state_configure(self.a,os.fsencode(state)),1)
        self.assertEqual(self.lib.bs_state_restored(self.a),1)
        for _ in range(30):self.lib.bs_tick(self.a);self.lib.bs_tick(self.b)
        self.assertFalse(self.status(self.b).active)
        self.lib.bs_queue_continue(self.a);self.until(lambda:self.status(self.b).awaiting_decision)
        destination=self.root/'received';self.lib.bs_accept(self.b,os.fsencode(destination));self.until(lambda:self.lib.bs_queue_count(self.a)==0)
        self.assertEqual(destination.read_bytes(),source.read_bytes())
    def test_history_persists_and_clear_is_durable(self):
        state=self.root/'history';self.lib.bs_state_configure(self.b,os.fsencode(state));self.transfer(b'saved receipt')
        self.assertEqual(self.lib.bs_history_count(self.b),1);self.close(self.b)
        self.b,self.pb=self.client(b'b'*32);self.assertEqual(self.lib.bs_state_configure(self.b,os.fsencode(state)),1);self.assertEqual(self.lib.bs_history_count(self.b),1)
        self.lib.bs_history_clear(self.b);self.assertEqual(self.lib.bs_state_flush(self.b),1);self.close(self.b)
        self.b,self.pb=self.client(b'b'*32);self.lib.bs_state_configure(self.b,os.fsencode(state));self.assertEqual(self.lib.bs_history_count(self.b),0)
    def test_corrupt_state_is_retained_without_any_queue(self):
        state=self.root/'bad';data=b'BSSTATE2'+b'corrupted state';state.write_bytes(data)
        self.assertEqual(self.lib.bs_state_configure(self.a,os.fsencode(state)),0);self.assertTrue(self.lib.bs_state_error(self.a));self.assertEqual(self.lib.bs_queue_count(self.a),0)
        self.close(self.a);self.assertEqual(state.read_bytes(),data)
    def test_changed_saved_file_does_not_get_sent(self):
        state=self.root/'state';source=self.root/'file';source.write_bytes(b'original');self.lib.bs_state_configure(self.a,os.fsencode(state));self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source));self.close(self.a)
        source.write_bytes(b'changed size');self.a,self.pa=self.client(b'a'*32);self.lib.bs_state_configure(self.a,os.fsencode(state));self.lib.bs_queue_continue(self.a);self.lib.bs_tick(self.a)
        self.assertTrue(self.lib.bs_queue_paused(self.a));self.assertFalse(self.status(self.b).active)
    def test_expanded_folder_restores_missing_parent_entries(self):
        for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p]}.items():getattr(self.lib,name).argtypes=args
        self.lib.bs_batch_enable(self.b,1);state=self.root/'folder-state';self.lib.bs_state_configure(self.a,os.fsencode(state))
        source=self.root/'Tree';(source/'Sub').mkdir(parents=True);(source/'Sub'/'file').write_bytes(b'folder payload')
        self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source));self.until(lambda:self.status(self.b).awaiting_decision);self.lib.bs_queue_pause(self.a)
        first=self.root/'first';first.mkdir();self.lib.bs_accept_batch(self.b,os.fsencode(first));self.until(lambda:not self.status(self.a).active and self.lib.bs_queue_count(self.a)==2)
        self.close(self.a);self.a,self.pa=self.client(b'a'*32);self.assertEqual(self.lib.bs_state_configure(self.a,os.fsencode(state)),1);self.assertEqual(self.lib.bs_queue_count(self.a),3)
        self.lib.bs_queue_continue(self.a);self.until(lambda:self.status(self.b).awaiting_decision)
        second=self.root/'second';second.mkdir();self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(second)),1);self.until(lambda:self.lib.bs_queue_count(self.a)==0)
        self.assertEqual((second/'Tree'/'Sub'/'file').read_bytes(),b'folder payload')
    def test_corrupt_payload_checksum_never_restores_queue(self):
        state=self.root/'checksum';source=self.root/'file';source.write_bytes(b'check')
        self.lib.bs_state_configure(self.a,os.fsencode(state));self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source));self.close(self.a)
        data=bytearray(state.read_bytes());data[-1]^=1;state.write_bytes(data);self.a,self.pa=self.client(b'a'*32)
        self.assertEqual(self.lib.bs_state_configure(self.a,os.fsencode(state)),0);self.assertEqual(self.lib.bs_queue_count(self.a),0);self.close(self.a);self.assertEqual(state.read_bytes(),data)
    def metadata_transfer(self,kind=1,receiver_enabled=True,change=False):
        source=self.root/'retro';source.write_bytes(b'retro contents')
        metadata=Metadata(16 if kind==1 else 0,0 if kind==1 else 0xfff,kind,b'Amiga comment' if kind==1 else b'')
        self.assertEqual(self.lib.bs_metadata_seed(os.fsencode(source),C.byref(metadata)),1)
        self.lib.bs_metadata_enable(self.a,1);self.lib.bs_metadata_enable(self.b,int(receiver_enabled))
        if change:source.write_bytes(b'changed contents')
        self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1);self.until(lambda:self.status(self.b).awaiting_decision)
        destination=self.root/'received';self.lib.bs_accept(self.b,os.fsencode(destination));self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(destination.read_bytes(),source.read_bytes());return source,destination
    def test_amiga_metadata_round_trip(self):
        source,dest=self.metadata_transfer();self.assertEqual(dest.with_name(dest.name+'.bskmeta').read_bytes(),source.with_name(source.name+'.bskmeta').read_bytes())
    def test_riscos_filetype_round_trip(self):
        source,dest=self.metadata_transfer(kind=2);self.assertEqual(dest.with_name(dest.name+'.bskmeta').read_bytes(),source.with_name(source.name+'.bskmeta').read_bytes())
    def test_opt_out_peer_receives_only_contents(self):
        _,dest=self.metadata_transfer(receiver_enabled=False);self.assertFalse(dest.with_name(dest.name+'.bskmeta').exists())
    def test_changed_content_does_not_forward_stale_metadata(self):
        _,dest=self.metadata_transfer(change=True);self.assertFalse(dest.with_name(dest.name+'.bskmeta').exists())
    def test_metadata_companion_never_overwrites_existing_file(self):
        occupied=self.root/'received.bskmeta';occupied.write_bytes(b'keep this')
        self.metadata_transfer();self.assertEqual(occupied.read_bytes(),b'keep this');self.assertIn(b'metadata could not',self.status(self.b).message)
    def test_companion_icon_is_opt_in(self):
        source=self.root/'app';source.write_bytes(b'app');(self.root/'app.info').write_bytes(b'icon')
        self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source));self.assertEqual(self.lib.bs_queue_count(self.a),1)
        self.lib.bs_queue_clear.argtypes=[C.c_void_p];self.lib.bs_queue_clear(self.a);self.lib.bs_metadata_enable(self.a,1)
        self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source));self.assertEqual(self.lib.bs_queue_count(self.a),2)

def load_tests(loader,tests,pattern):return loader.loadTestsFromTestCase(SavedWork)
