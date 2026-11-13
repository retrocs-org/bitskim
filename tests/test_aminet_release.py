"""Validate Aminet metadata and exercise uploads only with a simulated FTP client."""
import hashlib,importlib.util,json,sys,tempfile,unittest,subprocess
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
import aminet_release as a
class AminetRelease(unittest.TestCase):
 def test_readme_format_required_fields_and_version(self):
  text=a.readme('sender@example.org (Uploader)')
  self.assertTrue(text.startswith('Short:'));self.assertNotIn('\r',text)
  self.assertTrue(all(len(line)<=78 for line in text.splitlines()))
  fields=dict(line.split(':',1) for line in text.split('\n\n')[0].splitlines())
  for name in ('Short','Uploader','Type','Architecture'):self.assertIn(name,fields)
  self.assertLessEqual(len(fields['Short'].strip()),40)
  self.assertEqual(fields['Version'].strip(),'0.2');self.assertEqual(fields['Type'].strip(),'comm/tcp')
  self.assertNotIn('Distribution:',text)
 def test_missing_contact_and_header_injection_are_rejected(self):
  for value in (None,'','not-an-email','a@example.org\nType: bad','a@example.org\x00'):
   with self.assertRaises(ValueError):a.readme(value)
  with self.assertRaises(ValueError):a.readme('a@example.org','Author\nOther: injected')
 def bundle(self,folder):
  (folder/a.ARCHIVE).write_bytes(b'archive fixture')
  (folder/a.README).write_text(a.readme('sender@example.org'),encoding='ascii')
  manifest={'version':'0.2','host':a.HOST,'directory':'/new','uploader':'sender@example.org','category':a.CATEGORY,'source_sha256':'source','files':{name:a.digest(folder/name) for name in (a.ARCHIVE,a.README)}}
  (folder/'submission.json').write_text(json.dumps(manifest))
 def test_confirmation_required_before_any_connection(self):
  def forbidden():self.fail('Unexpected FTP connection')
  with tempfile.TemporaryDirectory() as d:
   with self.assertRaisesRegex(ValueError,'confirm-upload'):a.upload(Path(d),ftp_factory=forbidden)
 def test_upload_only_sends_pair_in_order_and_records_pending_review(self):
  with tempfile.TemporaryDirectory() as d,patch.object(a,'verify',return_value={'source_sha256':'source'}):
   folder=Path(d);self.bundle(folder);calls=[]
   class FTP:
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def connect(self,*args,**kwargs):calls.append(('connect',args))
    def login(self,*args):calls.append(('login',args))
    def cwd(self,path):calls.append(('cwd',path))
    def storbinary(self,command,stream):calls.append((command,stream.read()))
   a.upload(folder,True,ftp_factory=FTP)
   self.assertEqual([c[0] for c in calls if c[0].startswith('STOR ')],['STOR bitskim.lha','STOR bitskim.readme'])
   self.assertEqual(calls[0][1],(a.HOST,21));self.assertEqual(calls[1][1],('anonymous','sender@example.org'))
   self.assertEqual(json.loads((folder/'upload-receipt.json').read_text())['status'],'transferred-awaiting-aminet-review')
   with self.assertRaisesRegex(ValueError,'receipt'):a.upload(folder,True,ftp_factory=FTP)
 def test_tampering_and_changed_source_fail_before_upload(self):
  with tempfile.TemporaryDirectory() as d,patch.object(a,'verify',return_value={'source_sha256':'source'}):
   folder=Path(d);self.bundle(folder);(folder/a.ARCHIVE).write_bytes(b'changed')
   with self.assertRaisesRegex(ValueError,'changed'):a.check(folder)
   self.bundle(folder)
   with patch.object(a,'verify',return_value={'source_sha256':'different'}):
    with self.assertRaisesRegex(ValueError,'different source'):a.check(folder)
 def test_partial_transfer_is_recorded_without_automatic_retry(self):
  with tempfile.TemporaryDirectory() as d,patch.object(a,'verify',return_value={'source_sha256':'source'}):
   folder=Path(d);self.bundle(folder)
   class FTP:
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def connect(self,*args,**kwargs):pass
    def login(self,*args):pass
    def cwd(self,*args):pass
    def storbinary(self,command,stream):
     if command.endswith('.readme'):raise OSError('simulated disconnect')
   with self.assertRaisesRegex(RuntimeError,'partial'):a.upload(folder,True,ftp_factory=FTP)
   receipt=json.loads((folder/'upload-receipt.json').read_text())
   self.assertEqual(receipt['completed'],['bitskim.lha']);self.assertEqual(receipt['status'],'failed-may-be-partial')
 def test_prepare_checks_extracted_payload_and_builds_matching_pair(self):
  import amiga_release
  try:tool=amiga_release.writer()
  except RuntimeError:self.skipTest('No LHA writer available for extraction integration test')
  with tempfile.TemporaryDirectory() as d,patch.object(a,'verify',return_value={'source_sha256':'source'}):
   root=Path(d);out=root/'outputs';out.mkdir();stage=root/'stage';(stage/'BitSkim').mkdir(parents=True)
   file=stage/'BitSkim/demo';file.write_bytes(b'fixture payload')
   archive=out/'BitSkim-0.2.lha'
   subprocess.run([tool,'ao5g0q2',str(archive),'BitSkim/demo'],cwd=stage,check=True)
   (out/'BitSkim-0.2.lha.sha256').write_text(a.digest(archive)+'  '+archive.name+'\n')
   with patch.object(amiga_release,'payload',return_value={'BitSkim/demo':file}):
    folder=a.prepare('sender@example.org',root=root)
    self.assertEqual((folder/a.ARCHIVE).read_bytes(),archive.read_bytes())
    self.assertEqual(a.check(folder,root)['source_sha256'],'source')
    self.assertFalse((folder/'upload-receipt.json').exists())
    file.write_bytes(b'newer build payload')
    with self.assertRaisesRegex(ValueError,'does not match'):a.prepare('sender@example.org',root=root)
 def test_template_is_distinct_from_submittable_readme(self):
  with tempfile.TemporaryDirectory() as d:
   path=a.template(Path(d));self.assertEqual(path.suffix,'.template');self.assertIn('<YOUR REAL EMAIL REQUIRED>',path.read_text())
