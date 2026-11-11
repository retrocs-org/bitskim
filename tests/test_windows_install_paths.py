"""Installer scope and per-user INI location contracts, independent of runtime UI."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class WindowsInstallPaths(unittest.TestCase):
 def test_installers_default_to_program_files_and_machine_scope(self):
  for name,folder in [('BitSkim.nsi','$PROGRAMFILES64'),('BitSkim-Classic.nsi','$PROGRAMFILES')]:
   with self.subTest(name=name):
    s=(ROOT/'installers/windows'/name).read_text()
    self.assertIn('InstallDir "'+folder+'\\',s);self.assertIn('RequestExecutionLevel admin',s)
    self.assertIn('SetShellVarContext all',s);self.assertIn('WriteRegStr HKLM',s)
    self.assertNotIn('WriteRegStr HKCU',s);self.assertNotIn('Delete "$INSTDIR\\preferences.ini"',s)
 def test_ini_files_are_separate_from_installation(self):
  modern=(ROOT/'clients/windows/main.c').read_text();legacy=(ROOT/'clients/windows-legacy/main.c').read_text()
  self.assertIn('CSIDL_APPDATA|CSIDL_FLAG_CREATE',modern);self.assertIn('preferences.ini',modern)
  self.assertIn('SHGetSpecialFolderLocation(NULL,CSIDL_APPDATA',legacy);self.assertIn('preferences.ini',legacy)
  self.assertIn('CopyFileA(old_ini,ini,TRUE)',legacy)
