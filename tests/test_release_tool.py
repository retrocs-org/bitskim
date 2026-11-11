"""Offline release publication tests: no real service or credential is used."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('release_tool', ROOT / 'scripts/release.py')
release = importlib.util.module_from_spec(spec); spec.loader.exec_module(release)


class FakeGitHub:
    def __init__(self):
        self.release = None; self.assets = {}; self.fail = None; self.posts = 0
    def request(self, method, path, body=None, file=None, **kwargs):
        if method == 'GET' and '/releases/tags/' in path: return self.release
        if method == 'GET' and '/assets?' in path: return list(self.assets.values())
        if method == 'POST' and file:
            self.posts += 1
            if file.name == self.fail: raise RuntimeError('interrupted upload')
            result = {'name': file.name, 'digest': 'sha256:' + release.digest(file)}
            self.assets[file.name] = result; return result
        if method == 'POST':
            self.release = dict(body, id=42, html_url='https://github.com/team/app/releases/tag/v0.1',
                                upload_url='https://uploads.github.com/repos/team/app/releases/42/assets{?name,label}')
            return self.release
        if method == 'PATCH': self.release.update(body); return self.release
        raise AssertionError((method, path))


class FakeGitLab:
    host = 'gitlab.example.com'
    def __init__(self): self.packages = {}; self.release = None; self.puts = 0
    def request(self, method, path, body=None, file=None, **kwargs):
        if '/packages/' in path:
            if method == 'GET': return self.packages.get(path)
            if method == 'PUT':
                self.puts += 1; self.packages[path] = release.digest(file); return {}
        if method == 'GET': return self.release
        if method == 'POST':
            self.release = body; return body
        raise AssertionError((method, path))


class ReleaseTool(unittest.TestCase):
    def setup_release(self, folder):
        root = Path(folder); (root / 'outputs').mkdir()
        for name in ('first.zip', 'second.zip'): (root / 'outputs' / name).write_bytes(name.encode())
        return root, {'version': '0.1', 'tag': 'v0.1', 'commit': 'c' * 40,
                      'github': 'https://github.com/team/app', 'gitlab': 'https://gitlab.example.com/group/nested/app',
                      'notes': 'Reviewed release notes',
                      'files': {p.name: release.digest(p) for p in (root / 'outputs').iterdir()}}

    def test_github_partial_retry_keeps_draft_until_complete(self):
        with tempfile.TemporaryDirectory() as folder:
            root, manifest = self.setup_release(folder); api = FakeGitHub(); api.fail = 'second.zip'
            with self.assertRaisesRegex(RuntimeError, 'interrupted'):
                release.publish_repository('github', manifest, api, root)
            self.assertTrue(api.release['draft']); self.assertEqual(len(api.assets), 1)
            api.fail = None
            release.publish_repository('github', manifest, api, root)
            self.assertFalse(api.release['draft']); self.assertEqual(api.posts, 3)
            release.publish_repository('github', manifest, api, root)
            self.assertEqual(api.posts, 3)

    def test_github_refuses_different_existing_asset(self):
        with tempfile.TemporaryDirectory() as folder:
            root, manifest = self.setup_release(folder); api = FakeGitHub()
            release.publish_repository('github', manifest, api, root)
            api.assets['first.zip']['digest'] = 'sha256:wrong'
            with self.assertRaisesRegex(ValueError, 'digest'):
                release.publish_repository('github', manifest, api, root)

    def test_gitlab_packages_verified_and_reused(self):
        with tempfile.TemporaryDirectory() as folder:
            root, manifest = self.setup_release(folder); api = FakeGitLab()
            release.publish_repository('gitlab', manifest, api, root)
            self.assertEqual(api.puts, 2)
            self.assertIn('group%2Fnested%2Fapp', next(iter(api.packages)))
            release.publish_repository('gitlab', manifest, api, root)
            self.assertEqual(api.puts, 2)
            api.packages[next(iter(api.packages))] = 'wrong'
            with self.assertRaisesRegex(ValueError, 'different bytes'):
                release.publish_repository('gitlab', manifest, api, root)

    def test_missing_confirmation_never_reads_or_uploads(self):
        with patch.object(release, 'API') as api:
            with self.assertRaisesRegex(ValueError, 'No upload'):
                release.publish(['github'])
            api.assert_not_called()

    def test_destination_rejects_credentials_and_mismatched_push(self):
        for url in ['http://github.com/team/app', 'https://secret@github.com/team/app',
                    'https://github.com/team/app?token=secret', 'https://github.com/team/../app']:
            with self.assertRaises(ValueError): release.repository(url, True)
        config = {'gitlab': 'https://gitlab.example.com/team/app', 'gitlab_git': 'git@other.example:team/app.git'}
        with self.assertRaises(ValueError): release.git_destination(config, 'gitlab')
        config['gitlab_git'] = 'git@gitlab.example.com:team/app.git'
        self.assertEqual(release.git_destination(config, 'gitlab'), config['gitlab_git'])

    def test_clean_commit_refuses_dirty_and_ignored_build_inputs(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            def git(*args):
                return subprocess.run(['git', *args], cwd=root, check=True, capture_output=True, text=True).stdout.strip()
            git('init', '-q'); git('config', 'user.email', 'test@example.invalid'); git('config', 'user.name', 'Release Test')
            (root / 'VERSION').write_text('0.1\n'); (root / '.gitignore').write_text('core/private.c\n')
            git('add', '.'); git('commit', '-qm', 'source')
            self.assertEqual(release.clean_commit(root), git('rev-parse', 'HEAD'))
            (root / 'VERSION').write_text('0.2\n')
            with self.assertRaisesRegex(ValueError, 'Commit all'): release.clean_commit(root)
            git('checkout', '--', 'VERSION'); (root / 'core').mkdir(); (root / 'core/private.c').write_text('uncommitted input')
            with self.assertRaisesRegex(ValueError, 'not committed'): release.clean_commit(root)

    def test_remote_annotated_tag_uses_peeled_commit(self):
        with patch.object(release, 'run', return_value='object refs/tags/v0.1\ncommit refs/tags/v0.1^{}'):
            self.assertEqual(release.remote_tag('https://gitlab.example.com/a/b', 'v0.1'), 'commit')

    def test_plan_has_no_build_or_upload_side_effects(self):
        config = dict(release.DEFAULTS, env={'BITSKIM_SIGN_IDENTITY': 'Test Identity'})
        with patch.object(release, 'run') as run:
            release.build(config, plan=True)
            run.assert_not_called()
        config['env']['GITHUB_TOKEN'] = 'not permitted in a config'
        with self.assertRaisesRegex(ValueError, 'tokens belong'): release.build(config, plan=True)

    def test_remote_tag_conflict_blocks_every_push(self):
        with tempfile.TemporaryDirectory() as folder:
            root, manifest = self.setup_release(folder); manifest['source_sha256'] = 'hash'
            path = root / 'outputs/release/manifest.json'; path.parent.mkdir(); path.write_text(json.dumps(manifest))
            with patch.object(release, 'clean_commit', return_value=manifest['commit']), \
                 patch.object(release, 'source_hash', return_value='hash'), \
                 patch.object(release, 'check_builds'), \
                 patch.object(release, 'check_source_archive'), \
                 patch.object(release, 'inventory', return_value=manifest['files']), \
                 patch.object(release, 'remote_tag', return_value='wrong'), \
                 patch.object(release, 'API'), \
                 patch.dict('os.environ', {'GITHUB_TOKEN': 'offline-test'}), \
                 patch.object(release, 'run') as run:
                with self.assertRaisesRegex(ValueError, 'different commit'):
                    release.publish(['github'], True, root)
                run.assert_not_called()
            self.assertFalse((path.parent / 'receipt.json').exists())


if __name__ == '__main__': unittest.main()

class IntegratedSettings(unittest.TestCase):
    def test_1password_reference_and_account(self):
        spec={'credentials':{'gitlab':{'provider':'1password','reference':'op://v/i/f','account':'account-id'}}}
        with patch.object(release.subprocess,'run',return_value=subprocess.CompletedProcess([],0,'secret','')) as command:
            self.assertEqual(release.credential('gitlab',spec),'secret')
            self.assertEqual(command.call_args.args[0],['op','read','--no-newline','op://v/i/f','--account','account-id'])
    def test_named_field_with_slashes(self):
        spec={'credentials':{'github':{'provider':'1password','vault':'TestVault','item':'github.com','field':'example-org/project-token'}}}
        with patch.object(release.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps([{'label':'example-org/project-token','value':'secret'}]),'')):
            self.assertEqual(release.credential('github',spec),'secret')
    def test_cancelled_secret_lookup_does_not_expose_output(self):
        with patch.object(release.subprocess,'run',return_value=subprocess.CompletedProcess([],1,'secret','private diagnostic')):
            with self.assertRaisesRegex(ValueError,'lookup failed') as caught:release.credential('gitlab',{'credentials':{'gitlab':{'provider':'1password','reference':'op://v/i/f'}}})
            self.assertNotIn('secret',str(caught.exception).replace('no secret output displayed',''))
    def test_same_directory_with_different_spelling(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'repo';root.mkdir();alias=Path(tmp)/'alias';alias.symlink_to(root,target_is_directory=True)
            def git(*args):subprocess.run(['git',*args],cwd=root,check=True,capture_output=True)
            git('init','-q');git('config','user.email','test@example.invalid');git('config','user.name','Test')
            (root/'VERSION').write_text('0.1');git('add','.');git('commit','-qm','test')
            self.assertEqual(release.clean_commit(alias),release.clean_commit(root))
    def test_source_confirmation_before_git(self):
        with patch.object(release,'clean_commit') as clean:
            with self.assertRaisesRegex(ValueError,'confirm-push'):release.push_source({},['gitlab'])
            clean.assert_not_called()
    def test_manual_tag_exact(self):self.assertEqual(release.release_tag('0.1'),'0.1')
    def test_publication_standard_excludes_source_and_website(self):
        files,sidecars=release.publication_assets(Path('.'),{'BitSkim-0.1-macOS-arm64.dmg':'a'*64,'BitSkim-0.1-source.zip':'b'*64,'BitSkim-0.1-website.zip':'c'*64,'SHA256SUMS':'d'*64})
        self.assertEqual(set(files),{'BitSkim-0.1-macOS-arm64.dmg','BitSkim-0.1-macOS-arm64.dmg.sha256'})
        self.assertEqual(sidecars['BitSkim-0.1-macOS-arm64.dmg.sha256'],('a'*64+'  BitSkim-0.1-macOS-arm64.dmg\n').encode())

class SourceOnlyWorkflow(unittest.TestCase):
    def test_source_dry_run_does_not_push_or_resolve_tokens(self):
        config={'gitlab':'https://gitlab.example.com/team/app','gitlab_git':'git@gitlab.example.com:team/app.git','source_branch':''}
        def response(command,*args,**kwargs):
            if command[1:3]==['symbolic-ref','--quiet']:return 'main'
            return ''
        with patch.object(release,'clean_commit',return_value='a'*40),patch.object(release,'run',side_effect=response) as run,patch.object(release,'credential') as credential:
            release.push_source(config,['gitlab'],dry_run=True)
            pushes=[c.args[0] for c in run.call_args_list if c.args[0][1]=='push']
            self.assertEqual(len(pushes),1);self.assertIn('--dry-run',pushes[0]);self.assertIn('--no-follow-tags',pushes[0]);credential.assert_not_called()
    def test_missing_configuration_reports_external_path(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.assertRaisesRegex(ValueError,'Create release settings'):release.configuration(Path(folder)/'bitskim.json')

class InitialSourceCommit(unittest.TestCase):
    def test_init_dry_run_creates_commit_but_keeps_ignored_files_and_remote_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)/'source';root.mkdir();remote=Path(tmp)/'remote.git'
            subprocess.run(['git','init','--bare','-q',str(remote)],check=True)
            (root/'.gitignore').write_text('private.txt\n')
            (root/'VERSION').write_text('0.1')
            (root/'private.txt').write_text('not-for-git')
            identity={'GIT_AUTHOR_NAME':'Release Test','GIT_AUTHOR_EMAIL':'test@example.invalid','GIT_COMMITTER_NAME':'Release Test','GIT_COMMITTER_EMAIL':'test@example.invalid'}
            with patch.dict('os.environ',identity),patch.object(release,'git_destination',return_value=str(remote)):
                release.push_source({},['gitlab'],dry_run=True,root=root,initialize=True)
                first=release.clean_commit(root)
                release.initialize_source({},['gitlab'],root)
                self.assertEqual(release.clean_commit(root),first)
            tracked=subprocess.run(['git','ls-files'],cwd=root,capture_output=True,text=True,check=True).stdout
            self.assertNotIn('private.txt',tracked)
            refs=subprocess.run(['git','--git-dir',str(remote),'show-ref'],capture_output=True,text=True).stdout
            self.assertEqual(refs,'')
    def test_nonempty_remote_blocks_before_init(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.gitignore').write_text('work/\n')
            with patch.object(release,'git_destination',return_value='destination'),patch.object(release,'run',return_value='commit refs/heads/main'):
                with self.assertRaisesRegex(ValueError,'already has Git history'):release.initialize_source({},['gitlab'],root)
            self.assertFalse((root/'.git').exists())
    def test_second_nonempty_remote_prevents_first_local_commit(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.gitignore').write_text('work/\n')
            with patch.object(release,'git_destination',return_value='destination'),patch.object(release,'run',side_effect=['','commit refs/tags/0.1']):
                with self.assertRaisesRegex(ValueError,'already has Git history'):release.initialize_source({},['gitlab','github'],root)
            self.assertFalse((root/'.git').exists())
    def test_inaccessible_remote_does_not_initialize(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.gitignore').write_text('work/\n')
            with patch.object(release,'git_destination',return_value='destination'),patch.object(release,'run',side_effect=subprocess.CalledProcessError(128,['git'])):
                with self.assertRaises(subprocess.CalledProcessError):release.initialize_source({},['gitlab'],root)
            self.assertFalse((root/'.git').exists())

class ReleaseMessages(unittest.TestCase):
    def test_custom_initial_commit_message(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'.gitignore').write_text('work/\n');(root/'VERSION').write_text('0.1')
            identity={'GIT_AUTHOR_NAME':'Test','GIT_AUTHOR_EMAIL':'test@example.invalid','GIT_COMMITTER_NAME':'Test','GIT_COMMITTER_EMAIL':'test@example.invalid'}
            original=release.run
            def commands(args,*a,**kw):
                if args[1:3]==['ls-remote','--refs']:return ''
                return original(args,*a,**kw)
            with patch.dict('os.environ',identity),patch.object(release,'run',side_effect=commands),patch.object(release,'git_destination',return_value='not-used'):
                release.initialize_source({},['gitlab'],root,commit_message='BitSkim 0.1 source release')
            message=original(['git','log','-1','--format=%s'],root,True)
            self.assertEqual(message,'BitSkim 0.1 source release')
    def test_publish_rejects_message_not_in_prepared_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);path=root/'outputs/release/manifest.json';path.parent.mkdir(parents=True)
            path.write_text(json.dumps({'tag':'0.1','tag_message':'Original'}))
            with self.assertRaisesRegex(ValueError,'Tag message differs'):release.publish(['gitlab'],True,root,tag_message='Different')

class PrivateFiles(unittest.TestCase):
    def test_source_archive_refuses_private_file(self):
        import source_release
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'scripts').mkdir();(root/'scripts/release-config.json').write_text('{}')
            with patch.object(source_release,'verify',return_value={}):
                with self.assertRaisesRegex(RuntimeError,'Private/local credential file'):source_release.write_source(root/'outputs',targets=(),root=root)
    def test_tracked_private_file_blocks_source_check(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            def git(*args):subprocess.run(['git',*args],cwd=root,check=True,capture_output=True)
            git('init','-q');git('config','user.name','Test');git('config','user.email','test@example.invalid')
            (root/'.env').write_text('EXAMPLE=value');git('add','.');git('commit','-qm','fixture')
            with self.assertRaisesRegex(ValueError,'credential file is tracked'):release.clean_commit(root)
