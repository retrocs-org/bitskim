#!/usr/bin/env python3
"""Build, prepare and explicitly publish BitSkim releases. Python 3.9+, no SDKs."""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import quote, urlsplit
from zipfile import ZipFile

from build_provenance import ROOT, SOURCE_ROOTS, source_hash, verify, write_json

TARGETS = ('macos', 'windows', 'amiga', 'dos', 'riscos-arm2', 'riscos-strongarm', 'setup', 'dmg', 'windows-legacy', 'atari-sting', 'atari-mintnet', 'atari-mintnet060')
DEFAULTS = {'github': 'https://github.com/retrocs-org/bitskim', 'gitlab': '',
            'github_git': 'git@github.com:retrocs-org/bitskim.git', 'gitlab_git': '',
            'notary_profile': 'bitskim', 'uploader': '', 'author': 'BitSkim contributors',
            'env': {}, 'credentials': {}, 'source_branch': ''}


def run(args, root=ROOT, capture=False, env=None):
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
    tag = 'v0.2' if tag is None else tag
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
    if initialize: initialize_source(config, services, root, commit_message)
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


def inventory(root=ROOT):
    files = {}
    for line in (root / 'outputs/SHA256SUMS').read_text().splitlines():
        match = re.fullmatch(r'([a-f0-9]{64})  ([A-Za-z0-9][A-Za-z0-9_.-]*)', line)
        if not match or match[2] in files:
            raise ValueError('Invalid or duplicate release checksum entry')
        path = root / 'outputs' / match[2]
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
    if required != files.keys():
        raise ValueError('Incomplete release; build/package every release target first')
    files['SHA256SUMS'] = digest(root / 'outputs/SHA256SUMS')
    return files


def clean_commit(root=ROOT):
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
    records = {target: verify(target, root) for target in TARGETS}
    for target in ('macos', 'dmg'):
        n = records[target].get('notarization', {})
        if n.get('status') != 'Accepted' or not n.get('ticket_stapled'):
            raise ValueError('Public Mac releases require accepted, stapled notarization: ' + target)
    return records


def check_source_archive(root=ROOT):
    with ZipFile(root / 'outputs/BitSkim-0.2-source.zip') as archive:
        records = json.loads(archive.read('BitSkim/BUILD-PROVENANCE.json'))
        for target in TARGETS:
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
    stage = root / 'outputs/release/publish'
    for name, sha in manifest['files'].items():
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name): raise ValueError('Unsafe publication asset')
        path = stage / 'outputs' / name
        if not path.is_file() or path.is_symlink() or digest(path) != sha:
            raise ValueError('Prepared publication asset changed: ' + name)
    return stage

def prepare(config, notes, root=ROOT, tag=None, tag_message=None):
    tag = release_tag(tag)
    check_builds(root)
    full_inventory = inventory(root)
    files, sidecars = publication_assets(root, full_inventory)
    check_source_archive(root)
    for service in ('github', 'gitlab'):
        repository(config[service], service == 'github')
    text = notes.read_text().strip()
    if not text:
        raise ValueError('Release notes are empty')
    # An extracted source distribution can be built/prepared, but cannot publish.
    try:
        commit = clean_commit(root)
    except (ValueError, subprocess.CalledProcessError):
        commit = None
    manifest = {'version': '0.2', 'tag': tag, 'tag_message': tag_message or 'BitSkim ' + tag, 'source_sha256': source_hash(root),
                'commit': commit, 'github': config['github'], 'gitlab': config['gitlab'],
                'github_git': git_destination(config, 'github'), 'gitlab_git': git_destination(config, 'gitlab'),
                'notes': text, 'files': files, 'inventory': full_inventory}
    path = root / 'outputs/release/manifest.json'
    if (path.parent / 'receipt.json').exists():
        old = json.loads(path.read_text())
        if old != manifest:
            raise ValueError('An earlier publication receipt exists; review it before replacing this release')
    stage = root / 'outputs/release/publish/outputs'
    stage.mkdir(parents=True, exist_ok=True)
    import shutil
    for name in files:
        if name in sidecars: (stage / name).write_bytes(sidecars[name])
        else: shutil.copy2(root / 'outputs' / name, stage / name)
    verify_publication(root, manifest)
    write_json(path, manifest)
    if config['uploader']:
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


def publish_repository(service, manifest, api, root=ROOT):
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
            release = api.request('POST', base + '/releases', {'tag_name': tag, 'target_commitish': manifest['commit'],
                                  'name': manifest['tag'], 'body': manifest['notes'], 'draft': True})
        asset_base = base + '/releases/' + str(release['id']) + '/assets'
        assets = {}; page = 1
        while True:
            entries = api.request('GET', asset_base + '?per_page=100&page=' + str(page))
            assets.update({a['name']: a for a in entries})
            if len(entries) < 100: break
            page += 1
        upload = urlsplit(release['upload_url'].split('{')[0])
        if upload.scheme != 'https' or upload.hostname != 'uploads.github.com':
            raise ValueError('Unexpected GitHub upload URL')
        for name, sha in manifest['files'].items():
            if digest(root / 'outputs' / name) != sha:
                raise ValueError('Local asset changed during publication: ' + name)
            if name in assets:
                if assets[name].get('digest') != 'sha256:' + sha:
                    raise ValueError('GitHub asset exists with different/unknown digest: ' + name)
                continue
            item = api.request('POST', upload.path + '?name=' + quote(name, safe=''),
                               file=root / 'outputs' / name, host=upload.hostname)
            if item.get('digest') != 'sha256:' + sha:
                raise ValueError('GitHub did not confirm uploaded asset checksum: ' + name)
        api.request('PATCH', base + '/releases/' + str(release['id']), {'draft': False})
        return release['html_url']
    links = []
    for name, sha in manifest['files'].items():
        if digest(root / 'outputs' / name) != sha:
            raise ValueError('Local asset changed during publication: ' + name)
        package_path = base + '/packages/generic/bitskim/' + quote(manifest['version'], safe='') + '/' + quote(name, safe='')
        existing = api.request('GET', package_path, optional=True, hash_body=True)
        if existing is not None and existing != sha:
            raise ValueError('GitLab package already exists with different bytes: ' + name)
        if existing is None:
            api.request('PUT', package_path, file=root / 'outputs' / name)
            if api.request('GET', package_path, hash_body=True) != sha:
                raise ValueError('GitLab upload verification failed: ' + name)
        links.append({'name': name, 'url': 'https://' + api.host + package_path, 'link_type': 'package'})
    if release is None:
        release = api.request('POST', base + '/releases', {'tag_name': tag, 'name': manifest['tag'],
                              'description': manifest['notes'], 'assets': {'links': links}})
    else:
        current = {a['name']: a for a in release.get('assets', {}).get('links', [])}
        for link in links:
            if link['name'] in current:
                if current[link['name']]['url'] != link['url']:
                    raise ValueError('GitLab release link differs: ' + link['name'])
            else:
                api.request('POST', release_path + '/assets/links', link)
    return release.get('_links', {}).get('self', manifest['gitlab'] + '/-/releases/' + tag)


def publish(services, confirmed=False, root=ROOT, config=None, tag=None, tag_message=None):
    if not confirmed:
        raise ValueError('No upload performed. Review outputs/release/manifest.json, then use --confirm-publish')
    path = root / 'outputs/release/manifest.json'
    manifest = json.loads(path.read_text())
    if tag_message is not None and tag_message != manifest.get('tag_message', 'BitSkim ' + manifest['tag']):
        raise ValueError('Tag message differs from the prepared manifest; prepare again with --tag-message')
    if release_tag(manifest['tag']) != manifest['tag'] or manifest['version'] != '0.2' or (tag is not None and release_tag(tag) != manifest['tag']):
        raise ValueError('Version/tag mismatch')
    if clean_commit(root) != manifest['commit'] or source_hash(root) != manifest['source_sha256']:
        raise ValueError('Source/commit changed; build and prepare again')
    check_builds(root)
    check_source_archive(root)
    if inventory(root) != manifest.get('inventory', manifest['files']):
        raise ValueError('Release assets changed; prepare again')
    publication_root = verify_publication(root, manifest) if 'inventory' in manifest else root
    apis = {}
    # Preflight every selected repository and tag before any push or upload.
    for service in services:
        if service == 'aminet':
            from aminet_release import check
            check(root / 'outputs/aminet', root)
            if (root / 'outputs/aminet/upload-receipt.json').exists():
                receipt = json.loads((root / 'outputs/aminet/upload-receipt.json').read_text())
                if receipt.get('status') != 'transferred-awaiting-aminet-review':
                    raise ValueError('Review the partial Aminet receipt before retrying')
            continue
        host, project = repository(manifest[service], service == 'github')
        if config is not None:
            if manifest[service] != config[service] or git_destination(manifest, service) != git_destination(config, service):
                raise ValueError('Destination changed; prepare again')
        token = credential(service, config or {})
        api = API('api.github.com' if service == 'github' else host, token, service == 'github')
        api.request('GET', '/repos/' + project if service == 'github' else '/api/v4/projects/' + quote(project, safe=''))
        current = remote_tag(git_destination(manifest, service), manifest['tag'], root)
        if current is not None and current != manifest['commit']:
            raise ValueError(service + ': remote tag points at a different commit; refusing to move it')
        apis[service] = api
    tag = manifest['tag']
    local = run(['git', 'tag', '--list', tag], root, True)
    if local and run(['git', 'rev-parse', tag + '^{commit}'], root, True) != manifest['commit']:
        raise ValueError('Local release tag points at a different commit')
    receipt_path = path.parent / 'receipt.json'
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {'manifest_sha256': digest(path), 'completed': {}}
    if receipt['manifest_sha256'] != digest(path):
        raise ValueError('Publication receipt belongs to a different release manifest')
    write_json(receipt_path, receipt)
    if apis and not local:
        run(['git', 'tag', '-a', tag, '-m', manifest.get('tag_message', 'BitSkim ' + tag), manifest['commit']], root)
    for service in services:
        if service in receipt['completed']: continue
        receipt['in_progress'] = service; write_json(receipt_path, receipt)
        if service == 'aminet':
            from aminet_release import upload
            folder = root / 'outputs/aminet'
            if not (folder / 'upload-receipt.json').exists(): upload(folder, True, root)
            url = 'Aminet incoming: transferred, awaiting moderator review'
        else:
            run(['git', 'push', git_destination(manifest, service), 'refs/tags/' + tag + ':refs/tags/' + tag], root)
            if remote_tag(git_destination(manifest, service), tag, root) != manifest['commit']:
                raise ValueError('Remote tag verification failed: ' + service)
            url = publish_repository(service, manifest, apis[service], publication_root)
        receipt['completed'][service] = url; receipt.pop('in_progress', None); write_json(receipt_path, receipt)
        print(service + ': ' + url)


def build(config, root=ROOT, plan=False):
    allowed = {'BITSKIM_SIGN_IDENTITY', 'BITSKIM_RELEASE_PYTHON', 'AMIGA_CC', 'LHA', 'WINDOWS_CC', 'RISCOS_CC', 'RISCOS_SDK_SOURCE', 'WATCOM', 'DOS_RUNNER', 'PHP', 'AMIGA_DEV_BIN', 'BITSKIM_BUILDS_DIR', 'ATARI_CC', 'STING_INCLUDE', 'LEGACY_WINDOWS_CC', 'LEGACY_WINDOWS_RC', 'MAKENSIS', 'NSISDIR'}
    if set(config['env']) - allowed:
        raise ValueError('Only documented build settings belong in config; tokens belong in the environment')
    env = dict(os.environ, **config['env'])
    for key in ('GITHUB_TOKEN', 'GITLAB_TOKEN'): env.pop(key, None)
    if not env.get('BITSKIM_SIGN_IDENTITY'):
        raise ValueError('Set BITSKIM_SIGN_IDENTITY for the public Mac release')
    py = env.get('BITSKIM_RELEASE_PYTHON', sys.executable)
    commands = [[py, '-m', 'unittest', 'discover', '-s', 'tests', '-b'], ['./scripts/build-macos.sh'],
                [py, 'scripts/notarize-macos.py', 'macos', '--profile', config['notary_profile']],
                ['./scripts/build-windows.sh'], ['./scripts/build-amiga.sh'],
                [py, 'scripts/build-dos.py'], ['./scripts/build-riscos.sh'],
                ['./scripts/build-atari.sh'], ['./scripts/build-windows-legacy.sh'],
                ['./scripts/build-windows-installer.sh'], [py, 'scripts/build-macos-dmg.py'],
                [py, 'scripts/notarize-macos.py', 'dmg', '--profile', config['notary_profile']],
                [py, 'scripts/package.py'], [py, 'scripts/check-release-site.py']]
    if env.get('AMIGA_DEV_BIN') or (root / 'work/amiga-dev-bin.txt').exists():
        commands.append([py, 'scripts/sync-amiga-bin.py'])
    for command in commands:
        print(' '.join(command), flush=True)
        if not plan: run(command, root, env=env)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('source', 'plan', 'build', 'prepare', 'check', 'publish', 'release'))
    p.add_argument('--config', type=Path, default=Path(os.environ.get('BITSKIM_RELEASE_CONFIG', str(Path.home() / 'Development/tools/git-release/projects/bitskim.json'))))
    p.add_argument('--notes', type=Path, default=ROOT / 'docs/RELEASE-NOTES.md')
    p.add_argument('--to', default='gitlab,github', help='Comma-separated gitlab,github,aminet')
    p.add_argument('--confirm-publish', action='store_true')
    p.add_argument('--confirm-push', action='store_true')
    p.add_argument('--init', dest='initialize', action='store_true', help='Create the first local commit only after selected remotes are verified empty')
    p.add_argument('--dry-run', action='store_true')
    p.add_argument('--commit-message', help='Message for the first local commit with source --init')
    p.add_argument('--tag-message', help='Annotation for a newly created release tag')
    p.add_argument('--tag', help='Exact release tag, e.g. 0.2; default v0.2')
    a = p.parse_args(); config = configuration(a.config)
    if a.commit_message is not None and not (a.action == 'source' and a.initialize): p.error('--commit-message requires source --init')
    if a.tag_message is not None and a.action not in ('prepare', 'publish', 'release'): p.error('--tag-message is for prepare/publish/release')
    if (a.dry_run or a.initialize) and a.action != 'source': p.error('--dry-run and --init are for source only')
    if a.action == 'source': push_source(config, a.to.split(','), a.confirm_push, a.dry_run, initialize=a.initialize, commit_message=a.commit_message)
    elif a.action in ('plan', 'build'): build(config, plan=a.action == 'plan')
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
