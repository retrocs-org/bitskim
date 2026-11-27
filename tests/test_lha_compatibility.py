"""Amiga level-0 paths must survive Unix LHa and OLHa extraction unchanged."""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import amiga_release as release


def entry(name,data=b'payload\xffBitSkim\xffnested',level=0):
    body=b'-lh0-'+struct.pack('<II',len(data),len(data))+b'\xff\x01\x02\x03'+bytes((0x20,level,len(name)))+name+b'\xff\xff'
    return bytes((len(body),sum(body)&255))+body+data


class LHAHeaders(unittest.TestCase):
    def test_only_filename_separators_and_header_sums_change(self):
        names=[b'BitSkim.info',b'BitSkim\xffARexx\xffSend.rexx',b'BitSkim\xffempty']
        original=b''.join(entry(name,b'' if name.endswith(b'empty') else b'payload\xffBitSkim\xffnested') for name in names)+b'\0'
        expected=b''.join(entry(name.replace(b'\xff',b'/'),b'' if name.endswith(b'empty') else b'payload\xffBitSkim\xffnested') for name in names)+b'\0'
        with tempfile.TemporaryDirectory() as temporary:
            archive=Path(temporary)/'package.lha';archive.write_bytes(original)
            inventory=[name.replace(b'\xff',b'/').decode() for name in names]
            self.assertEqual(release.normalize_lha_paths(archive,inventory),2)
            self.assertEqual(archive.read_bytes(),expected)
            self.assertEqual(release.normalize_lha_paths(archive,inventory),0)
            self.assertEqual(archive.read_bytes(),expected)

    def test_malformed_or_unexpected_archive_is_never_rewritten(self):
        valid=entry(b'BitSkim\xffInstall')+b'\0'
        checksum=bytearray(valid);checksum[1]^=1
        packed=bytearray(valid);packed[7:11]=struct.pack('<I',0xffffffff);packed[1]=sum(packed[2:packed[0]+2])&255
        cases=[bytes(checksum),bytes(packed),valid[:-1],valid[:-3],valid+b'trailer',b'\x01',
               entry(b'BitSkim\xffInstall',level=1)+b'\0',entry(b'Other')+b'\0',
               entry(b'BitSkim\xffInstall')*2+b'\0',entry(b'BitSkim\xffbad\x80')+b'\0']
        with tempfile.TemporaryDirectory() as temporary:
            archive=Path(temporary)/'package.lha'
            for data in cases:
                with self.subTest(data=data[:26]):
                    archive.write_bytes(data)
                    with self.assertRaises(RuntimeError):release.normalize_lha_paths(archive,['BitSkim/Install'])
                    self.assertEqual(archive.read_bytes(),data)
            archive.write_bytes(valid)
            with self.assertRaisesRegex(RuntimeError,'inventory'):
                release.normalize_lha_paths(archive,['BitSkim/Install','BitSkim/missing'])
            self.assertEqual(archive.read_bytes(),valid)

    def test_nested_package_extracts_identically_with_both_readers(self):
        olha=shutil.which(os.environ.get('OLHA','olha'))
        if not olha:self.skipTest('Set OLHA to a host OLHa build for the independent-reader check')
        try:writer=release.writer()
        except RuntimeError:self.skipTest('A compression-capable LHa writer is not installed')
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);files={}
            for name,data in [('BitSkim.info',b'icon'),('BitSkim/ARexx/Send.rexx',bytes(range(256))*8),('BitSkim/empty',b'')]:
                source=root/'inputs'/name;source.parent.mkdir(parents=True,exist_ok=True);source.write_bytes(data);files[name]=source
            with patch.object(release,'verify'),patch.object(release,'payload',return_value=files),patch.dict(os.environ,{'LHA_READER':writer}):
                archive=release.write_lha(root/'package.lha',root)
            for label,reader in [('unix-lha',writer),('olha',olha)]:
                with self.subTest(reader=label):
                    destination=root/label;destination.mkdir()
                    subprocess.run([reader,'t',str(archive)],check=True,stdout=subprocess.DEVNULL)
                    subprocess.run([reader,'x',str(archive)],cwd=destination,check=True,stdout=subprocess.DEVNULL)
                    actual={p.relative_to(destination).as_posix():p.read_bytes() for p in destination.rglob('*') if p.is_file()}
                    self.assertEqual(actual,{name:path.read_bytes() for name,path in files.items()})
