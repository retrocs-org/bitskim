"""Real socket checks for remembered folders and no-payload reachability."""
import ctypes as C, os
from test_queue import QueueTests
class RecentNetwork(QueueTests):
 def setUp(self):
  super().setUp();l=self.lib
  l.bs_folders_configure.argtypes=[C.c_void_p,C.c_char_p]
  l.bs_last_folder.argtypes=[C.c_void_p,C.c_int];l.bs_last_folder.restype=C.c_char_p
  l.bs_network_test.argtypes=[C.c_void_p,C.c_char_p,C.c_ushort]
  l.bs_diagnostics.argtypes=[C.c_void_p,C.c_char_p,C.c_size_t]
 def test_separate_folders_survive_reload_and_missing_folder_is_ignored(self):
  store=self.root/'folders.dat';send=self.root/'send';receive=self.root/'receive';send.mkdir();receive.mkdir()
  self.lib.bs_folders_configure(self.a,os.fsencode(store));self.lib.bs_folders_configure(self.b,os.fsencode(store))
  source=send/'file';source.write_bytes(b'abc')
  self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source))
  self.until(lambda:self.status(self.b).awaiting_decision)
  self.lib.bs_accept(self.b,os.fsencode(receive/'copy'))
  self.until(lambda:not self.status(self.a).active and (receive/'copy').exists())
  # Load the receiver's persisted slot without destroying the sender's slot.
  self.lib.bs_folders_configure(self.a,os.fsencode(store))
  self.assertEqual(self.lib.bs_last_folder(self.a,0).rstrip(b'/'),os.fsencode(send))
  self.assertEqual(self.lib.bs_last_folder(self.a,1).rstrip(b'/'),os.fsencode(receive))
  (receive/'copy').unlink();receive.rmdir()
  self.assertEqual(self.lib.bs_last_folder(self.a,1),b'')
 def test_connection_probe_does_not_create_offer(self):
  self.assertEqual(self.lib.bs_network_test(self.a,b'127.0.0.1',self.pb),1)
  report=C.create_string_buffer(4096)
  def finished():
   self.lib.bs_diagnostics(self.a,report,len(report));return b'TCP reachable' in report.value
  self.until(finished)
  for _ in range(10):self.lib.bs_tick(self.b)
  self.assertFalse(self.status(self.b).awaiting_decision)
 def test_cancel_discards_remaining_for_both_sender_and_receiver(self):
  for cancel_receiver in (False,True):
   source=self.root/'file';source.write_bytes(b'x'*100)
   for _ in range(3):self.lib.bs_queue_address(self.a,b'127.0.0.1',self.pb,os.fsencode(source))
   self.lib.bs_queue_continue(self.a)
   self.until(lambda:self.status(self.b).awaiting_decision)
   self.lib.bs_cancel(self.b if cancel_receiver else self.a)
   self.until(lambda:not self.status(self.a).active and not self.status(self.b).active)
   self.assertEqual(self.lib.bs_queue_count(self.a),0)
