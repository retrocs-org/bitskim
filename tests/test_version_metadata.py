"""Keep shipped app and installer metadata aligned with the release version."""
from pathlib import Path
import plistlib,re,unittest
ROOT=Path(__file__).resolve().parents[1]
class Versions(unittest.TestCase):
 def test_app_and_installer_versions_match(self):
  version=(ROOT/'VERSION').read_text().strip()
  self.assertIn('#define BS_VERSION "'+version+'"',(ROOT/'core/bitskim.h').read_text())
  info=plistlib.loads((ROOT/'clients/macos/Info.plist').read_bytes())
  self.assertEqual(info['CFBundleShortVersionString'],version);self.assertEqual(info['CFBundleVersion'],version)
  resource=','.join((version+'.0.0').split('.'))
  for name in ('windows','windows-legacy'):
   with self.subTest(client=name):
    text=(ROOT/'clients'/name/'BitSkim.rc').read_text()
    self.assertIn('FILEVERSION '+resource,text);self.assertIn('PRODUCTVERSION '+resource,text)
    self.assertIn('"FileVersion", "'+version+'\\0"',text)
  for name in ('BitSkim.nsi','BitSkim-Classic.nsi'):
   with self.subTest(installer=name):
    text=(ROOT/'installers/windows'/name).read_text()
    self.assertIn('VIProductVersion "'+version+'.0.0"',text)
 def test_pairing_identifier_remains_compatible_with_first_release(self):
  # Both PSK callbacks must continue to recognize the original protocol token.
  source=(ROOT/'core/tls.h').read_text()
  self.assertEqual(re.findall(r'identity\[\]="([^"]+)"',source),['BitSkim-0.1','BitSkim-0.1'])
