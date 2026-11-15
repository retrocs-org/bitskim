"""Local installer deployment preserves the payload and unrelated guest files."""
import importlib.util,sys,tempfile,unittest,zipfile
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
spec=importlib.util.spec_from_file_location('builds_drawer',ROOT/'scripts/sync-builds.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class BuildsDrawer(unittest.TestCase):
 def test_deploys_installer_and_sibling_icon_with_packages_nested(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)/'source';out=root/'outputs';out.mkdir(parents=True);base=Path(d)/'Builds';base.mkdir();(base/'unrelated').write_text('keep')
   files={}
   for name in ['BitSkim.info','BitSkim/Install-BitSkim','BitSkim/Install-BitSkim.info']:
    path=root/'fixture'/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(name.encode());files[name]=path
   with zipfile.ZipFile(out/'BitSkim-0.2-AmigaOS.zip','w') as package:
    for name,path in files.items():package.write(path,name)
   with patch.object(module,'verify'),patch.object(module,'payload',return_value=files):module.sync(base,root)
   self.assertEqual((base/'BitSkim.info').read_bytes(),b'BitSkim.info');self.assertTrue((base/'BitSkim/Install-BitSkim.info').is_file());self.assertTrue((base/'BitSkim/packages/BitSkim-0.2-AmigaOS.zip').is_file());self.assertFalse((base/'BitSkim-0.2-AmigaOS.zip').exists());self.assertEqual((base/'unrelated').read_text(),'keep')
 def test_refuses_symbolic_link_destinations(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);base=root/'Builds';base.mkdir();outside=root/'outside';outside.mkdir();(base/'BitSkim').symlink_to(outside,target_is_directory=True)
   with self.assertRaisesRegex(ValueError,'symbolic'):module.sync(base,root)
