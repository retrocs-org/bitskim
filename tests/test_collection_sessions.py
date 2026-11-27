"""Actual TLS batches reuse one connection, and safely fall back for old peers."""
import ctypes as C,os,unittest
import test_secure_resume as modern
class CollectionSessions(unittest.TestCase):
 setUpClass=classmethod(modern.ModernTests.setUpClass.__func__)
 setUp=modern.ModernTests.setUp;tearDown=modern.ModernTests.tearDown
 client=modern.ModernTests.client;status=modern.ModernTests.status;until=modern.ModernTests.until
 def batch(self,reuse=True):
  l=self.lib
  for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_session_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],'bs_queue_paused':[C.c_void_p]}.items():getattr(l,name).argtypes=args
  for c in (self.a,self.b):l.bs_batch_enable(c,1)
  self.assertEqual(l.bs_session_enable(self.b,int(reuse)),1)
  dest=self.root/'destination';dest.mkdir()
  for i in range(24):
   path=self.root/('file%02d'%i);path.write_bytes(('payload '+str(i)).encode());self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(path)),1)
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(dest)),1)
  self.until(lambda:l.bs_queue_count(self.a)==0,timeout=10)
  for i in range(24):self.assertEqual((dest/('file%02d'%i)).read_bytes(),('payload '+str(i)).encode())
  self.assertFalse(l.bs_queue_paused(self.a));return l.bs_test_connect_count()
 def test_tls_collection_reuses_one_connection(self):self.assertEqual(self.batch(),1)
 def test_peer_without_session_capability_keeps_individual_connections(self):self.assertEqual(self.batch(False),24)

 def test_pause_expires_cached_connection_and_continues_with_original_approval(self):
  l=self.lib
  for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p],'bs_queue_pause':[C.c_void_p],'bs_queue_continue':[C.c_void_p]}.items():getattr(l,name).argtypes=args
  for c in (self.a,self.b):l.bs_batch_enable(c,1)
  for i in range(3):
   path=self.root/('paused%d'%i);path.write_bytes(b'checked');self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(path)),1)
  self.until(lambda:self.status(self.b).awaiting_decision);l.bs_queue_pause(self.a)
  dest=self.root/'paused';dest.mkdir();self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(dest)),1)
  self.until(lambda:not self.status(self.a).active and l.bs_queue_count(self.a)==2)
  l.bs_test_advance(3)
  for c in (self.a,self.b):l.bs_tick(c)
  l.bs_queue_continue(self.a)
  def done():
   self.assertFalse(self.status(self.b).awaiting_decision)
   return l.bs_queue_count(self.a)==0
  self.until(done)
  self.assertEqual(l.bs_test_connect_count(),2)
  for i in range(3):self.assertEqual((dest/('paused%d'%i)).read_bytes(),b'checked')
 def test_new_recipient_needs_its_own_approval(self):
  l=self.lib
  for name,args in {'bs_batch_enable':[C.c_void_p,C.c_int],'bs_accept_batch':[C.c_void_p,C.c_char_p],'bs_queue_address':[C.c_void_p,C.c_char_p,C.c_ushort,C.c_char_p]}.items():getattr(l,name).argtypes=args
  other,port=self.client(b'd'*32)
  for c in (self.a,self.b,other):l.bs_batch_enable(c,1)
  for recipient in (self.pb,self.pb,port):
   path=self.root/('recipient%d'%recipient);path.write_bytes(b'private fixture');self.assertEqual(l.bs_queue_address(self.a,b'127.0.0.1',recipient,os.fsencode(path)),1)
  self.until(lambda:self.status(self.b).awaiting_decision);dest=self.root/'receiver';dest.mkdir();self.assertEqual(l.bs_accept_batch(self.b,os.fsencode(dest)),1)
  self.until(lambda:self.status(other).awaiting_decision)
  self.assertEqual(l.bs_queue_count(self.a),1);self.assertEqual(l.bs_test_connect_count(),2);l.bs_decline(other)
  self.until(lambda:not self.status(self.a).active)
