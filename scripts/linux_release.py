#!/usr/bin/env python3
"""Package verified Linux ELF clients without bundling system libraries."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile

from build_provenance import ROOT, verify
from release_paths import output_dir
from archive_dates import current_timestamp

ARCHES = ('arm64', 'x86_64')
INSTALL_SCRIPT = b'''#!/bin/sh
set -eu
cd "$(dirname "$0")"
prefix=${1:-/usr/local}
case "$prefix" in /*) ;; *) echo "Use an absolute install prefix" >&2; exit 1;; esac
mkdir -p "$prefix/bin" "$prefix/share/doc/bitskim"
cp -R bin/. "$prefix/bin/"
chmod 755 "$prefix/bin/bitskim-cli"
if [ -f bin/bitskim ]; then chmod 755 "$prefix/bin/bitskim"; fi
cp -R share/. "$prefix/share/"
echo "Installed BitSkim under $prefix"
'''


def targets(arch, cli_only=False):
    if arch not in ARCHES:
        raise ValueError('Unsupported Linux architecture')
    return ['linux-' + arch + '-cli'] + ([] if cli_only else ['linux-' + arch])


def stem(arch, cli_only=False):
    targets(arch, cli_only)
    return 'BitSkim-0.2-Linux-' + ('CLI-' if cli_only else '') + arch


def payload(arch, cli_only=False, root=ROOT):
    records = {name: verify(name, root) for name in targets(arch, cli_only)}
    files = {'bin/bitskim-cli': root / 'build/linux' / arch / 'bitskim-cli'}
    if not cli_only:
        files.update({
            'bin/bitskim': root / 'build/linux' / arch / 'bitskim',
            'share/applications/org.bitskim.BitSkim.desktop': root / 'clients/linux/org.bitskim.BitSkim.desktop',
            'share/icons/hicolor/512x512/apps/org.bitskim.BitSkim.png': root / 'public-web/assets/bitskim.png',
        })
    for source, name in [
        ('docs/LINUX.md', 'README.md'), ('docs/TRANSFERS.md', 'TRANSFERS.md'),
        ('LICENSE', 'LICENSE'), ('THIRD-PARTY-NOTICES.MD', 'THIRD-PARTY-NOTICES.MD'),
        ('assets/themes/CATPPUCCIN-LICENSE', 'CATPPUCCIN-LICENSE'),
        ('assets/licenses/QRCODEGEN-LICENSE', 'QRCODEGEN-LICENSE'),
        ('build/linux/' + arch + '/DEPENDENCIES.json', 'DEPENDENCIES.json'),
    ]:
        files['share/doc/bitskim/' + name] = root / source
    result = {}
    for name, path in sorted(files.items()):
        if path.is_symlink() or not path.is_file():
            raise RuntimeError('Missing/unsafe Linux payload: ' + str(path))
        result[name] = (path.read_bytes(), 0o755 if name.startswith('bin/') else 0o644)
    result['share/doc/bitskim/BUILD-PROVENANCE.json'] = ((json.dumps(records, indent=2) + '\n').encode(), 0o644)
    result['install.sh'] = (INSTALL_SCRIPT, 0o755)
    return result


def verify_archive(archive, arch, cli_only=False, root=ROOT):
    """Compare the entire archive against current builds, sources and permissions."""
    expected = {stem(arch, cli_only) + '/' + name: value
                for name, value in payload(arch, cli_only, root).items()}
    actual = {}
    with tarfile.open(archive) as source:
        for member in source:
            if (not member.isfile() or member.name not in expected or member.name in actual
                    or member.mode != expected[member.name][1]):
                raise RuntimeError('Unexpected Linux archive member: ' + member.name)
            stream = source.extractfile(member)
            digest = hashlib.sha256()
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
            actual[member.name] = digest.digest()
    hashes = {name: hashlib.sha256(data).digest() for name, (data, _) in expected.items()}
    if actual != hashes:
        raise RuntimeError('Linux archive differs from the checked build')
    return True


def package(out, arch, cli_only=False, root=ROOT):
    files = payload(arch, cli_only, root)
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    destination = out / (stem(arch, cli_only) + '.tar.gz')
    with tempfile.NamedTemporaryFile(dir=out, suffix='.tar.gz', delete=False) as temporary:
        temporary_path = Path(temporary.name)
    try:
        timestamp = current_timestamp()
        with tarfile.open(temporary_path, 'w:gz') as archive:
            for name, (data, mode) in files.items():
                info = tarfile.TarInfo(stem(arch, cli_only) + '/' + name)
                info.size, info.mode, info.mtime = len(data), mode, timestamp
                archive.addfile(info, io.BytesIO(data))
        verify_archive(temporary_path, arch, cli_only, root)
        temporary_path.replace(destination)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--arch', choices=ARCHES, required=True)
    parser.add_argument('--cli-only', action='store_true')
    args = parser.parse_args()
    out = output_dir(ROOT)
    archive = package(out, args.arch, args.cli_only)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (out / (archive.name + '.sha256')).write_text(digest + '  ' + archive.name + '\n')
    sums = out / 'SHA256SUMS'
    lines = sums.read_text().splitlines() if sums.exists() else []
    lines = [line for line in lines if line.split() and line.split()[-1].lstrip('*') != archive.name]
    lines.append(digest + '  ' + archive.name)
    sums.write_text('\n'.join(lines) + '\n')
    print('Verified Linux package: ' + str(archive))


if __name__ == '__main__':
    main()
