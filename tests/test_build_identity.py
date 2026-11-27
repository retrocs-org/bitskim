"""Build source identity is stable across checkout locations and changes with source."""
import ctypes,os,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from build_identity import write_identity
from build_provenance import source_hash
class BuildIdentity(unittest.TestCase):
 def test_independent_of_path_generated_outputs_and_git(self):
  with tempfile.TemporaryDirectory() as tmp:
   roots=[Path(tmp)/name for name in ('one','two')]
   for root in roots:(root/'core').mkdir(parents=True);(root/'VERSION').write_text('0.2');(root/'core/source.c').write_text('same source')
   before=[source_hash(root) for root in roots];headers=[write_identity(root).read_text() for root in roots]
   self.assertEqual(headers[0],headers[1]);self.assertEqual(before,[source_hash(root) for root in roots]);self.assertNotIn(str(roots[0]),headers[0])
   (roots[1]/'core/source.c').write_text('changed source');self.assertNotEqual(write_identity(roots[1]).read_text(),headers[0])
 def test_compiled_engine_returns_injected_identity(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=Path(tmp);header=write_identity(ROOT);target=root/'identity.so'
   subprocess.run(['cc','-shared','-fPIC','-std=c89','-include',str(header),'core/bitskim.c','core/platform.c','-o',str(target)],cwd=ROOT,check=True)
   lib=ctypes.CDLL(str(target));lib.bs_build_id.restype=ctypes.c_char_p
   self.assertEqual(lib.bs_build_id().decode(),source_hash(ROOT)[:12])
