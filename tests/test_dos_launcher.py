"""The test launcher must preserve user files and mount only its test drive."""
import importlib.util,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dos_launcher',ROOT/'scripts/run-dos.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class Launcher(unittest.TestCase):
 def test_refresh_preserves_settings_received_files_and_sample_edits(self):
  with tempfile.TemporaryDirectory() as directory:
   root=Path(directory);run=root/'drive';(run/'FILES').mkdir(parents=True);(run/'INBOX').mkdir()
   (run/'BITSKIM.CFG').write_bytes(b'NAME=Existing\n');(run/'INBOX/SAVED.DAT').write_bytes(b'keep');(run/'FILES/HELLO.TXT').write_bytes(b'my edit')
   exe=root/'new.exe';driver=root/'driver.com';exe.write_bytes(b'MZ test');driver.write_bytes(b'packet')
   config=module.prepare(run,exe,driver,46442)
   self.assertEqual((run/'BITSKIM.CFG').read_bytes(),b'NAME=Existing\n')
   self.assertEqual((run/'INBOX/SAVED.DAT').read_bytes(),b'keep')
   self.assertEqual((run/'FILES/HELLO.TXT').read_bytes(),b'my edit')
   self.assertEqual((run/'BITSKIM.EXE').read_bytes(),exe.read_bytes())
   self.assertIn('mount c "'+str(run)+'"',config.read_text())
   self.assertNotIn('webserver_enabled',config.read_text())
   self.assertIn('tcp_port_forwards=46442:46442',config.read_text())
   self.assertIn('udp_port_forwards=46442:46440',config.read_text())
   self.assertIn('set BITSKIM_PORT=46442',(run/'BSSTART.BAT').read_text())
   self.assertIn('set BITSKIM_NAT_HOST=10.0.2.2',(run/'BSSTART.BAT').read_text())
 def test_rejects_mount_command_injection_before_writing(self):
  with tempfile.TemporaryDirectory() as directory:
   run=Path(directory)/'bad"\nexit'
   with self.assertRaises(ValueError):module.prepare(run,Path('exe'),Path('driver'),46442)
   self.assertFalse(run.exists())
