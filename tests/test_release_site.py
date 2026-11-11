"""Release website validation must reject broken links and tampered downloads."""
import importlib.util,hashlib,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('release_site',ROOT/'scripts/check-release-site.py');site=importlib.util.module_from_spec(spec);spec.loader.exec_module(site)
class ReleaseSite(unittest.TestCase):
 def test_download_integrity_and_missing_assets(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'downloads').mkdir();payload=root/'downloads/client.zip';payload.write_bytes(b'verified package')
   (root/'downloads/SHA256SUMS').write_text(hashlib.sha256(payload.read_bytes()).hexdigest()+'  client.zip\n')
   page='<link rel="canonical" href="https://bitskim.retrocs.org/"><a href="https://github.com/retrocs-org/bitskim">Source</a><a href="downloads/client.zip">Download</a>'
   (root/'index.html').write_text(page);self.assertEqual(site.validate(root),1)
   payload.write_bytes(b'tampered');self.assertRaisesRegex(RuntimeError,'Checksum mismatch',site.validate,root)
   payload.write_bytes(b'verified package');(root/'index.html').write_text(page+'<img src="missing.png" alt="App screenshot">')
   self.assertRaisesRegex(RuntimeError,'missing missing.png',site.validate,root)
 def test_obsolete_repository_and_missing_anchor(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'downloads').mkdir();(root/'downloads/SHA256SUMS').write_text('')
   (root/'index.html').write_text('<link rel="canonical" href="https://bitskim.retrocs.org/"><a href="https://github.com/kladhest-se/bitskim">Old</a><a href="#missing">Jump</a>')
   with self.assertRaises(RuntimeError) as failure:site.validate(root)
   self.assertIn('obsolete repository',str(failure.exception));self.assertIn('missing anchor',str(failure.exception))
