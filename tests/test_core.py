#!/usr/bin/env python3
"""Exercise the actual C engine over sockets, including adversarial protocol input."""
import ctypes as C
from pathlib import Path
import os, socket, struct, subprocess, tempfile, time, unittest, zlib
ROOT=Path(__file__).resolve().parents[1]
LIB=ROOT/'build/libbitskim.so'
class Config(C.Structure):
    _fields_=[('name',C.c_char*64),('platform',C.c_char*32),('id',C.c_char*33),('bind_address',C.c_char*48),('port',C.c_ushort),('visible',C.c_int),('receiving',C.c_int),('discovery_port',C.c_ushort)]
class Status(C.Structure):
    _fields_=[('active',C.c_int),('incoming',C.c_int),('awaiting_decision',C.c_int),('peer_name',C.c_char*64),('peer_address',C.c_char*48),('filename',C.c_char*256),('message',C.c_char*256),('total',C.c_ulong),('transferred',C.c_ulong)]
class Peer(C.Structure):
    _fields_=[('id',C.c_char*33),('name',C.c_char*64),('platform',C.c_char*32),('address',C.c_char*48),('port',C.c_ushort),('last_seen',C.c_ulong)]
def frame(kind,payload=b''): return b'BSK1'+struct.pack('>HHI',kind,0,len(payload))+payload
def hello(): return bytes.fromhex('c'*32)+struct.pack('>HBB',46440,4,4)+b'TestTest'
def offer(name,size): return struct.pack('>QH',size,len(name))+name
class CoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        LIB.parent.mkdir(exist_ok=True)
        subprocess.run(['cc','-shared','-fPIC','-g','-Wall','-Wextra','-Werror','-std=c89','tests/core_test_hooks.c','core/platform.c','-o',str(LIB)],cwd=ROOT,check=True)
        cls.lib=l=C.CDLL(str(LIB)); l.bs_platform_init()
        l.bs_create.argtypes=[C.POINTER(Config),C.c_void_p,C.c_void_p]; l.bs_create.restype=C.c_void_p
        for name in ('bs_destroy','bs_tick','bs_decline','bs_cancel','bs_refresh'): getattr(l,name).argtypes=[C.c_void_p]
        l.bs_discovery_search.argtypes=[C.c_void_p,C.c_int]
        l.bs_send.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]; l.bs_send.restype=C.c_int
        l.bs_send_peer.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p]; l.bs_send_peer.restype=C.c_int
        for name in ('bs_send_peer_address','bs_queue_peer_address'):
            getattr(l,name).argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_ushort,C.c_char_p];getattr(l,name).restype=C.c_int
        l.bs_test_peer_endpoint.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_ushort]
        l.bs_queue_count.argtypes=[C.c_void_p];l.bs_queue_count.restype=C.c_int
        l.bs_accept.argtypes=[C.c_void_p,C.c_char_p]; l.bs_accept.restype=C.c_int
        l.bs_transfer_rate.argtypes=[C.c_void_p]; l.bs_transfer_rate.restype=C.c_ulong
        l.bs_transfer_serial.argtypes=[C.c_void_p];l.bs_transfer_serial.restype=C.c_ulong
        l.bs_transfer_result.argtypes=[C.c_void_p];l.bs_transfer_result.restype=C.c_int
        l.bs_status.argtypes=[C.c_void_p]; l.bs_status.restype=C.POINTER(Status)
        l.bs_last_error.argtypes=[C.c_void_p]; l.bs_last_error.restype=C.c_char_p
        l.bs_peer_count.argtypes=[C.c_void_p]; l.bs_peer_count.restype=C.c_int
        l.bs_peer.argtypes=[C.c_void_p,C.c_int]; l.bs_peer.restype=C.POINTER(Peer)
        l.bs_safe_name.argtypes=[C.c_char_p]; l.bs_safe_name.restype=C.c_int
        l.bs_crc32.argtypes=[C.c_ulong,C.c_char_p,C.c_size_t]; l.bs_crc32.restype=C.c_ulong
    def setUp(self):
        self.lib.bs_test_reset()
        self.tmp=tempfile.TemporaryDirectory(); self.root=Path(self.tmp.name); self.clients=[]; self.sockets=[]
        self.a,self.pa=self.client(b'a'*32); self.b,self.pb=self.client(b'b'*32)
    def tearDown(self):
        for s in self.sockets: s.close()
        for c in self.clients: self.lib.bs_destroy(c)
        self.tmp.cleanup()
    def client(self,identity,visible=1,receiving=1):
        # Ask the OS for a temporary TCP port; engine owns TCP and UDP on it.
        for _ in range(20):
            s=socket.socket(); s.bind(('127.0.0.1',0)); port=s.getsockname()[1]
            udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
            try: udp.bind(('127.0.0.1',port))
            except OSError: s.close(); udp.close(); continue
            s.close(); udp.close()
            conf=Config(b'Test peer',b'Test',identity,b'127.0.0.1',port,visible,receiving)
            c=self.lib.bs_create(C.byref(conf),None,None)
            if c: self.clients.append(c); return c,port
        self.fail('Cannot open a test port')
    def status(self,c): return self.lib.bs_status(c).contents
    def until(self,predicate,timeout=5):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            for c in self.clients: self.lib.bs_tick(c)
            if predicate(): return
            time.sleep(.0005)
        self.fail('Timed out: '+repr([(self.status(c).message,self.status(c).active) for c in self.clients]))
    def start(self,data=b'payload',name='test.bin'):
        source=self.root/name; source.write_bytes(data)
        self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        self.until(lambda:self.status(self.b).awaiting_decision)
        return source
    def transfer(self,data):
        self.start(data); dest=self.root/'received.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(dest.read_bytes(),data)
        self.assertIn(b'delivered successfully',self.status(self.a).message)
        self.assertEqual(list(self.root.glob('*.part')),[])
    def raw_peer(self):
        s=socket.create_connection(('127.0.0.1',self.pb)); s.settimeout(.5); self.sockets.append(s)
        self.until(lambda:self.status(self.b).active)
        return s
    def test_disconnect_is_read_even_while_sending_is_blocked(self):
        self.start(b'payload')
        destination=self.root/'interrupted.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(destination)),1)
        self.lib.bs_test_block_send(1)
        self.lib.bs_destroy(self.a); self.clients.remove(self.a)
        self.until(lambda:not self.status(self.b).active,timeout=1)
        self.assertIn(b'disconnected',self.status(self.b).message)
        self.assertFalse(destination.exists())
        self.assertEqual(list(self.root.glob('*.part')),[])
    def advertise(self,identity,port):
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp)
        greeting=bytes.fromhex(identity.decode())+struct.pack('>HBB',port,4,4)+b'TestTest'
        udp.sendto((b'BSKD\x01\x02'+greeting).ljust(160,b'\0'),('127.0.0.1',self.pa))
    def test_timeout_removes_only_failed_peer_and_discovery_can_restore_it(self):
        self.advertise(b'b'*32,self.pb); self.advertise(b'd'*32,self.pb+1 if self.pb<65535 else self.pb-1)
        self.until(lambda:self.lib.bs_peer_count(self.a)==2)
        self.start(b'x'*(8<<20)); destination=self.root/'offline.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(destination)),1)
        self.until(lambda:b'Sending file' in self.status(self.a).message)
        self.lib.bs_test_block_send(1); self.lib.bs_test_advance(31)
        self.until(lambda:not self.status(self.a).active)
        self.assertEqual(self.lib.bs_peer_count(self.a),1)
        self.assertEqual(self.lib.bs_peer(self.a,0).contents.id,b'd'*32)
        self.advertise(b'b'*32,self.pb)
        self.until(lambda:self.lib.bs_peer_count(self.a)==2)
    def test_declining_file_does_not_remove_reachable_peer(self):
        self.advertise(b'b'*32,self.pb); self.until(lambda:self.lib.bs_peer_count(self.a)==1)
        self.start(); self.lib.bs_decline(self.b)
        self.until(lambda:not self.status(self.a).active)
        self.assertEqual(self.lib.bs_peer_count(self.a),1)
    def test_stalled_transfer_reports_waiting_then_times_out(self):
        self.start(b'x'*(8<<20))
        destination=self.root/'stalled.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(destination)),1)
        self.until(lambda:b'Sending file' in self.status(self.a).message)
        self.lib.bs_test_block_send(1); self.lib.bs_test_advance(6)
        self.lib.bs_tick(self.a)
        self.assertIn(b'Waiting for peer',self.status(self.a).message)
        self.lib.bs_test_advance(31)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertIn(b'timed out',self.status(self.a).message)
        self.assertFalse(destination.exists())
        self.assertEqual(list(self.root.glob('*.part')),[])
    def test_partial_writes_preserve_queued_protocol_frames(self):
        self.lib.bs_test_send_limit(1)
        self.transfer(b"partial writes preserve framing")
    def test_paused_discovery_replies_without_searching_or_tracking_peers(self):
        self.advertise(b'b'*32,self.pb); self.until(lambda:self.lib.bs_peer_count(self.a)==1)
        self.lib.bs_discovery_search(self.a,0)
        self.assertEqual(self.lib.bs_peer_count(self.a),0)
        before=self.lib.bs_test_broadcast_count()
        self.lib.bs_refresh(self.a); self.lib.bs_test_advance(31); self.lib.bs_tick(self.a)
        self.assertEqual(self.lib.bs_test_broadcast_count(),before)
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp); udp.bind(('127.0.0.1',0)); udp.settimeout(1)
        udp.sendto((b'BSKD\x01\x01'+hello()).ljust(160,b'\0'),('127.0.0.1',self.pa))
        udp.setblocking(False); replies=[]
        def got_reply():
            try: replies.append(udp.recvfrom(512)[0]); return True
            except BlockingIOError: return False
        self.until(got_reply)
        self.assertEqual(replies[0][:6],b'BSKD\x01\x02')
        self.assertEqual(self.lib.bs_peer_count(self.a),0)
        before_resume=self.lib.bs_test_broadcast_count()
        self.lib.bs_discovery_search(self.a,1)
        self.assertEqual(self.lib.bs_test_broadcast_count(),before_resume+1)
        self.advertise(b'b'*32,self.pb); self.until(lambda:self.lib.bs_peer_count(self.a)==1)
    def test_paused_discovery_keeps_background_file_reception(self):
        self.lib.bs_discovery_search(self.b,0)
        self.transfer(b'background reception while iconified')
    def test_sender_counts_only_fully_written_payload_and_reports_rate(self):
        self.start(b'x'*(8<<20)); dest=self.root/'stats.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        for _ in range(5): self.lib.bs_tick(self.b) # flush ACCEPT before blocking writes
        self.lib.bs_test_block_send(1)
        self.until(lambda:b'Sending file' in self.status(self.a).message)
        self.lib.bs_tick(self.a)
        self.assertEqual(self.status(self.a).transferred,0)
        self.lib.bs_test_block_send(0); self.lib.bs_test_send_limit(1)
        for _ in range(12): self.lib.bs_tick(self.a)
        self.assertEqual(self.status(self.a).transferred,0)
        self.lib.bs_test_send_limit(0); self.lib.bs_tick(self.a)
        # The pending frame completes, then at most one bounded burst follows.
        done=self.status(self.a).transferred
        self.assertGreater(done,0); self.assertEqual(done%4096,0); self.assertLessEqual(done,33*4096)
        self.lib.bs_test_block_send(1); self.lib.bs_test_advance(2); self.lib.bs_tick(self.a)
        self.assertGreater(self.lib.bs_transfer_rate(self.a),0)
        self.assertLessEqual(self.lib.bs_transfer_rate(self.a),done)
        self.lib.bs_test_advance(3); self.lib.bs_tick(self.a)
        self.assertEqual(self.lib.bs_transfer_rate(self.a),0)
        self.lib.bs_cancel(self.a)
        self.assertEqual(self.lib.bs_transfer_rate(self.a),0)
    def test_binary_transfer(self): self.transfer(os.urandom(300_017))
    def test_posix_backslash_filename_is_not_silently_renamed(self):
        source=self.root/'not\\portable.bin'; source.write_bytes(b'payload')
        self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),0)
    def test_zero_byte_transfer(self): self.transfer(b'')
    def test_reverse_direction(self):
        self.a,self.b=self.b,self.a; self.pa,self.pb=self.pb,self.pa
        self.transfer(os.urandom(8193))
    def test_decline_sends_no_payload(self):
        self.start(os.urandom(8000)); self.lib.bs_decline(self.b)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(self.status(self.a).transferred,0)
        self.assertEqual(list(self.root.glob('*.part')),[])
    def test_collision_never_overwrites(self):
        self.start(); dest=self.root/'existing'; dest.write_bytes(b'original')
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(dest.read_bytes(),b'original')
        self.assertIn(b'nothing overwritten',self.status(self.b).message)
        self.assertEqual(list(self.root.glob('*.part')),[])
    def test_cancel_removes_partial(self):
        self.start(os.urandom(900_000)); dest=self.root/'cancelled'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:self.status(self.b).transferred>0)
        self.lib.bs_cancel(self.b); self.until(lambda:not self.status(self.a).active)
        self.assertFalse(dest.exists()); self.assertEqual(list(self.root.glob('*.part')),[])
    def test_observed_results_and_serials_survive_completion(self):
        self.assertEqual(self.lib.bs_transfer_serial(self.a),0)
        self.assertEqual(self.lib.bs_transfer_result(self.a),0)
        self.transfer(b'completed payload')
        for c in (self.a,self.b):
            self.assertEqual(self.lib.bs_transfer_serial(c),1)
            self.assertEqual(self.lib.bs_transfer_result(c),1)
            self.lib.bs_tick(c);self.assertEqual(self.lib.bs_transfer_result(c),1)
        self.start(b'next',name='second.bin')
        self.assertEqual(self.lib.bs_transfer_serial(self.a),2)
        self.assertEqual(self.lib.bs_transfer_result(self.a),0)
        self.lib.bs_decline(self.b)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(self.lib.bs_transfer_result(self.a),4)
        self.assertEqual(self.lib.bs_transfer_result(self.b),4)

    def test_cancel_and_timeout_have_explicit_distinct_results(self):
        self.start();self.lib.bs_cancel(self.b)
        self.assertEqual(self.lib.bs_transfer_result(self.b),3)
        self.until(lambda:not self.status(self.a).active)
        self.assertEqual(self.lib.bs_transfer_result(self.a),3) # remote CANCEL frame, not bare EOF
        self.start(name='later.bin');self.lib.bs_test_advance(121)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(self.lib.bs_transfer_result(self.a),2)
        self.assertEqual(self.lib.bs_transfer_result(self.b),2)

    def test_remote_cancel_frame_is_distinct_from_failure(self):
        s=self.raw_peer();s.sendall(frame(8,b'Cancelled by remote user'))
        self.until(lambda:not self.status(self.b).active)
        self.assertEqual(self.lib.bs_transfer_result(self.b),3)

    def test_stable_peer_send_checks_hello_identity_before_offer(self):
        source=self.root/'identity.bin';source.write_bytes(b'must not reach the wrong peer')
        # Stale catalog entry: ID c now points at the address occupied by ID b.
        self.advertise(b'c'*32,self.pb);self.until(lambda:self.lib.bs_peer_count(self.a)>0)
        self.assertEqual(self.lib.bs_send_peer(self.a,b'c'*32,os.fsencode(source)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(self.lib.bs_transfer_result(self.a),2)
        self.assertIn(b'identity changed',self.status(self.a).message)
        self.assertEqual(self.status(self.b).total,0)
        self.assertEqual(self.status(self.b).filename,b'')
        self.assertEqual(self.status(self.b).transferred,0)
        # Correct current identity resolves and can deliver through the same API.
        self.advertise(b'b'*32,self.pb);self.until(lambda:self.lib.bs_peer_count(self.a)>0)
        self.assertEqual(self.lib.bs_send_peer(self.a,b'b'*32,os.fsencode(source)),1)
        self.until(lambda:self.status(self.b).awaiting_decision)
        dest=self.root/'correct';self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
        self.assertEqual(dest.read_bytes(),source.read_bytes())
        self.assertEqual(self.lib.bs_send_peer(self.a,b'd'*32,os.fsencode(source)),0)

    def test_invalid_names_and_utf8(self):
        for name in (b'',b'..',b'../bad',b'RAM:bad',b'a\\b',b'bad\x01',b'a.',b'\xc0\xaf',b'\xed\xa0\x80',b'\xf4\x90\x80\x80'):
            self.assertEqual(self.lib.bs_safe_name(name),0,name)
        self.assertEqual(self.lib.bs_safe_name('café.txt'.encode()),1)
    def test_crc_golden_and_incremental(self):
        self.assertEqual(self.lib.bs_crc32(0,b'123456789',9),0xcbf43926)
        crc=self.lib.bs_crc32(0,b'1234',4); self.assertEqual(self.lib.bs_crc32(crc,b'56789',5),0xcbf43926)
    def test_malformed_length_closes_connection(self):
        s=self.raw_peer(); s.sendall(b'BSK1'+struct.pack('>HHI',1,0,0xffffffff))
        self.until(lambda:not self.status(self.b).active)
        self.assertIn(b'large',self.status(self.b).message)
    def test_fragmented_greeting_and_bad_checksum(self):
        s=self.raw_peer(); wire=frame(1,hello())+frame(2,offer(b'sample.bin',3))
        for octet in wire:
            s.sendall(bytes([octet])); self.lib.bs_tick(self.b)
        self.until(lambda:self.status(self.b).awaiting_decision)
        dest=self.root/'checksum'; self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        s.sendall(frame(5,b'abc')+frame(6,struct.pack('>I',0)))
        self.until(lambda:not self.status(self.b).active)
        self.assertFalse(dest.exists()); self.assertEqual(list(self.root.glob('*.part')),[])
        self.assertIn(b'Checksum mismatch',self.status(self.b).message)
        self.assertEqual(self.lib.bs_transfer_result(self.b),2)
    def test_preaccept_data_is_rejected(self):
        s=self.raw_peer(); s.sendall(frame(1,hello())+frame(2,offer(b'x',1))+frame(5,b'x'))
        self.until(lambda:not self.status(self.b).active)
        self.assertEqual(list(self.root.iterdir()),[])
    def test_oversize_offer_rejected(self):
        s=self.raw_peer(); s.sendall(frame(1,hello())+frame(2,offer(b'x',2**31)))
        self.until(lambda:not self.status(self.b).active)
        self.assertIn(b'file size',self.status(self.b).message)
    def test_receiving_disabled(self):
        c,port=self.client(b'd'*32,receiving=0)
        s=socket.create_connection(('127.0.0.1',port)); self.sockets.append(s); s.settimeout(2)
        for _ in range(10): self.lib.bs_tick(c)
        self.assertFalse(self.status(c).active)
        reply=s.recv(256); self.assertEqual(reply[:6],b'BSK1\x00\x09'); self.assertIn(b'not accepting',reply)
    def test_busy_receiver_refuses_with_reason_and_stays_listed(self):
        self.advertise(b'b'*32,self.pb); self.until(lambda:self.lib.bs_peer_count(self.a)==1)
        self.raw_peer()
        source=self.root/'busy.bin'; source.write_bytes(b'payload')
        self.assertEqual(self.lib.bs_send(self.a,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        self.until(lambda:not self.status(self.a).active)
        self.assertIn(b'busy',self.status(self.a).message)
        self.assertEqual(self.lib.bs_transfer_result(self.a),2)
        self.assertEqual(self.lib.bs_peer_count(self.a),1)
    def test_sender_cancel_reaches_receiver_as_cancel(self):
        self.start(os.urandom(4<<20)); dest=self.root/'cancelled-by-sender'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:self.status(self.b).transferred>0)
        self.lib.bs_cancel(self.a)
        self.until(lambda:not self.status(self.b).active)
        self.assertEqual(self.lib.bs_transfer_result(self.a),3)
        self.assertEqual(self.lib.bs_transfer_result(self.b),3)
        self.assertIn(b'cancelled',self.status(self.b).message)
        self.assertFalse(dest.exists()); self.assertEqual(list(self.root.glob('*.part')),[])
    def test_remote_reason_text_is_sanitized(self):
        s=self.raw_peer(); s.sendall(frame(8,b'bad\x1b[2Jtext'))
        self.until(lambda:not self.status(self.b).active)
        self.assertEqual(self.status(self.b).message,b'bad [2Jtext')
        s=self.raw_peer(); s.sendall(frame(9,b'\xff\xfe'))
        self.until(lambda:not self.status(self.b).active)
        self.assertEqual(self.status(self.b).message,b'Peer reported an error')
    def test_listener_rebinds_immediately_after_transfer(self):
        self.transfer(os.urandom(5000))
        self.lib.bs_destroy(self.b); self.clients.remove(self.b)
        conf=Config(b'Again',b'Test',b'b'*32,b'127.0.0.1',self.pb,1,1)
        again=self.lib.bs_create(C.byref(conf),None,None)
        self.assertTrue(again); self.clients.append(again)
    def test_large_transfer_needs_few_ticks(self):
        data=os.urandom(4<<20); self.start(data); dest=self.root/'fast.bin'
        self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        ticks=0
        while (self.status(self.a).active or self.status(self.b).active) and ticks<2000:
            self.lib.bs_tick(self.a); self.lib.bs_tick(self.b); ticks+=1; time.sleep(.0005)
        self.assertEqual(dest.read_bytes(),data)
        self.assertLess(ticks,400) # one 4 KiB chunk per tick would need 1024+
    def test_peer_events_only_for_catalog_changes(self):
        events=[]
        callback=C.CFUNCTYPE(None,C.c_void_p,C.c_int)(lambda ctx,event: events.append(event))
        self._keep=callback
        for _ in range(20):
            s=socket.socket(); s.bind(('127.0.0.1',0)); port=s.getsockname()[1]; s.close()
            conf=Config(b'Watcher',b'Test',b'e'*32,b'127.0.0.1',port,1,1)
            c=self.lib.bs_create(C.byref(conf),callback,None)
            if c: break
        self.assertTrue(c); self.clients.append(c)
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp)
        def announce(name):
            greeting=bytes.fromhex('c'*32)+struct.pack('>HBB',46440,len(name),4)+name+b'Test'
            udp.sendto((b'BSKD\x01\x02'+greeting).ljust(160,b'\0'),('127.0.0.1',port))
            for _ in range(50): self.lib.bs_tick(c); time.sleep(.001)
        announce(b'One'); announce(b'One'); announce(b'One')
        self.assertEqual(events.count(1),1)
        announce(b'Renamed')
        self.assertEqual(events.count(1),2)
    def test_discovery_replies_are_limited_per_source(self):
        sockets=[]
        for address in ('127.0.0.1','127.0.0.2'):
            udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp)
            try: udp.bind((address,0))
            except OSError: self.skipTest('host lacks a second loopback address')
            udp.setblocking(False); sockets.append(udp)
        for udp in sockets: udp.sendto((b'BSKD\x01\x01'+hello()).ljust(160,b'\0'),('127.0.0.1',self.pb))
        got=set()
        def both():
            for i,udp in enumerate(sockets):
                try: udp.recv(512); got.add(i)
                except BlockingIOError: pass
            return len(got)==2
        self.until(both)
    def test_discovery_reply_and_hidden_behavior(self):
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); udp.bind(('127.0.0.1',0)); udp.setblocking(False); self.sockets.append(udp)
        payload=b'BSKD\x01\x01'+hello(); udp.sendto(payload.ljust(160,b'\0'),('127.0.0.1',self.pb))
        data=[]
        def got_reply():
            try: data.append(udp.recv(512)); return True
            except BlockingIOError: return False
        self.until(got_reply); self.assertEqual(data[0][:6],b'BSKD\x01\x02')
        hidden,port=self.client(b'e'*32,visible=0)
        udp.sendto(payload.ljust(160,b'\0'),('127.0.0.1',port))
        for _ in range(20): self.lib.bs_tick(hidden)
        with self.assertRaises(BlockingIOError): udp.recv(512)
    def test_shared_host_discovery_with_different_tcp_ports(self):
        conf=Config(b'Amiga test peer',b'AmigaOS',b'f'*32,b'127.0.0.1',0,1,1,self.pb)
        probe=socket.socket(); probe.bind(('127.0.0.1',0)); conf.port=probe.getsockname()[1]; probe.close()
        c=self.lib.bs_create(C.byref(conf),None,None); self.assertTrue(c); self.clients.append(c)
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp)
        greeting=bytes.fromhex('f'*32)+struct.pack('>HBB',conf.port,5,7)+b'AmigaAmigaOS'
        udp.sendto((b'BSKD\x01\x01'+greeting).ljust(160,b'\0'),('127.0.0.1',self.pb))
        self.until(lambda:self.lib.bs_peer_count(self.b)>0)
        peer=self.lib.bs_peer(self.b,0).contents; self.assertEqual(peer.port,conf.port)
    def test_discovery_adds_peer(self):
        udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); self.sockets.append(udp)
        udp.sendto((b'BSKD\x01\x02'+hello()).ljust(160,b'\0'),('127.0.0.1',self.pb))
        self.until(lambda:self.lib.bs_peer_count(self.b)>0)
        peer=self.lib.bs_peer(self.b,0).contents
        self.assertEqual(peer.name,b'Test'); self.assertEqual(peer.address,b'127.0.0.1')
    def test_queued_discovered_endpoint_selects_one_address_and_preserves_identity(self):
        source=self.root/'selected.txt';source.write_bytes(b'selected endpoint')
        self.lib.bs_test_peer_endpoint(self.a,b'b'*32,b'127.0.0.1',self.pb)
        self.lib.bs_test_peer_endpoint(self.a,b'b'*32,b'192.0.2.123',self.pb)
        self.assertEqual(self.lib.bs_send_peer(self.a,b'b'*32,os.fsencode(source)),0)
        self.assertIn(b'ambiguous',self.lib.bs_last_error(self.a))
        self.assertEqual(self.lib.bs_queue_peer_address(self.a,b'b'*32,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        self.until(lambda:self.status(self.b).awaiting_decision)
        dest=self.root/'received-selected.txt';self.assertEqual(self.lib.bs_accept(self.b,os.fsencode(dest)),1)
        self.until(lambda:not self.status(self.a).active and not self.status(self.b).active and not self.lib.bs_queue_count(self.a))
        self.assertEqual(dest.read_bytes(),b'selected endpoint')
    def test_selected_endpoint_rejects_changed_hello_identity(self):
        source=self.root/'private.txt';source.write_bytes(b'must not be offered')
        self.lib.bs_test_peer_endpoint(self.a,b'c'*32,b'127.0.0.1',self.pb)
        self.assertEqual(self.lib.bs_send_peer_address(self.a,b'c'*32,b'127.0.0.1',self.pb,os.fsencode(source)),1)
        self.until(lambda:not self.status(self.a).active)
        self.assertIn(b'Peer identity changed',self.lib.bs_last_error(self.a))
        self.assertFalse(self.status(self.b).awaiting_decision)
    def test_selected_endpoint_rejects_unadvertised_address_or_port(self):
        source=self.root/'unused.txt';source.write_bytes(b'x')
        self.lib.bs_test_peer_endpoint(self.a,b'b'*32,b'127.0.0.1',self.pb)
        port=self.pb+1 if self.pb<65535 else self.pb-1
        self.assertEqual(self.lib.bs_send_peer_address(self.a,b'b'*32,b'127.0.0.1',port,os.fsencode(source)),0)
        self.assertFalse(self.status(self.a).active)
        self.assertIn(b'address is unavailable',self.lib.bs_last_error(self.a))
if __name__=='__main__': unittest.main(verbosity=2)
