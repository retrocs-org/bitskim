"""Inspect the actual cross-built PE, including failure on a modern subsystem."""
import importlib.util,struct,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('legacy_check',ROOT/'scripts/check-windows-legacy.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class LegacyImage(unittest.TestCase):
    def image(self):
        p=ROOT/'build/windows-legacy/BitSkim.exe'
        if not p.exists(): self.skipTest('Build Windows Classic first')
        return p
    def test_actual_import_gate(self): module.check(self.image())
    def test_reject_new_subsystem(self):
        data=bytearray(self.image().read_bytes()); pe=struct.unpack_from('<I',data,0x3c)[0];struct.pack_into('<H',data,pe+24+48,6)
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'new.exe';p.write_bytes(data)
            with self.assertRaises(AssertionError): module.check(p)
