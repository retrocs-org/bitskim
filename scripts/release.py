#!/usr/bin/env python3
"""Build, prepare and explicitly publish BitSkim releases. Python 3.9+, no SDKs."""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
from release_paths import output_dir
import re
import subprocess
import sys
import shutil
import time
import tempfile
from datetime import datetime, timezone
from release_security import scan,redact
from release_notes import validate_notes
from release_paths import select_output
from urllib.parse import quote, urlsplit
from zipfile import ZipFile

from build_provenance import ROOT, SOURCE_ROOTS, source_hash, verify, write_json

TARGETS = ('macos', 'windows', 'amiga', 'dos', 'riscos-arm2', 'riscos-strongarm', 'setup', 'dmg', 'windows-legacy', 'atari-sting', 'atari-mintnet', 'atari-mintnet060')
DEFAULTS = {'github': 'https://github.com/retrocs-org/bitskim', 'gitlab': '',
            'github_git': 'git@github.com:retrocs-org/bitskim.git', 'gitlab_git': '',
            'notary_profile': 'bitskim', 'uploader': '', 'author': 'BitSkim contributors',
            'env': {}, 'credentials': {}, 'source_branch': '', 'aminet': False}


def run(args, root=ROOT, capture=False, env=None, log=None, verbose=False, secrets=()):
    if log is not None:
        process=subprocess.Popen([str(a) for a in args],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors="replace")
        for line in process.stdout:
            line=redact(line,secrets);log.write(line);log.flush()
            if verbose:print(line,end="",flush=True)
        code=process.wait()
        if code:raise subprocess.CalledProcessError(code,args)
        return None
    result = subprocess.run([str(a) for a in args], cwd=root, env=env, check=True,
                            stdout=subprocess.PIPE if capture else None, text=True)
    return result.stdout.strip() if capture else None


def configuration(path):
    path = Path(path).expanduser()
    if not path.is_file():
        raise ValueError('Create release settings at ' + str(path) + '; see docs/RELEASING.md for external settings. Keep tokens in 1Password or the environment')
    config = dict(DEFAULTS)
    if path.exists():
        supplied = json.loads(path.read_text())
        if set(supplied) - set(DEFAULTS):
            raise ValueError('Unknown configuration keys')
        config.update(supplied)
    return config


def credential(service, config):
    spec = config.get('credentials', {}).get(service, {})
    provider = spec.get('provider', 'env')
    if provider == 'env':
        token = os.environ.get(spec.get('variable', service.upper() + '_TOKEN'), '')
    elif provider == '1password':
        reference = spec.get('reference', '')
        if reference:
            if any(spec.get(key) for key in ('vault','item','field')):raise ValueError('Choose a 1Password reference or named fields, not both')
            if not reference.startswith('op://'): raise ValueError('Invalid 1Password secret reference for ' + service)
            command = ['op', 'read', '--no-newline', reference]
        else:
            if not all(spec.get(key) for key in ('vault', 'item', 'field')):
                raise ValueError('Configure a secret reference or vault/item/field for ' + service)
            command = ['op', 'item', 'get', spec['item'], '--vault', spec['vault'], '--fields', 'label=' + spec['field'], '--format', 'json', '--reveal']
        if spec.get('account'): command += ['--account', spec['account']]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise ValueError('1Password lookup failed or was cancelled for ' + service + '; no secret output displayed')
        if reference:
            token = result.stdout
        else:
            try:
                data = json.loads(result.stdout)
                fields = data if isinstance(data, list) else data.get('fields', [data])
                matches = [field for field in fields if field.get('label') == spec['field'] and field.get('value')]
                if len(matches) != 1: raise ValueError()
                token = matches[0]['value']
            except (ValueError, KeyError, TypeError, AttributeError):
                raise ValueError('1Password field missing or ambiguous for ' + service + '; use a copied secret reference') from None
    else:
        raise ValueError('Unknown credential provider for ' + service)
    if not token.strip() or token.startswith('op://'):
        raise ValueError('Missing/unresolved token for ' + service + '; configure 1Password or an environment variable')
    return token.strip()


def release_tag(tag=None):
    tag = '0.2' if tag is None else tag
    if not tag or tag.startswith('-'):
        raise ValueError('Invalid Git release tag')
    check = subprocess.run(['git', 'check-ref-format', 'refs/tags/' + tag], capture_output=True)
    if check.returncode: raise ValueError('Invalid Git release tag')
    return tag


def initialize_source(config, services, root=ROOT, commit_message=None):
    metadata = root / '.git'
    detected = subprocess.run(['git', 'rev-parse', '--show-toplevel'], cwd=root, capture_output=True, text=True)
    if detected.returncode == 0:
        if not Path(detected.stdout.strip()).samefile(root):
            raise ValueError('Refusing to initialize inside another Git repository; choose a separate project root')
        head = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'], cwd=root, capture_output=True)
        if head.returncode == 0:
            return  # Never create an extra initial commit in an existing repository.
    elif metadata.exists() or metadata.is_symlink():
        raise ValueError('Existing .git metadata is invalid; inspect it instead of overwriting it')
    if not (root / '.gitignore').is_file():
        raise ValueError('Review/create the project .gitignore before using --init')
    destinations = {service: git_destination(config, service) for service in services}
    # Check every selected remote before creating metadata, staging or committing.
    for service, destination in destinations.items():
        refs = run(['git', 'ls-remote', '--refs', destination], root, True)
        if refs.strip():
            raise ValueError(service + ' already has Git history/refs; clone that repository and import the source instead. Nothing initialized or uploaded.')
    for identity in ('GIT_AUTHOR_IDENT', 'GIT_COMMITTER_IDENT'):
        result = subprocess.run(['git', 'var', identity], cwd=root, capture_output=True)
        if result.returncode:
            raise ValueError('Configure Git user.name and user.email before creating the first commit')
    branch = config.get('source_branch') or 'main'
    run(['git', 'check-ref-format', '--branch', branch], root, True)
    if detected.returncode != 0:
        run(['git', 'init', '--initial-branch', branch], root)
    for service, destination in destinations.items():
        name = 'origin' if service == 'gitlab' else 'github'
        existing = subprocess.run(['git', 'remote', 'get-url', name], cwd=root, capture_output=True, text=True)
        if existing.returncode == 0:
            if existing.stdout.strip() != destination: raise ValueError('Existing Git remote differs: ' + name)
        else:
            run(['git', 'remote', 'add', name, destination], root)
    run(['git', 'add', '--all', '--', '.'], root)
    run(['git', 'commit', '-m', commit_message or 'Push changes to repository'], root)
    print('Created initial local commit, respecting .gitignore. No upload performed by initialization.')


def push_source(config, services, confirmed=False, dry_run=False, root=ROOT, initialize=False, commit_message=None):
    if not dry_run and not confirmed:
        raise ValueError('Use --dry-run first, then --confirm-push to upload committed source')
    if not services or len(set(services)) != len(services) or set(services) - {'gitlab', 'github'}:
        raise ValueError('Source push supports gitlab and github only')
    scan(root)
    if initialize: initialize_source(config, services, root, commit_message)
    scan(root)
    commit = clean_commit(root)
    branch = run(['git', 'symbolic-ref', '--quiet', '--short', 'HEAD'], root, True)
    target = config.get('source_branch') or branch
    run(['git', 'check-ref-format', '--branch', target], root, True)
    refspec = 'refs/heads/' + branch + ':refs/heads/' + target
    destinations = {service: git_destination(config, service) for service in services}
    for service, destination in destinations.items():
        print(('Checking source push to repository: ' if dry_run else 'Pushing changes to repository: ') + service + ' (' + branch + ' -> ' + target + ')', flush=True)
        run(['git', 'push', '--dry-run', '--no-follow-tags', destination, refspec], root)
    if dry_run:
        print('Source preflight passed; no upload performed.')
        return
    for service, destination in destinations.items():
        if clean_commit(root) != commit: raise ValueError('Source changed during push')
        run(['git', 'push', '--no-follow-tags', destination, refspec], root)
        current = run(['git', 'ls-remote', '--heads', destination, 'refs/heads/' + target], root, True)
        if not current or current.split()[0] != commit: raise ValueError('Remote source branch verification failed')
        print(service + ': source branch verified')
    print('Source uploaded; no compilation, release tags or binary uploads performed.')

def repository(url, github=False):
    parsed = urlsplit(url)
    path = parsed.path.strip('/')
    if path.endswith('.git'): path = path[:-4]
    if (parsed.scheme != 'https' or not parsed.hostname or parsed.username or parsed.password
            or parsed.query or parsed.fragment or parsed.port or not re.fullmatch(r'[A-Za-z0-9_.-]+(?:/[A-Za-z0-9_.-]+)+', path)):
        raise ValueError('Repository must be a credential-free HTTPS repository URL')
    if any(part in ('.', '..') for part in path.split('/')):
        raise ValueError('Repository path cannot contain dot segments')
    if github and (parsed.hostname != 'github.com' or len(path.split('/')) != 2):
        raise ValueError('GitHub destination must be https://github.com/OWNER/REPO')
    return parsed.hostname, path


def git_destination(config, service):
    host, project = repository(config[service], service == 'github')
    destination = config.get(service + '_git') or config[service]
    # Only the same HTTPS repository or its ordinary SSH spelling is allowed.
    expected = 'git@' + host + ':' + project + '.git'
    if destination != expected and destination.rstrip('/').removesuffix('.git') != config[service].rstrip('/').removesuffix('.git'):
        raise ValueError('Git push destination must match its release API repository')
    return destination


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def linux_asset_names():
    return {'BitSkim-0.2-Linux-'+kind+arch+'.tar.gz'
            for arch in ('arm64','x86_64') for kind in ('','CLI-')}


def release_targets(root=ROOT):
    targets=list(TARGETS)
    for arch in ('arm64','x86_64'):
        desktop=(output_dir(root)/('BitSkim-0.2-Linux-'+arch+'.tar.gz')).is_file()
        cli=(output_dir(root)/('BitSkim-0.2-Linux-CLI-'+arch+'.tar.gz')).is_file()
        if desktop: targets.append('linux-'+arch)
        if desktop or cli: targets.append('linux-'+arch+'-cli')
    return tuple(targets)


def inventory(root=ROOT):
    files = {}
    for line in (output_dir(root) / 'SHA256SUMS').read_text().splitlines():
        match = re.fullmatch(r'([a-f0-9]{64})  ([A-Za-z0-9][A-Za-z0-9_.-]*)', line)
        if not match or match[2] in files:
            raise ValueError('Invalid or duplicate release checksum entry')
        path = output_dir(root) / match[2]
        if not path.is_file() or path.is_symlink() or digest(path) != match[1]:
            raise ValueError('Missing or changed release asset: ' + match[2])
        files[match[2]] = match[1]
    required = {'BitSkim-0.2-source.zip', 'BitSkim-src.tar.gz', 'BitSkim-0.2.lha',
                'BitSkim-0.2-macOS-arm64.zip', 'BitSkim-0.2-macOS-arm64.dmg',
                'BitSkim-0.2-Windows10-x64.zip', 'BitSkim-0.2-Windows10-x64-Setup.exe',
                'BitSkim-0.2-AmigaOS.zip', 'BitSkim-0.2-DOS-mTCP.zip',
                'BitSkim-0.2-RISCOS3-ARM2.zip', 'BitSkim-0.2-RISCOS3-StrongARM.zip',
                'BitSkim-0.2-website.zip', 'BitSkim-0.2-Windows-Classic-x86.zip',
                'BitSkim-0.2-Atari-TOS-STinG-68000.zip', 'BitSkim-0.2-Atari-MiNTNet-68000.zip',
                'BitSkim-0.2-Atari-MiNTNet-68060.zip'}
    required.update(name for name in linux_asset_names() if (output_dir(root)/name).is_file())
    if required != files.keys():
        raise ValueError('Incomplete release; build/package every release target first')
    files['SHA256SUMS'] = digest(output_dir(root) / 'SHA256SUMS')
    return files


def clean_commit(root=ROOT):
    scan(root)
    if not (root / '.git').exists():
        raise ValueError('Source has no .git directory; use a Git checkout, import the source and commit it before releasing')
    if not Path(run(['git', 'rev-parse', '--show-toplevel'], root, True)).samefile(root):
        raise ValueError('Publishing requires the project root to be a Git checkout')
    if run(['git', 'status', '--porcelain', '--untracked-files=all'], root, True):
        raise ValueError('Commit all release source changes before publishing')
    # Ignored compiler inputs must not silently differ from the tagged source.
    tracked = set(run(['git', 'ls-files'], root, True).splitlines())
    from source_release import private_release_file
    for name in tracked:
        if private_release_file(name): raise ValueError('Private/local credential file is tracked; remove it from Git before pushing: ' + name)
    for name in SOURCE_ROOTS:
        base = root / name
        for path in (base.rglob('*') if base.is_dir() else [base]):
            rel = path.relative_to(root)
            if ('__pycache__' in rel.parts or path.name == '.DS_Store' or path.name.startswith('._')
                    or rel.parts[:2] == ('public-web', 'downloads')):
                continue
            if (path.is_file() or path.is_symlink()) and str(rel) not in tracked:
                raise ValueError('Build input is not committed: ' + str(rel))
    return run(['git', 'rev-parse', 'HEAD'], root, True)


def check_builds(root=ROOT):
    scan(root)
    records = {target: verify(target, root) for target in release_targets(root)}
    from linux_release import verify_archive
    for arch in ('arm64','x86_64'):
        for cli_only in (False,True):
            name='BitSkim-0.2-Linux-'+('CLI-' if cli_only else '')+arch+'.tar.gz'
            archive=output_dir(root)/name
            if archive.is_file():verify_archive(archive,arch,cli_only,root)
    for target in ('macos', 'dmg'):
        n = records[target].get('notarization', {})
        if n.get('status') != 'Accepted' or not n.get('ticket_stapled'):
            raise ValueError('Public Mac releases require accepted, stapled notarization: ' + target)
    return records


def check_source_archive(root=ROOT):
    with ZipFile(output_dir(root) / 'BitSkim-0.2-source.zip') as archive:
        records = json.loads(archive.read('BitSkim/BUILD-PROVENANCE.json'))
        for target in release_targets(root):
            if records.get(target) != verify(target, root):
                raise ValueError('Source archive build record differs: ' + target)
        for name in archive.namelist():
            if name == 'BitSkim/BUILD-PROVENANCE.json': continue
            rel = Path(name)
            if rel.parts[0] != 'BitSkim' or '..' in rel.parts:
                raise ValueError('Unsafe source archive path')
            path = root.joinpath(*rel.parts[1:])
            if not path.is_file() or archive.read(name) != path.read_bytes():
                raise ValueError('Source archive content differs: ' + name)
        if 'BitSkim/scripts/release.py' not in archive.namelist():
            raise ValueError('Source archive does not include the release tool')


def publication_assets(root, full_inventory):
    files = {}
    payloads = {}
    for name, sha in full_inventory.items():
        if name in ('SHA256SUMS', 'BitSkim-0.2-source.zip', 'BitSkim-src.tar.gz', 'BitSkim-0.2-website.zip'):
            continue
        files[name] = sha
        content = (sha + '  ' + name + '\n').encode()
        files[name + '.sha256'] = hashlib.sha256(content).hexdigest()
        payloads[name + '.sha256'] = content
    return files, payloads


def verify_publication(root, manifest):
    stage = output_dir(root) / 'release/publish'
    for name, sha in manifest['files'].items():
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name): raise ValueError('Unsafe publication asset')
        path = stage / 'outputs' / name
        if not path.is_file() or path.is_symlink() or digest(path) != sha:
            raise ValueError('Prepared publication asset changed: ' + name)
    return stage

def prepare(config, notes, root=ROOT, tag=None, tag_message=None):
    tag = release_tag(tag)
    if tag!='0.2':raise ValueError('New release tag must equal VERSION: 0.2 (no v prefix)')
    scan(root)
    text=validate_notes(notes.read_text().strip(),root)
    check_builds(root)
    full_inventory = inventory(root)
    files, sidecars = publication_assets(root, full_inventory)
    check_source_archive(root)
    for service in ('github', 'gitlab'):
        repository(config[service], service == 'github')
    # An extracted source distribution can be built/prepared, but cannot publish.
    try:
        commit = clean_commit(root)
    except (ValueError, subprocess.CalledProcessError):
        commit = None
    manifest = {'release_kind': 'stable', 'make_latest': True, 'version': '0.2', 'tag': tag, 'tag_message': tag_message or 'BitSkim ' + tag, 'source_sha256': source_hash(root),
                'commit': commit, 'github': config['github'], 'gitlab': config['gitlab'],
                'github_git': git_destination(config, 'github'), 'gitlab_git': git_destination(config, 'gitlab'),
                'notes': text, 'files': files, 'inventory': full_inventory}
    path = output_dir(root) / 'release/manifest.json'
    if (path.parent / 'receipt.json').exists():
        old = json.loads(path.read_text())
        if old != manifest:
            raise ValueError('An earlier publication receipt exists; review it before replacing this release')
    stage = output_dir(root) / 'release/publish/outputs'
    stage.mkdir(parents=True, exist_ok=True)
    import shutil
    for name in files:
        if name in sidecars: (stage / name).write_bytes(sidecars[name])
        else: shutil.copy2(output_dir(root) / name, stage / name)
    verify_publication(root, manifest)
    write_json(path, manifest)
    if config.get('aminet'):
        if not config.get('uploader'):raise ValueError('Set the public Aminet uploader contact')
        from aminet_release import prepare as aminet_prepare
        aminet_prepare(config['uploader'], config['author'], root)
    print('Prepared ' + str(path) + (' (no Git commit; publishing is blocked)' if commit is None else ''))
    return manifest


class API:
    """HTTPS only, streaming uploads/download verification, no credential redirects."""
    def __init__(self, host, token, github=False):
        self.host, self.token, self.github = host, token, github

    def request(self, method, path, body=None, file=None, optional=False, hash_body=False, host=None):
        host = host or self.host
        if host not in (self.host, 'uploads.github.com' if self.github else self.host):
            raise ValueError('Untrusted API upload host')
        headers = {'User-Agent': 'BitSkim-release/0.2', 'Accept': 'application/json'}
        headers['Authorization' if self.github else 'PRIVATE-TOKEN'] = ('Bearer ' if self.github else '') + self.token
        data = json.dumps(body).encode() if body is not None else None
        if data is not None:
            headers['Content-Type'] = 'application/json'
        connection = http.client.HTTPSConnection(host, timeout=120)
        stream = None
        try:
            if file:
                stream = file.open('rb'); data = stream
                headers.update({'Content-Type': 'application/octet-stream', 'Content-Length': str(file.stat().st_size)})
            connection.request(method, path, body=data, headers=headers)
            response = connection.getresponse()
            if optional and response.status == 404:
                response.read(); return None
            if not 200 <= response.status < 300:
                response.read()
                raise RuntimeError('Release API returned HTTP %d for %s %s (no automatic retry)' % (response.status, method, self.host))
            if hash_body:
                h = hashlib.sha256()
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    h.update(chunk)
                return h.hexdigest()
            payload = response.read()
            return json.loads(payload) if payload else {}
        finally:
            if stream: stream.close()
            connection.close()


def remote_tag(url, tag, root=ROOT):
    result = run(['git', 'ls-remote', '--tags', url, 'refs/tags/' + tag, 'refs/tags/' + tag + '^{}'], root, True)
    refs = dict((ref, sha) for sha, ref in (line.split() for line in result.splitlines()))
    return refs.get('refs/tags/' + tag + '^{}', refs.get('refs/tags/' + tag))


def remote_tag_details(url,tag,root=ROOT):
    result=run(['git','ls-remote','--tags',url,'refs/tags/'+tag,'refs/tags/'+tag+'^{}'],root,True)
    refs=dict((ref,sha) for sha,ref in (line.split() for line in result.splitlines()))
    obj=refs.get('refs/tags/'+tag)
    return {'object':obj,'commit':refs.get('refs/tags/'+tag+'^{}',obj),'annotated':'refs/tags/'+tag+'^{}' in refs}

def validate_tag_identity(manifest,services,root=ROOT):
    tag=manifest['tag'];local=run(['git','tag','--list',tag],root,True)
    remotes={service:remote_tag_details(git_destination(manifest,service),tag,root) for service in services}
    if any(info['object'] for info in remotes.values()) and not local:raise ValueError('Fetch the existing annotated release tag locally before resuming; never recreate it')
    if local:
        if run(['git','cat-file','-t','refs/tags/'+tag],root,True)!='tag':raise ValueError('Release tag must be annotated; existing lightweight tags are not replaced')
        if run(['git','rev-parse',tag+'^{commit}'],root,True)!=manifest['commit']:raise ValueError('Local release tag points at a different commit')
        if run(['git','for-each-ref','--format=%(contents)','refs/tags/'+tag],root,True).strip()!=manifest.get('tag_message','BitSkim '+tag).strip():raise ValueError('Existing tag annotation differs from the prepared release')
        obj=run(['git','rev-parse','refs/tags/'+tag],root,True)
        for info in remotes.values():
            if info['object'] and (not info['annotated'] or info['object']!=obj):raise ValueError('Repository tag objects differ; never move or recreate a published tag')
    return local

def check_repository_access(service,project,api,token):
    if service=='github':
        repo=api.request('GET','/repos/'+project)
        if repo.get('full_name','').lower()!=project.lower() or not repo.get('id'):raise ValueError('GitHub returned a different repository identity')
        if token.startswith('ghs_'):
            page=1;found=False
            while True:
                entries=api.request('GET','/installation/repositories?per_page=100&page='+str(page)).get('repositories',[])
                if any(item.get('id')==repo['id'] and item.get('full_name','').lower()==project.lower() for item in entries):found=True
                if len(entries)<100:break
                page+=1
            if not found:raise ValueError('GitHub App installation token does not include the configured repository')
    else:
        repo=api.request('GET','/api/v4/projects/'+quote(project,safe=''))
        if repo.get('path_with_namespace')!=project:raise ValueError('GitLab returned a different repository identity')


def publish_repository(service, manifest, api, root=ROOT, assets_dir=None, preflight=False):
    assets_dir=Path(assets_dir) if assets_dir is not None else output_dir(root)
    _, project = repository(manifest[service], service == 'github')
    tag = manifest['tag']; encoded_tag = quote(tag, safe='')
    base = '/repos/' + project if service == 'github' else '/api/v4/projects/' + quote(project, safe='')
    release_path = base + '/releases/' + ('tags/' if service == 'github' else '') + encoded_tag
    release = api.request('GET', release_path, optional=True)
    notes_key = 'body' if service == 'github' else 'description'
    if release and release.get(notes_key, '').strip() != manifest['notes']:
        raise ValueError(service + ': existing release notes differ; review manually')
    if service == 'github':
        if release is None:
            if preflight:return
            release = api.request('POST', base + '/releases', {'tag_name': tag, 'target_commitish': manifest['commit'],
                                  'name': manifest['tag'], 'body': manifest['notes'], 'draft': True, 'prerelease': False})
        asset_base = base + '/releases/' + str(release['id']) + '/assets'
        assets = {}; page = 1
        while True:
            entries = api.request('GET', asset_base + '?per_page=100&page=' + str(page))
            assets.update({a['name']: a for a in entries})
            if len(entries) < 100: break
            page += 1
        if set(assets)-set(manifest['files']):raise ValueError('Unexpected GitHub release assets; review manually')
        for name,item in assets.items():
            if item.get('digest')!='sha256:'+manifest['files'][name] or item.get('size')!=(assets_dir/name).stat().st_size:raise ValueError('GitHub asset has different/unknown digest or size: '+name)
        if not release.get('draft',False) and set(assets)!=set(manifest['files']):raise ValueError('Incomplete public GitHub release; review manually')
        if preflight:return
        upload = urlsplit(release['upload_url'].split('{')[0])
        if upload.scheme != 'https' or upload.hostname != 'uploads.github.com':
            raise ValueError('Unexpected GitHub upload URL')
        for name, sha in manifest['files'].items():
            if digest(assets_dir / name) != sha:
                raise ValueError('Local asset changed during publication: ' + name)
            if name in assets:
                if assets[name].get('digest') != 'sha256:' + sha:
                    raise ValueError('GitHub asset exists with different/unknown digest: ' + name)
                continue
            item = api.request('POST', upload.path + '?name=' + quote(name, safe=''),
                               file=assets_dir / name, host=upload.hostname)
            if item.get('digest') != 'sha256:' + sha or item.get('size') != (assets_dir/name).stat().st_size:
                raise ValueError('GitHub did not confirm uploaded asset checksum: ' + name)
        api.request('PATCH', base + '/releases/' + str(release['id']), {'draft': False, 'prerelease': False, 'make_latest': 'true'})
        latest=api.request('GET',base+'/releases/latest')
        if not latest or latest.get('tag_name')!=tag or latest.get('draft') or latest.get('prerelease'):raise ValueError('GitHub latest normal release verification failed')
        return release['html_url']
    links=[];packages=[]
    for name,sha in manifest['files'].items():
        if digest(assets_dir/name)!=sha:raise ValueError('Local asset changed during publication: '+name)
        package_path=base+'/packages/generic/bitskim/'+quote(manifest['version'],safe='')+'/'+quote(name,safe='')
        existing=api.request('GET',package_path,optional=True,hash_body=True)
        if existing is not None and existing!=sha:raise ValueError('GitLab package already exists with different bytes: '+name)
        packages.append((name,sha,package_path,existing))
        links.append({'name':name,'url':'https://'+api.host+package_path,'link_type':'package'})
    if release:
        current={a['name']:a['url'] for a in release.get('assets',{}).get('links',[])}
        if len(current)!=len(release.get('assets',{}).get('links',[])) or current!={a['name']:a['url'] for a in links} or any(p[3] is None for p in packages):raise ValueError('GitLab public release inventory differs; review manually')
    if preflight:return
    for name,sha,path,existing in packages:
        if existing is None:
            api.request('PUT',path,file=assets_dir/name)
            if api.request('GET',path,hash_body=True)!=sha:raise ValueError('GitLab upload verification failed: '+name)
    now=datetime.now(timezone.utc).isoformat()
    if release is None:
        release=api.request('POST',base+'/releases',{'tag_name':tag,'name':tag,'description':manifest['notes'],'released_at':now,'assets':{'links':links}})
    else:
        release=api.request('PUT',release_path,{'released_at':now})
    latest=api.request('GET',base+'/releases/permalink/latest')
    if not latest or latest.get('tag_name')!=tag or latest.get('upcoming_release'):raise ValueError('GitLab latest normal release verification failed')
    return release.get('_links',{}).get('self',manifest['gitlab']+'/-/releases/'+tag)


def publish(services, confirmed=False, root=ROOT, config=None, tag=None, tag_message=None):
    if not confirmed:
        raise ValueError('No upload performed. Review outputs/release/manifest.json, then use --confirm-publish')
    existing=output_dir(root)/'release/aminet-existing'
    if services==['aminet'] and (existing/'submission.json').exists():
        if tag_message is not None:raise ValueError('Already-published Aminet submissions do not change tag annotations')
        publish_existing_aminet(root,tag);return
    path = output_dir(root) / 'release/manifest.json'
    manifest = json.loads(path.read_text())
    if tag_message is not None and tag_message != manifest.get('tag_message', 'BitSkim ' + manifest['tag']):
        raise ValueError('Tag message differs from the prepared manifest; prepare again with --tag-message')
    if release_tag(manifest['tag']) != manifest['tag'] or manifest['version'] != '0.2' or (tag is not None and release_tag(tag) != manifest['tag']):
        raise ValueError('Version/tag mismatch')
    if clean_commit(root) != manifest['commit'] or source_hash(root) != manifest['source_sha256']:
        raise ValueError('Source/commit changed; build and prepare again')
    validate_notes(manifest['notes'],root)
    check_builds(root)
    check_source_archive(root)
    if inventory(root) != manifest.get('inventory', manifest['files']):
        raise ValueError('Release assets changed; prepare again')
    publication_root = verify_publication(root, manifest) if 'inventory' in manifest else root
    publication_assets_dir=publication_root/'outputs' if 'inventory' in manifest else output_dir(root)
    apis = {}
    # Preflight every selected repository and tag before any push or upload.
    for service in services:
        if service == 'aminet':
            from aminet_release import check,preflight
            check(output_dir(root) / 'aminet', root)
            if not (output_dir(root)/'aminet/upload-receipt.json').exists():preflight(output_dir(root)/'aminet',root)
            if (output_dir(root) / 'aminet/upload-receipt.json').exists():
                receipt = json.loads((output_dir(root) / 'aminet/upload-receipt.json').read_text())
                if receipt.get('status') != 'transferred-awaiting-aminet-review':
                    raise ValueError('Review the partial Aminet receipt before retrying')
            continue
        host, project = repository(manifest[service], service == 'github')
        if config is not None:
            if manifest[service] != config[service] or git_destination(manifest, service) != git_destination(config, service):
                raise ValueError('Destination changed; prepare again')
        token = credential(service, config or {})
        api = API('api.github.com' if service == 'github' else host, token, service == 'github')
        current = remote_tag(git_destination(manifest, service), manifest['tag'], root)
        if current is not None and current != manifest['commit']:
            raise ValueError(service + ': remote tag points at a different commit; refusing to move it')
        check_repository_access(service,project,api,token)
        publish_repository(service,manifest,api,root,assets_dir=publication_assets_dir,preflight=True)
        apis[service] = api
    tag = manifest['tag']
    local=validate_tag_identity(manifest,list(apis),root) if apis else ''
    receipt_path = path.parent / 'receipt.json'
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {'manifest_sha256': digest(path), 'completed': {}}
    if receipt['manifest_sha256'] != digest(path):
        raise ValueError('Publication receipt belongs to a different release manifest')
    write_json(receipt_path, receipt)
    if apis and not local:
        run(['git', 'tag', '-a', tag, '-m', manifest.get('tag_message', 'BitSkim ' + tag), manifest['commit']], root)
    if apis:
        obj=run(['git','rev-parse','refs/tags/'+tag],root,True)
        if receipt.get('tag_object',obj)!=obj:raise ValueError('Receipt belongs to a different tag object')
        receipt['tag_object']=obj;write_json(receipt_path,receipt)
    for service in apis:
        run(['git','push','--dry-run','--no-follow-tags',git_destination(manifest,service),'refs/tags/'+tag+':refs/tags/'+tag],root)
    for service in services:
        if service in receipt['completed']: continue
        if 'inventory' in manifest:verify_publication(root,manifest)
        receipt['in_progress'] = service; write_json(receipt_path, receipt)
        if service == 'aminet':
            from aminet_release import upload
            folder = output_dir(root) / 'aminet'
            if not (folder / 'upload-receipt.json').exists(): upload(folder, True, root)
            url = 'Aminet incoming: transferred, awaiting moderator review'
        else:
            run(['git', 'push', git_destination(manifest, service), 'refs/tags/' + tag + ':refs/tags/' + tag], root)
            remote=remote_tag_details(git_destination(manifest,service),tag,root)
            if remote['commit']!=manifest['commit'] or not remote['annotated'] or remote['object']!=receipt['tag_object']:
                raise ValueError('Remote tag verification failed: ' + service)
            verify_publication(root,manifest)
            url = publish_repository(service, manifest, apis[service], publication_root,assets_dir=publication_assets_dir)
        receipt['completed'][service] = url; receipt.pop('in_progress', None); write_json(receipt_path, receipt)
        print(service + ': ' + url)


def build(config, root=ROOT, plan=False, verbose=False):
    allowed = {'BITSKIM_SIGN_IDENTITY', 'BITSKIM_RELEASE_PYTHON', 'AMIGA_CC', 'LHA', 'LHA_READER', 'WINDOWS_CC', 'RISCOS_CC', 'RISCOS_SDK_SOURCE', 'WATCOM', 'DOS_RUNNER', 'PHP', 'AMIGA_DEV_BIN', 'BITSKIM_BUILDS_DIR', 'ATARI_CC', 'STING_INCLUDE', 'LEGACY_WINDOWS_CC', 'LEGACY_WINDOWS_RC', 'MAKENSIS', 'NSISDIR', 'BITSKIM_LINUX_INSTANCE', 'BITSKIM_LIMA_HOME', 'BITSKIM_LINUX_ARCHES', 'BITSKIM_LINUX_PKG_CONFIG_PATH'}
    if set(config['env']) - allowed:
        raise ValueError('Only documented build settings belong in config; tokens belong in the environment')
    env = dict(os.environ, **config['env'])
    for key in ('GITHUB_TOKEN', 'GITLAB_TOKEN'): env.pop(key, None)
    if not env.get('BITSKIM_SIGN_IDENTITY'):
        raise ValueError('Set BITSKIM_SIGN_IDENTITY for the public Mac release')
    secret_variables={'GITHUB_TOKEN','GITLAB_TOKEN','GH_TOKEN','GLAB_TOKEN','OP_SERVICE_ACCOUNT_TOKEN'}
    secret_variables.update(key for key in env if re.search(r'TOKEN|SECRET|PASSWORD|PRIVATE_KEY|ACCESS_KEY|CREDENTIAL',key,re.I))
    secret_variables.update(spec.get('variable','') for spec in config.get('credentials',{}).values() if spec.get('provider','env')=='env')
    secrets=[os.environ.get(key,env.get(key,'')) for key in secret_variables if os.environ.get(key,env.get(key,''))]
    for key in secret_variables:env.pop(key,None)
    py = env.get('BITSKIM_RELEASE_PYTHON', sys.executable)
    commands = [[py, '-m', 'unittest', 'discover', '-s', 'tests', '-b'], ['./scripts/build-macos.sh'],
                [py, 'scripts/notarize-macos.py', 'macos', '--profile', config['notary_profile']],
                ['./scripts/build-windows.sh'], ['./scripts/build-amiga.sh'],
                [py, 'scripts/build-dos.py'], ['./scripts/build-riscos.sh'],
                ['./scripts/build-atari.sh'], ['./scripts/build-windows-legacy.sh'],
                ['./scripts/build-windows-installer.sh'], [py, 'scripts/build-macos-dmg.py'],
                [py, 'scripts/notarize-macos.py', 'dmg', '--profile', config['notary_profile']],
                [py, 'scripts/package.py'], [py, 'scripts/check-release-site.py']]
    if env.get('BITSKIM_LINUX_INSTANCE'):
        arches=env.get('BITSKIM_LINUX_ARCHES','arm64,x86_64').split(',')
        if not arches or len(set(arches))!=len(arches) or any(a not in ('arm64','x86_64') for a in arches):
            raise ValueError('BITSKIM_LINUX_ARCHES must list arm64 and/or x86_64 once')
        position=next(i for i,c in enumerate(commands) if 'scripts/package.py' in c)
        commands[position:position]=[[py,'scripts/build-linux-lima.py','--instance',env['BITSKIM_LINUX_INSTANCE'],'--arch',arch,'--test'] for arch in arches]
    elif env.get('BITSKIM_LINUX_ARCHES'):
        raise ValueError('Set BITSKIM_LINUX_INSTANCE for the configured Linux builds')
    if env.get('AMIGA_DEV_BIN') or (root / 'work/amiga-dev-bin.txt').exists():
        commands.append([py, 'scripts/sync-amiga-bin.py'])
    if plan:
        for command in commands:print(' '.join(command))
        return
    clean_commit(root);before=source_hash(root);out=output_dir(root);state=out/'release'
    if (state/'receipt.json').exists() or any(out.glob('**/upload-receipt.json')):raise ValueError('Publication receipts exist; use a fresh --output directory or checkout')
    if out!=root/'outputs' and out.exists() and any(out.iterdir()):raise ValueError('Choose a fresh --output directory for this build')
    state.mkdir(parents=True,exist_ok=True);log_path=state/'build.log'
    try:
        with log_path.open('w') as log:
            for index,command in enumerate(commands,1):
                start=time.monotonic();print(str(index)+'/'+str(len(commands))+': '+command[1 if command[0]==py else 0],flush=True)
                child=env.copy()
                if '-m' in command:child.pop('BITSKIM_RELEASE_OUTPUT',None)
                run(command,root,env=child,log=log,verbose=verbose,secrets=secrets)
                if source_hash(root)!=before:raise ValueError('Release inputs changed during build; rebuild')
                print('  completed in %.1fs'%(time.monotonic()-start),flush=True)
        check_builds(root);files=inventory(root);check_source_archive(root)
        snapshot=state/'provenance';snapshot.mkdir(exist_ok=True)
        for target in release_targets(root):shutil.copy2(root/'build/provenance'/(target+'.json'),snapshot/(target+'.json'))
        for name in files:
            if name!='SHA256SUMS':print(name+': '+str((out/name).stat().st_size)+' bytes')
        print('Build log: '+str(log_path))
    except Exception:
        if log_path.exists():shutil.copy2(log_path,state/'failed-build.log')
        raise


def retained_release(root,tag=None):
    path=output_dir(root)/'release/manifest.json';manifest=json.loads(path.read_text());receipt=json.loads((path.parent/'receipt.json').read_text())
    if receipt.get('manifest_sha256')!=digest(path) or not set(receipt.get('completed',{})).intersection(('github','gitlab')):raise ValueError('A completed repository publication receipt is required')
    if tag is not None and tag!=manifest['tag']:raise ValueError('Requested tag differs from the retained published release')
    if not re.fullmatch(r'[0-9a-f]{40,64}',manifest['commit']):raise ValueError('Invalid retained source commit')
    stage=verify_publication(root,manifest)/'outputs'
    return manifest,stage,digest(path)

def prepare_aminet(config,root=ROOT,tag=None):
    from aminet_release import readme,ARCHIVE,README,HOST,CATEGORY,contact
    if not config.get('aminet') or not config.get('uploader'):raise ValueError('Enable aminet and configure the public uploader contact externally')
    manifest,stage,fingerprint=retained_release(root,tag);version=manifest['version'];archive=stage/('BitSkim-'+version+'.lha')
    if archive.name not in manifest['files']:raise ValueError('Retained release has no verified Amiga LHA')
    folder=output_dir(root)/'release/aminet-existing'
    if (folder/'upload-receipt.json').exists():raise ValueError('Review the existing Aminet upload receipt; no automatic retry')
    if folder.exists() and any(folder.iterdir()):raise ValueError('Existing Aminet preparation must be preserved and reviewed before another preparation')
    changelog=run(['git','show',manifest['commit']+':CHANGELOG.MD'],root,True)
    with tempfile.TemporaryDirectory() as temp:
        documentation=Path(temp);(documentation/'CHANGELOG.MD').write_text(changelog)
        text=readme(config['uploader'],config.get('author','BitSkim contributors'),documentation,version,manifest['tag'])
    folder.mkdir(parents=True);shutil.copy2(archive,folder/ARCHIVE);(folder/README).write_text(text,encoding='ascii')
    write_json(folder/'submission.json',{'version':version,'tag':manifest['tag'],'published_manifest_sha256':fingerprint,'host':HOST,'directory':'/new','uploader':config['uploader'],'category':CATEGORY,'source_sha256':manifest['source_sha256'],'files':{name:digest(folder/name) for name in (ARCHIVE,README)}})
    print('Prepared '+str(folder)+'; review bitskim.readme before explicit upload')
    return folder

def publish_existing_aminet(root,tag=None):
    from aminet_release import upload,ARCHIVE,check
    manifest,stage,fingerprint=retained_release(root,tag);folder=output_dir(root)/'release/aminet-existing';submission=check(folder,root,published=True)
    if submission.get('published_manifest_sha256')!=fingerprint or submission.get('tag')!=manifest['tag'] or submission['version']!=manifest['version']:raise ValueError('Aminet preparation belongs to another published release')
    if digest(folder/ARCHIVE)!=manifest['files']['BitSkim-'+manifest['version']+'.lha']:raise ValueError('Aminet archive differs from the published LHA')
    upload(folder,True,root,published=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('source', 'plan', 'build', 'prepare', 'prepare-aminet', 'check', 'publish', 'release'))
    p.add_argument('--config', type=Path, default=Path(os.environ.get('BITSKIM_RELEASE_CONFIG', str(Path.home() / 'Development/tools/git-release/projects/bitskim.json'))))
    p.add_argument('--output',type=Path,help='Fresh artifact directory under outputs/; use the same value for check/prepare/publish')
    p.add_argument('--verbose',action='store_true',help='Stream redacted build output as well as saving its log')
    p.add_argument('--notes', type=Path, default=ROOT / 'docs/RELEASE-NOTES.md')
    p.add_argument('--to', default='gitlab,github', help='Comma-separated gitlab,github,aminet')
    p.add_argument('--confirm-publish', action='store_true')
    p.add_argument('--confirm-push', action='store_true')
    p.add_argument('--init', dest='initialize', action='store_true', help='Create the first local commit only after selected remotes are verified empty')
    p.add_argument('--dry-run', action='store_true')
    p.add_argument('--commit-message', help='Message for the first local commit with source --init')
    p.add_argument('--tag-message', help='Annotation for a newly created release tag')
    p.add_argument('--tag', help='Exact release tag, e.g. 0.2; default 0.2')
    a = p.parse_args()
    if a.output is not None:select_output(ROOT,a.output)
    if a.verbose and a.action!='build':p.error('--verbose is for build only')
    config = configuration(a.config)
    if a.commit_message is not None and not (a.action == 'source' and a.initialize): p.error('--commit-message requires source --init')
    if a.tag_message is not None and a.action not in ('prepare', 'publish', 'release'): p.error('--tag-message is for prepare/publish/release')
    if (a.dry_run or a.initialize) and a.action != 'source': p.error('--dry-run and --init are for source only')
    if a.action == 'source': push_source(config, a.to.split(','), a.confirm_push, a.dry_run, initialize=a.initialize, commit_message=a.commit_message)
    elif a.action in ('plan', 'build'): build(config, plan=a.action == 'plan',verbose=a.verbose)
    elif a.action == 'prepare-aminet':prepare_aminet(config,tag=a.tag)
    elif a.action == 'prepare': prepare(config, a.notes, tag=a.tag, tag_message=a.tag_message)
    elif a.action == 'check': check_builds(); inventory(); check_source_archive(); print('PASS: current builds, notarization and release checksums')
    else:
        services = a.to.split(',')
        if not services or len(set(services)) != len(services) or set(services) - {'gitlab', 'github', 'aminet'}:
            raise ValueError('Choose distinct destinations from gitlab,github,aminet')
        if a.action == 'release':
            if not a.confirm_publish: raise ValueError('Use --confirm-publish to prepare and publish')
            prepare(config, a.notes, tag=a.tag, tag_message=a.tag_message)
        publish(services, a.confirm_publish, config=config, tag=a.tag, tag_message=a.tag_message)


if __name__ == '__main__':
    try: main()
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
