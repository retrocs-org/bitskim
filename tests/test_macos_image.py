"""Check real mounted/copied signed bundles and reject forbidden Finder metadata."""
import importlib.util
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('macos_image',ROOT/'scripts/check-macos-dmg.py');image=importlib.util.module_from_spec(spec);spec.loader.exec_module(image)

@unittest.skipUnless(sys.platform=='darwin','Native Mac disk image tools required')
class MacImage(unittest.TestCase):
 def test_mounted_copied_signature_and_invalid_finder_metadata(self):
  with tempfile.TemporaryDirectory(prefix='bitskim-image-regression-') as temporary:
   root=Path(temporary);stage=root/'stage';bundle=stage/'BitSkim.app';(bundle/'Contents/MacOS').mkdir(parents=True)
   (stage/'Applications').symlink_to('/Applications');(root/'main.c').write_text('int main(void){return 0;}\n')
   subprocess.run(['clang',str(root/'main.c'),'-o',str(bundle/'Contents/MacOS/BitSkim')],check=True,capture_output=True)
   (bundle/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'org.bitskim.client','CFBundleExecutable':'BitSkim','CFBundleShortVersionString':'0.2','CFBundlePackageType':'APPL'}))
   subprocess.run(['codesign','--sign','-','--force',str(bundle)],check=True,capture_output=True)
   def make(name):
    path=root/name
    subprocess.run(['hdiutil','create','-fs','APFS','-srcfolder',str(stage),'-volname','BitSkim test','-format','UDZO',str(path)],check=True,capture_output=True);return path
   good=make('good.dmg');report=root/'good-report';self.assertEqual(image.check_image(good,report)['result'],'passed')
   metadata=bytearray(32);metadata[10:14]=b'\xff'*4
   subprocess.run(['xattr','-wx','com.apple.FinderInfo',metadata.hex(),str(bundle/'Contents/MacOS/BitSkim')],check=True,capture_output=True)
   bad=make('bad.dmg')
   with self.assertRaisesRegex(RuntimeError,'resource fork|Finder information'):image.check_image(bad,root/'bad-report')
   self.assertEqual(json.loads((root/'bad-report/package-review.json').read_text())['result'],'failed')
