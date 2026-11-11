"""Experimental downloads retain their binary, notices and release orchestration."""
import importlib.util
from pathlib import Path
import sys,tempfile,unittest,zipfile
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from windows_legacy_release import write_windows_legacy
spec=importlib.util.spec_from_file_location('release_experimental',ROOT/'scripts/release.py')
release=importlib.util.module_from_spec(spec);spec.loader.exec_module(release)
class ExperimentalPackages(unittest.TestCase):
 def test_legacy_archive_keeps_binary_and_notices(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp)
   files=['docs/TRANSFER-TOOLS.md','build/windows-legacy/BitSkim.exe','docs/WINDOWS-LEGACY.md','LICENSE','assets/themes/CATPPUCCIN-LICENSE','THIRD-PARTY-NOTICES.MD']
   for name in files:
    p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(name.encode())
   with patch('windows_legacy_release.verify',return_value={'target':'windows-legacy'}):
    output=write_windows_legacy(root/'client.zip',root)
   with zipfile.ZipFile(output) as z:
    self.assertEqual(z.read('BitSkim/BitSkim.exe'),b'build/windows-legacy/BitSkim.exe')
    self.assertIn('BitSkim/THIRD-PARTY-NOTICES.MD',z.namelist())
    self.assertIn('BitSkim/BUILD-PROVENANCE.json',z.namelist())
 def test_release_builds_experimental_clients_before_packaging(self):
  with patch.object(release,'run') as run:
   release.build({'env':{'BITSKIM_SIGN_IDENTITY':'test'},'notary_profile':'test'})
  commands=[args[0][0] for args in run.call_args_list]
  package=next(i for i,c in enumerate(commands) if 'scripts/package.py' in c)
  for script in ['./scripts/build-atari.sh','./scripts/build-windows-legacy.sh']:
   self.assertLess(commands.index([script]),package)
