"""DOS release archives must include matching, complete corresponding source."""
import importlib.util,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
from zipfile import ZipFile
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import dos_release
class DOSRelease(unittest.TestCase):
 def test_corresponding_source_and_stale_source_refusal(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   names=('docs/TRANSFER-TOOLS.md','docs/DOS.md','clients/dos/MTCPSAMP.CFG','third_party/mtcp/COPYING','LICENSE','THIRD-PARTY-NOTICES.MD','assets/licenses/WATCOM-LICENSE','third_party/mtcp/REVISION')
   for name in names:
    target=root/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(('fixture '+name).encode())
   binary=root/'build/dos/BITSKIM.EXE';binary.parent.mkdir(parents=True);binary.write_bytes(b'MZ fixture')
   source=root/'source.zip'
   def write_source(fingerprint):
    with ZipFile(source,'w') as z:
     z.writestr('BitSkim/BUILD-PROVENANCE.json',json.dumps({'dos':{'source_sha256':fingerprint}}))
     z.write(root/'third_party/mtcp/REVISION','BitSkim/third_party/mtcp/REVISION')
     z.writestr('BitSkim/third_party/mtcp/src/test.cpp','corresponding source fixture')
   write_source('current')
   with patch.object(dos_release,'verify',return_value={'source_sha256':'current'}):
    output=dos_release.write_dos(root/'output.zip',source,root)
    with ZipFile(output) as z:
     self.assertEqual(z.read('BITSKIM/SOURCE.ZIP'),source.read_bytes())
     self.assertEqual(z.read('BITSKIM/BITSKIM.EXE'),binary.read_bytes())
    write_source('obsolete')
    self.assertRaisesRegex(RuntimeError,'Refresh the full source',dos_release.write_dos,root/'stale.zip',source,root)
