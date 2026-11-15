"""Offline release-policy regression tests; never contact a publishing service."""
import io,json,os,subprocess,sys,tempfile,unittest
from datetime import datetime,timezone
from pathlib import Path
from unittest.mock import patch
import test_release_tool as fixtures
release=fixtures.release
FakeGitHub=fixtures.FakeGitHub
FakeGitLab=fixtures.FakeGitLab
import aminet_release
from release_notes import validate_notes,version_changes,plain
from release_paths import output_dir,select_output
from release_security import scan
import build_provenance
ROOT=Path(__file__).resolve().parents[1]

class ReleaseGuidelines(unittest.TestCase):
 def fixture(self,directory):return fixtures.ReleaseTool().setup_release(directory)
 def test_github_normal_latest_only_after_verified_assets(self):
  with tempfile.TemporaryDirectory() as d:
   root,manifest=self.fixture(d);api=FakeGitHub();release.publish_repository('github',manifest,api,root)
   self.assertFalse(api.release['prerelease']);self.assertFalse(api.release['draft']);self.assertEqual(api.release['make_latest'],'true')
   api.assets['extra.zip']={'name':'extra.zip','digest':'sha256:unknown','size':1};before=api.posts
   with self.assertRaisesRegex(ValueError,'Unexpected'):release.publish_repository('github',manifest,api,root)
   self.assertEqual(api.posts,before)
 def test_public_release_missing_assets_is_never_filled(self):
  with tempfile.TemporaryDirectory() as d:
   root,manifest=self.fixture(d);api=FakeGitHub();release.publish_repository('github',manifest,api,root);del api.assets['second.zip'];before=api.posts
   with self.assertRaisesRegex(ValueError,'Incomplete public'):release.publish_repository('github',manifest,api,root)
   self.assertEqual(api.posts,before)
 def test_unknown_or_wrong_asset_size_is_refused(self):
  with tempfile.TemporaryDirectory() as d:
   root,manifest=self.fixture(d);api=FakeGitHub();release.publish_repository('github',manifest,api,root);api.assets['first.zip']['size']=0
   with self.assertRaisesRegex(ValueError,'size'):release.publish_repository('github',manifest,api,root)
 def test_preflight_never_creates_releases_or_packages(self):
  with tempfile.TemporaryDirectory() as d:
   root,manifest=self.fixture(d)
   for name,api in [('github',FakeGitHub()),('gitlab',FakeGitLab())]:
    release.publish_repository(name,manifest,api,root,preflight=True);self.assertIsNone(api.release)
    self.assertEqual(getattr(api,'posts',getattr(api,'puts',None)),0)
 def test_gitlab_is_current_normal_and_latest(self):
  with tempfile.TemporaryDirectory() as d:
   root,manifest=self.fixture(d);api=FakeGitLab();release.publish_repository('gitlab',manifest,api,root)
   self.assertLessEqual(datetime.fromisoformat(api.release['released_at']),datetime.now(timezone.utc))
   self.assertEqual(api.release['name'],manifest['tag'])
 def test_notes_keep_complete_current_changes_and_reject_placeholders(self):
  notes=(ROOT/'docs/RELEASE-NOTES.md').read_text().strip();self.assertEqual(validate_notes(notes,ROOT),notes)
  for value in [notes.replace('## Features','## Overview',1),notes.replace('## Installation','## Installation\n\nTODO',1),notes.rsplit('\n- ',1)[0]]:
   with self.assertRaises(ValueError):validate_notes(value,ROOT)
 def test_aminet_has_all_version_changes_and_no_older_history(self):
  text=aminet_release.readme('uploader@example.invalid');flat=' '.join(text.split())
  self.assertTrue(all(len(line)<=76 for line in text.splitlines()))
  self.assertIn('Distribution: Aminet',flat);self.assertIn('VERSION CHANGELOG',flat)
  for values in version_changes(ROOT).values():
   for item in values:self.assertIn(' '.join(plain(item).split()),flat)
  self.assertNotIn('0.1 - Initial release',text)
 def test_aminet_existing_incoming_files_block_before_receipt_or_stor(self):
  from test_aminet_release import AminetRelease
  for listing in [['/new/bitskim.lha'],['bitskim.readme'],None]:
   with self.subTest(listing=listing),tempfile.TemporaryDirectory() as d,patch.object(aminet_release,'verify',return_value={'source_sha256':'source'}):
    folder=Path(d);AminetRelease().bundle(folder);writes=[]
    class FTP:
     def __enter__(self):return self
     def __exit__(self,*args):pass
     def connect(self,*args,**kwargs):pass
     def login(self,*args):pass
     def cwd(self,*args):pass
     def nlst(self):
      if listing is None:raise OSError('unreadable')
      return listing
     def storbinary(self,*args):writes.append(args)
    with self.assertRaises((ValueError,RuntimeError)):aminet_release.upload(folder,True,ftp_factory=FTP)
    self.assertEqual(writes,[]);self.assertFalse((folder/'upload-receipt.json').exists())
 def test_output_is_confined_and_provenance_uses_selected_installer(self):
  with tempfile.TemporaryDirectory() as d,patch.dict(os.environ,{},clear=False):
   os.environ.pop('BITSKIM_RELEASE_OUTPUT',None);root=Path(d);(root/'VERSION').write_text('0.2')
   for value in ['../elsewhere','outputs/release','outputs/release/nested','/tmp/other-output']:
    with self.assertRaises(ValueError):select_output(root,value)
   out=select_output(root,'outputs/release-0.2');self.assertEqual(output_dir(root),out)
   build_provenance.begin('windows',root)
   for name in build_provenance.ARTIFACTS['windows']:
    file=root/name;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(b'executable fixture')
   build_provenance.finish('windows',root);build_provenance.begin('setup',root);out.mkdir(parents=True)
   (out/'BitSkim-0.2-Windows10-x64-Setup.exe').write_bytes(b'installer fixture');record=build_provenance.finish('setup',root)
   self.assertIn('outputs/release-0.2/BitSkim-0.2-Windows10-x64-Setup.exe',record['artifacts']);build_provenance.verify('setup',root)
 def test_build_log_redacts_values_and_reports_child_failures(self):
  secret='test-value-'+('x'*35);log=io.StringIO()
  release.run([sys.executable,'-c','print('+repr(secret)+')'],log=log,secrets=[secret])
  self.assertNotIn(secret,log.getvalue());self.assertIn('REDACTED',log.getvalue())
  with self.assertRaises(subprocess.CalledProcessError):release.run([sys.executable,'-c','raise SystemExit(7)'],log=log)
 def test_common_secret_is_refused_without_value_in_error(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'core').mkdir();secret='gh'+'p_'+('x'*40);(root/'core/leaked.txt').write_text(secret)
   with self.assertRaisesRegex(ValueError,'Possible secret') as result:scan(root)
   self.assertNotIn(secret,str(result.exception))
 def test_1password_cannot_combine_reference_and_named_selector(self):
  with self.assertRaisesRegex(ValueError,'not both'):release.credential('github',{'credentials':{'github':{'provider':'1password','reference':'op://v/i/f','vault':'v'}}})
 def test_app_installation_access_checks_repository_id(self):
  class API:
   def request(self,method,path):return {'id':42,'full_name':'org/app'} if path.startswith('/repos/') else {'repositories':[{'id':42,'full_name':'org/app'}]}
  release.check_repository_access('github','org/app',API(),'gh'+'s_'+'test')
  with self.assertRaises(ValueError):release.check_repository_access('github','another/app',API(),'gh'+'s_'+'test')
 def test_existing_published_aminet_uses_tagged_changes_and_identical_bytes(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   def git(*args):return subprocess.check_output(['git',*args],cwd=root,text=True).strip()
   git('init','-q');git('config','user.name','Test');git('config','user.email','test@example.invalid')
   (root/'CHANGELOG.MD').write_text('# Changelog\n\n## 0.1\n\n- Initial verified feature.\n');git('add','.');git('commit','-qm','fixture');commit=git('rev-parse','HEAD')
   state=root/'outputs/release';stage=state/'publish/outputs';stage.mkdir(parents=True);archive=stage/'BitSkim-0.1.lha';archive.write_bytes(b'published archive bytes')
   manifest={'version':'0.1','tag':'0.1','commit':commit,'source_sha256':'original','files':{archive.name:release.digest(archive)}};path=state/'manifest.json';path.write_text(json.dumps(manifest));(state/'receipt.json').write_text(json.dumps({'manifest_sha256':release.digest(path),'completed':{'github':'published'}}))
   (root/'CHANGELOG.MD').write_text('Uncommitted later changes must not enter the published submission')
   folder=release.prepare_aminet({'aminet':True,'uploader':'sender@example.invalid'},root,tag='0.1')
   self.assertEqual((folder/'bitskim.lha').read_bytes(),archive.read_bytes());self.assertIn('Initial verified feature.',(folder/'bitskim.readme').read_text());self.assertNotIn('Uncommitted',(folder/'bitskim.readme').read_text())
   self.assertEqual(json.loads(path.read_text()),manifest)
 def test_annotated_tag_object_mismatch_is_rejected_at_same_commit(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   def git(*args):return subprocess.check_output(['git',*args],cwd=root,text=True).strip()
   git('init','-q');git('config','user.name','Test');git('config','user.email','test@example.invalid');(root/'VERSION').write_text('0.2');git('add','.');git('commit','-qm','fixture');git('tag','-a','0.2','-m','BitSkim 0.2');commit=git('rev-parse','HEAD')
   manifest={'tag':'0.2','commit':commit};wrong={'object':'b'*40,'commit':commit,'annotated':True}
   with patch.object(release,'git_destination',return_value='unused'),patch.object(release,'remote_tag_details',return_value=wrong):
    with self.assertRaisesRegex(ValueError,'objects differ'):release.validate_tag_identity(manifest,['github','gitlab'],root)
 def test_two_repository_publication_and_resume_preserve_tag_object(self):
  with tempfile.TemporaryDirectory() as d:
   base=Path(d);root=base/'source';root.mkdir()
   def git(*args):return subprocess.check_output(['git',*args],cwd=root,text=True,stderr=subprocess.DEVNULL).strip()
   git('init','-q');git('config','user.name','Test');git('config','user.email','test@example.invalid');(root/'VERSION').write_text('0.2');(root/'.gitignore').write_text('outputs/\n');git('add','.');git('commit','-qm','fixture');commit=git('rev-parse','HEAD')
   remotes={}
   for service in ('github','gitlab'):
    remote=base/(service+'.git');subprocess.run(['git','init','--bare','-q',str(remote)],check=True);remotes[service]=str(remote)
   out=root/'outputs';out.mkdir();file=out/'client.zip';file.write_bytes(b'reviewed bytes');stage=out/'release/publish/outputs';stage.mkdir(parents=True);(stage/file.name).write_bytes(file.read_bytes());files={file.name:release.digest(file)}
   manifest={'version':'0.2','tag':'0.2','commit':commit,'tag_message':'BitSkim 0.2','source_sha256':release.source_hash(root),'github':'https://github.com/team/app','gitlab':'https://gitlab.example.com/group/nested/app','notes':'reviewed','files':files,'inventory':files}
   path=out/'release/manifest.json';path.write_text(json.dumps(manifest))
   class GH(FakeGitHub):
    def request(self,method,path,*args,**kwargs):
     if method=='GET' and path=='/repos/team/app':return {'id':42,'full_name':'team/app'}
     return super().request(method,path,*args,**kwargs)
   class GL(FakeGitLab):
    def request(self,method,path,*args,**kwargs):
     if method=='GET' and path.endswith('/projects/group%2Fnested%2Fapp'):return {'path_with_namespace':'group/nested/app'}
     return super().request(method,path,*args,**kwargs)
   github=GH();gitlab=GL()
   with patch.object(release,'check_builds'),patch.object(release,'check_source_archive'),patch.object(release,'validate_notes'),patch.object(release,'inventory',return_value=files),patch.object(release,'credential',return_value='offline-test'),patch.object(release,'git_destination',side_effect=lambda config,service:remotes[service]),patch.object(release,'API',side_effect=lambda host,*args:github if host=='api.github.com' else gitlab):
    release.publish(['gitlab','github'],True,root,tag='0.2');receipt=json.loads((path.parent/'receipt.json').read_text())
    self.assertEqual(set(receipt['completed']),{'github','gitlab'})
    for destination in remotes.values():self.assertEqual(release.remote_tag_details(destination,'0.2',root)['object'],receipt['tag_object'])
    before=github.posts;release.publish(['gitlab','github'],True,root,tag='0.2');self.assertEqual(github.posts,before)
