"""Reject source/artifact mismatches, interrupted builds and stale installer payloads."""
import importlib.util,json,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('provenance',ROOT/'scripts/build_provenance.py');p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
class BuildProvenance(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
  (self.root/'VERSION').write_text('0.1\n');(self.root/'core').mkdir();(self.root/'core/engine.c').write_text('original')
  for name in p.ARTIFACTS['windows']+p.ARTIFACTS['setup']:
   path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('payload')
 def tearDown(self):self.tmp.cleanup()
 def build(self,target):p.begin(target,self.root);p.finish(target,self.root)
 def test_no_stamp_does_not_accept_existing_binary(self):
  with self.assertRaises(p.StaleBuild):p.verify('windows',self.root)
 def test_source_change_and_midbuild_change_refuse(self):
  self.build('windows');p.verify('windows',self.root)
  (self.root/'core/engine.c').write_text('new code')
  with self.assertRaises(p.StaleBuild):p.verify('windows',self.root)
  p.begin('windows',self.root);(self.root/'core/engine.c').write_text('changed while compiling')
  with self.assertRaises(p.StaleBuild):p.finish('windows',self.root)
 def test_modified_payload_and_changed_installer_dependency(self):
  self.build('windows');self.build('setup')
  (self.root/p.ARTIFACTS['windows'][0]).write_text('different executable')
  with self.assertRaises(p.StaleBuild):p.verify('windows',self.root)
  self.build('windows')
  with self.assertRaises(p.StaleBuild):p.verify('setup',self.root)
 def test_ignored_outputs_do_not_invalidate_source(self):
  self.build('windows');(self.root/'public-web/downloads').mkdir(parents=True)
  (self.root/'public-web/downloads/generated.zip').write_bytes(b'generated')
  p.verify('windows',self.root)
 def test_restarted_build_invalidates_prior_success(self):
  self.build('windows');p.begin('windows',self.root)
  with self.assertRaises(p.StaleBuild):p.verify('windows',self.root)
