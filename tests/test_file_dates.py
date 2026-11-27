"""Real transfers: opt-in dates, legacy wire compatibility and recovery actions."""
import ctypes as C
import os, socket, struct, unittest, zlib
import test_core as base

class FileDates(unittest.TestCase):
    setUpClass=classmethod(base.CoreTests.setUpClass.__func__)
    client=base.CoreTests.client
    status=base.CoreTests.status
    until=base.CoreTests.until
    start=base.CoreTests.start
    raw_peer=base.CoreTests.raw_peer
    tearDown=base.CoreTests.tearDown
    def setUp(self):
        base.CoreTests.setUp(self)
        for name in ('bs_metadata_enable','bs_test_fail_mtime'):
            getattr(self.lib,name).argtypes=[C.c_void_p,C.c_int] if name=='bs_metadata_enable' else [C.c_int]
        for name in ('bs_user_error','bs_user_status'):
            getattr(self.lib,name).argtypes=[C.c_void_p];getattr(self.lib,name).restype=C.c_char_p
        for name in ('bs_error_actions','bs_error_retry'):
            getattr(self.lib,name).argtypes=[C.c_void_p]
    def dated(self,stamp=946684800,data=b'payload',sender=True,receiver=True,change=False):
        self.lib.bs_metadata_enable(self.a,int(sender));self.lib.bs_metadata_enable(self.b,int(receiver))
        source=self.root/'source';source.write_bytes(data);os.utime(source,(stamp,stamp))
        self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        if change:os.utime(source,(stamp+1,stamp+1))
        self.until(lambda:self.status(self.b).awaiting_decision)
        dest=self.root/'received';self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(dest.read_bytes(),data);self.assertEqual(list(self.root.glob('*.part')),[])
        return dest
    def test_verified_file_date_and_receipt(self):
        dest=self.dated();self.assertEqual(int(dest.stat().st_mtime),946684800)
        for c in (self.a,self.b):self.assertIn(b'modification date',self.status(c).message);self.assertIn(b'preserved',self.status(c).message)
    def test_empty_file_epoch_zero_is_a_valid_date(self):
        dest=self.dated(stamp=0,data=b'');self.assertEqual(int(dest.stat().st_mtime),0)
    def test_opt_out_either_end_keeps_normal_receive_date(self):
        for sender,receiver in ((False,False),(True,False),(False,True)):
            with self.subTest(sender=sender,receiver=receiver):
                dest=self.dated(sender=sender,receiver=receiver);self.assertNotEqual(int(dest.stat().st_mtime),946684800)
                self.assertNotIn(b'modification date',self.status(self.a).message);dest.unlink()
    def test_unavailable_destination_date_does_not_lose_verified_contents(self):
        self.lib.bs_test_fail_mtime(1);self.dated()
        self.assertIn(b'could not',self.status(self.a).message);self.assertIn(b'could not',self.status(self.b).message)
        self.assertEqual(self.lib.bs_transfer_result(self.b),1)
    def test_changed_source_date_is_not_silently_claimed_preserved(self):
        dest=self.dated(change=True);self.assertNotEqual(int(dest.stat().st_mtime),946684800)
        self.assertIn(b'date unavailable or changed',self.status(self.a).message)
    def test_dates_apply_to_each_nested_file_without_copying_directory_dates(self):
        for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]}.items():getattr(self.lib,name).argtypes=args
        self.lib.bs_batch_enable(self.b,1);self.lib.bs_metadata_enable(self.a,1);self.lib.bs_metadata_enable(self.b,1)
        folder=self.root/'tree';(folder/'nested').mkdir(parents=True)
        for index,name in enumerate(('first','nested/second')):
            f=folder/name;f.write_bytes(name.encode());os.utime(f,(946684800+index*60,946684800+index*60))
        self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(folder)),1)
        self.until(lambda:self.status(self.b).awaiting_decision);dest=self.root/'target';dest.mkdir()
        self.assertEqual(self.lib.bs_accept_batch(self.b,os.fsencode(dest)),1)
        self.until(lambda:self.lib.bs_queue_count(self.a)==0 and not self.status(self.a).active)
        for index,name in enumerate(('first','nested/second')):
            f=dest/'tree'/name;self.assertEqual(f.read_bytes(),name.encode());self.assertEqual(int(f.stat().st_mtime),946684800+index*60)
    def test_platform_setter_refuses_a_symlink(self):
        self.lib.bsp_mtime_set.argtypes=[C.c_char_p,C.c_ulong]
        target=self.root/'target';target.write_bytes(b'keep');before=target.stat().st_mtime_ns
        link=self.root/'link';link.symlink_to(target)
        self.assertEqual(self.lib.bsp_mtime_set(os.fsencode(link),946684800),0);self.assertEqual(target.stat().st_mtime_ns,before)
    def raw_offer(self,features=16|16384):
        self.lib.bs_metadata_enable(self.b,1);s=self.raw_peer()
        platform=b'Test|BX10';hello=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,len(platform))+b'Test'+platform
        caps=struct.pack('>4sBBBBIIIIHHHHI',b'BXC1',1,0,0,0,features,features,2147483647,2147483647,255,255,1023,0,1)
        s.sendall(base.frame(1,hello)+base.frame(21,caps)+base.frame(2,base.offer(b'file',1)))
        self.until(lambda:self.status(self.b).awaiting_decision)
        dest=self.root/'raw-received';self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        return s,dest
    def test_unnegotiated_date_rejected_without_committing(self):
        s,dest=self.raw_offer(features=16);s.sendall(base.frame(5,b'x')+base.frame(33,struct.pack('>III',946684800,zlib.crc32(b'x'),1)))
        self.until(lambda:not self.status(self.b).active);self.assertIn(b'unnegotiated',self.status(self.b).message);self.assertFalse(dest.exists())
    def test_duplicate_early_and_payload_mismatched_dates_rejected(self):
        for mode in ('duplicate','early','crc','size','length'):
            with self.subTest(mode=mode):
                s,dest=self.raw_offer();stamp=struct.pack('>III',946684800,zlib.crc32(b'x')+(mode=='crc'),1+(mode=='size'))
                data=b'' if mode=='early' else base.frame(5,b'x')
                if mode=='length':stamp=stamp[:-1]
                s.sendall(data+base.frame(33,stamp)+(base.frame(33,stamp) if mode=='duplicate' else b''))
                self.until(lambda:not self.status(self.b).active);self.assertFalse(dest.exists());self.assertIn(b'Invalid or unnegotiated',self.status(self.b).message)
    def test_network_recovery_retries_only_the_current_outgoing_failure(self):
        self.start();self.lib.bs_destroy(self.b);self.clients.remove(self.b)
        self.until(lambda:not self.status(self.a).active)
        raw=self.lib.bs_last_error(self.a);guided=self.lib.bs_user_error(self.a)
        self.assertTrue(guided.startswith(raw));self.assertIn(b'Next:',guided);self.assertEqual(self.lib.bs_error_actions(self.a)&3,3)
        self.assertEqual(self.lib.bs_send(self.a,b'bad address',1,b'/missing'),0)
        self.assertEqual(self.lib.bs_error_actions(self.a)&1,0)
    def test_failure_retry_does_not_resume_unrelated_queued_work(self):
        self.start();self.lib.bs_destroy(self.b);self.clients.remove(self.b);self.until(lambda:not self.status(self.a).active)
        self.lib.bs_queue_pause.argtypes=[C.c_void_p];self.lib.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
        self.lib.bs_queue_pause(self.a);self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(self.root/'test.bin')),1)
        self.assertEqual(self.lib.bs_error_actions(self.a)&1,0);self.assertEqual(self.lib.bs_error_retry(self.a),0)
        self.assertEqual(self.lib.bs_queue_count(self.a),1)
    def test_preconnect_queue_failure_records_a_new_failed_receipt(self):
        self.lib.bs_queue_address.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]
        self.lib.bs_history_count.argtypes=[C.c_void_p]
        self.lib.bs_history_text.argtypes=[C.c_void_p,C.c_int,C.c_char_p,C.c_size_t]
        self.lib.bs_test_fail_connect.argtypes=[C.c_int]
        dest=self.dated(sender=False,receiver=False);previous=self.lib.bs_transfer_serial(self.a)
        self.lib.bs_test_fail_connect(1);self.assertEqual(self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(self.root/'source')),1)
        self.until(lambda:self.lib.bs_transfer_result(self.a)==2)
        self.assertGreater(self.lib.bs_transfer_serial(self.a),previous);self.assertEqual(self.lib.bs_history_count(self.a),2)
        report=C.create_string_buffer(2048);self.lib.bs_history_text(self.a,0,report,len(report))
        self.assertIn(b'Could not connect',report.value);self.assertIn(b'transfer incomplete',report.value)
        self.assertIn(b'Next:',self.lib.bs_user_status(self.a));self.assertEqual(dest.read_bytes(),b'payload')
    def test_untrusted_remote_error_is_sanitized_and_does_not_offer_trust_bypass(self):
        s=self.raw_peer();s.sendall(base.frame(1,base.hello())+base.frame(9,b'Trusted device identity changed\n'))
        self.until(lambda:not self.status(self.b).active)
        self.assertNotIn(b'\n',self.lib.bs_last_error(self.b));self.assertIn(b'other client',self.lib.bs_user_error(self.b));self.assertEqual(self.lib.bs_error_actions(self.b),0)
