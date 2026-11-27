"""Linux payload inventories and build provenance survive packaging unchanged."""
import json
import sys
import tarfile
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from build_provenance import begin, finish, StaleBuild
from linux_release import package, verify_archive


class LinuxPackages(unittest.TestCase):
    def test_desktop_cli_archives_and_tamper_rejection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'VERSION').write_text('0.2\n')
            sources = ['docs/LINUX.md', 'docs/TRANSFERS.md', 'LICENSE',
                       'THIRD-PARTY-NOTICES.MD', 'assets/themes/CATPPUCCIN-LICENSE',
                       'assets/licenses/QRCODEGEN-LICENSE',
                       'clients/linux/org.bitskim.BitSkim.desktop',
                       'public-web/assets/bitskim.png']
            for name in sources:
                destination = root / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes((ROOT / name).read_bytes())
            build = root / 'build/linux/arm64'
            build.mkdir(parents=True)
            (build / 'DEPENDENCIES.json').write_text('{"architecture":"arm64"}\n')
            for target, binary in [('linux-arm64', 'bitskim'), ('linux-arm64-cli', 'bitskim-cli')]:
                (build / binary).write_bytes(b'checked payload ' + binary.encode())
                begin(target, root)
                finish(target, root)
            before = int(time.time())
            desktop = package(root / 'outputs', 'arm64', root=root)
            with tarfile.open(desktop) as archive:
                prefix = desktop.name.removesuffix('.tar.gz')
                self.assertTrue(all(before <= member.mtime <= int(time.time()) for member in archive))
                self.assertEqual(archive.extractfile(prefix + '/bin/bitskim-cli').read(),
                                 (build / 'bitskim-cli').read_bytes())
                self.assertEqual(archive.getmember(prefix + '/bin/bitskim').mode, 0o755)
                self.assertEqual(archive.getmember(prefix + '/install.sh').mode, 0o755)
                records = json.load(archive.extractfile(prefix + '/share/doc/bitskim/BUILD-PROVENANCE.json'))
                self.assertEqual(set(records), {'linux-arm64', 'linux-arm64-cli'})
                self.assertFalse(any('identity.pem' in name or name.endswith('.c') for name in archive.getnames()))
            cli = package(root / 'outputs', 'arm64', cli_only=True, root=root)
            with tarfile.open(cli) as archive:
                self.assertFalse(any(name.endswith('/bitskim') or name.endswith('.desktop') for name in archive.getnames()))
            original = cli.read_bytes()
            with tarfile.open(cli) as archive:
                members = [(member, archive.extractfile(member).read()) for member in archive]
            import io
            with tarfile.open(cli, 'w:gz') as archive:
                for member, data in members:
                    if member.name.endswith('/bin/bitskim-cli'):
                        data = b'wrong binary with a new archive checksum'
                        member.size = len(data)
                    archive.addfile(member, io.BytesIO(data))
            with self.assertRaisesRegex(RuntimeError, 'differs'):
                verify_archive(cli, 'arm64', cli_only=True, root=root)
            cli.write_bytes(original)
            (build / 'bitskim-cli').write_bytes(b'tampered')
            with self.assertRaisesRegex(StaleBuild, 'artifact changed'):
                package(root / 'outputs', 'arm64', cli_only=True, root=root)
            self.assertEqual(cli.read_bytes(), original)
