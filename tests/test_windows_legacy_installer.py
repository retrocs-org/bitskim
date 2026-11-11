"""Check actual setup payload and non-destructive legacy installer policy."""
from pathlib import Path
import shutil,struct,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ClassicInstaller(unittest.TestCase):
 def test_policy(self):
  source=(ROOT/'installers/windows/BitSkim-Classic.nsi').read_text()
  self.assertIn('Unicode false',source)
  self.assertNotIn('System::',source)
  self.assertNotIn('RMDir /r',source)
  self.assertNotIn('Delete "$INSTDIR\\BitSkim.ini"',source)
  self.assertNotIn('File "${ROOT}/build/windows/BitSkim.exe"',source)
  self.assertIn('FindWindow $0 "BitSkimLegacy"',source)
 def test_real_payload(self):
  setup=ROOT/'outputs/BitSkim-0.1-Windows-Classic-x86-Setup.exe'
  if not setup.exists() or not shutil.which('7z'): self.skipTest('Build setup and install 7z first')
  with tempfile.TemporaryDirectory() as d:
   subprocess.run(['7z','x','-y','-o'+d,str(setup)],check=True,stdout=subprocess.DEVNULL)
   p=Path(d)
   self.assertEqual((p/'BitSkim.exe').read_bytes(),(ROOT/'build/windows-legacy/BitSkim.exe').read_bytes())
   self.assertEqual((p/'BUILD-PROVENANCE.json').read_bytes(),(ROOT/'build/provenance/windows-legacy.json').read_bytes())
   self.assertEqual((p/'README.MD').read_bytes(),(ROOT/'docs/WINDOWS-LEGACY.md').read_bytes())
   self.assertFalse((p/'BitSkim.ini').exists())
   # NSIS and its generated uninstaller must both have a 32-bit PE stub.
   for file in (setup,p/'Uninstall.exe'):
    data=file.read_bytes(); offset=struct.unpack_from('<I',data,0x3c)[0]
    self.assertEqual(data[offset:offset+4],b'PE\0\0')
    self.assertEqual(struct.unpack_from('<H',data,offset+4)[0],0x14c)
