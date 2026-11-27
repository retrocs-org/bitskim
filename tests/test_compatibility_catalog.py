"""The maintained compatibility inventory must render safely and stay in sync."""
import importlib.util,json,subprocess,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('compatibility_sync',ROOT/'scripts/sync-compatibility.py');sync=importlib.util.module_from_spec(spec);spec.loader.exec_module(sync)
class CompatibilityCatalog(unittest.TestCase):
 def test_generated_copies_match_current_catalog(self):
  for path,text in sync.outputs().items():
   with self.subTest(path=path.name):self.assertEqual(path.read_text(),text)
 def test_public_table_contains_every_client_and_limit(self):
  output=subprocess.check_output(['php','-r','require "public-web/includes/compatibility.php";bitskim_compatibility_table();'],cwd=ROOT,text=True)
  catalog=json.loads((ROOT/'docs/compatibility.json').read_text())
  self.assertIn('Still unverified',output)
  for row in catalog['clients']:
   self.assertIn(row['name'],output);self.assertIn(row['pending'],output)
