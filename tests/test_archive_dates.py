"""Release archives use packaging dates even for old sources/generated entries."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
from unittest.mock import patch
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from archive_dates import PackageZipFile, current_timestamp, stamp_tree
import amiga_release
import source_release
from riscos_release import acorn_add


class ArchiveDates(unittest.TestCase):
    def test_zip_old_sources_and_generated_entries_keep_current_dates_and_modes(self):
        timestamp = int(time.time()) & ~1
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'tool'
            source.write_bytes(b'unchanged executable')
            source.chmod(0o755)
            os.utime(source, (0, 0))
            generated = ZipInfo('generated.txt')  # Its default date is 1980.
            generated.extra = b'\xff\xff\x00\x00'
            generated.external_attr = 0o100644 << 16
            with patch('archive_dates.time.time', return_value=timestamp), patch.dict(os.environ, {'SOURCE_DATE_EPOCH': '0'}):
                with PackageZipFile(root / 'package.zip', 'w', ZIP_DEFLATED) as archive:
                    archive.write(source, 'tool')
                    archive.writestr(generated, b'generated contents')
                    archive.writestr('notes.txt', 'release notes')
            with ZipFile(root / 'package.zip') as archive:
                self.assertTrue(all(item.date_time == time.localtime(timestamp)[:6] for item in archive.infolist()))
                self.assertEqual(archive.read('tool'), source.read_bytes())
                self.assertEqual(archive.getinfo('tool').external_attr >> 16 & 0o777, 0o755)
                self.assertEqual(archive.getinfo('tool').compress_type, ZIP_DEFLATED)
                self.assertEqual(archive.getinfo('generated.txt').extra, generated.extra)
            self.assertEqual(source.stat().st_mtime, 0)
            self.assertEqual(generated.date_time[0], 1980)

    def test_source_zip_and_tar_use_one_packaging_time(self):
        timestamp = int(time.time()) & ~1
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('README.MD', 'CHANGELOG.MD', 'LICENSE', 'LICENSE.md', 'THIRD-PARTY-NOTICES.MD',
                         'VERSION', 'CONTRIBUTING.md', '.gitignore', 'Makefile'):
                path = root / name
                path.write_text('source fixture')
                os.utime(path, (0, 0))
            with patch('archive_dates.time.time', return_value=timestamp):
                source_zip, source_tar = source_release.write_source(root / 'outputs', targets=(), root=root)
            with ZipFile(source_zip) as archive, tarfile.open(source_tar) as tar:
                self.assertTrue(all(item.date_time == time.localtime(timestamp)[:6] for item in archive.infolist()))
                self.assertTrue(all(item.mtime == timestamp for item in tar))
                self.assertEqual(archive.read('BitSkim/README.MD'), b'source fixture')
            self.assertEqual((root / 'README.MD').stat().st_mtime, 0)

    def test_lha_extraction_keeps_current_dates_without_touching_sources(self):
        try:
            tool = amiga_release.writer()
        except RuntimeError:
            self.skipTest('A compression-capable LHa writer is not installed')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'Install'
            source.write_bytes(b'installer fixture')
            os.utime(source, (315532800, 315532800))
            before = int(time.time()) - 2
            with patch.object(amiga_release, 'verify'), patch.object(amiga_release, 'payload', return_value={'BitSkim/Install': source}), patch.dict(os.environ, {'LHA_READER': tool}):
                archive = amiga_release.write_lha(root / 'package.lha', root)
            extracted = root / 'extracted'
            extracted.mkdir()
            subprocess.run([tool, 'x', str(archive)], cwd=extracted, check=True, stdout=subprocess.DEVNULL)
            received = extracted / 'BitSkim/Install'
            self.assertEqual(received.read_bytes(), source.read_bytes())
            self.assertGreaterEqual(received.stat().st_mtime, before)
            self.assertLessEqual(received.stat().st_mtime, int(time.time()) + 1)
            self.assertEqual(source.stat().st_mtime, 315532800)

    def test_riscos_filetype_and_native_date_match_zip_date(self):
        timestamp = int(time.time()) & ~1
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'image'
            source.write_bytes(b'run image')
            os.utime(source, (0, 0))
            with patch('archive_dates.time.time', return_value=timestamp):
                with PackageZipFile(root / 'riscos.zip', 'w', ZIP_DEFLATED) as archive:
                    acorn_add(archive, source, '!BitSkim/!RunImage', 0xff8)
            with ZipFile(root / 'riscos.zip') as archive:
                info = archive.getinfo('!BitSkim/!RunImage')
                tag, size, signature, load, execute, attributes, reserved = struct.unpack('<HH4sIIII', info.extra)
                self.assertEqual((tag, size, signature), (0x4341, 20, b'ARC0'))
                self.assertEqual(load >> 8 & 0xfff, 0xff8)
                self.assertEqual(((load & 255) << 32) | execute, (timestamp + 2208988800) * 100)
                self.assertEqual(info.date_time, time.localtime(timestamp)[:6])

    def test_staging_dates_skip_symbolic_links(self):
        timestamp = int(time.time())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stage = root / 'stage'
            stage.mkdir()
            (stage / 'tool').write_bytes(b'tool')
            outside = root / 'outside'
            outside.write_bytes(b'leave unchanged')
            os.utime(outside, (0, 0))
            (stage / 'link').symlink_to(outside)
            stamp_tree(stage, timestamp)
            self.assertEqual(stage.stat().st_mtime, timestamp)
            self.assertEqual((stage / 'tool').stat().st_mtime, timestamp)
            self.assertEqual(outside.stat().st_mtime, 0)

    def test_invalid_build_clock_is_rejected(self):
        with patch('archive_dates.time.time', return_value=0):
            with self.assertRaisesRegex(ValueError, 'correct the host clock'):
                current_timestamp()
