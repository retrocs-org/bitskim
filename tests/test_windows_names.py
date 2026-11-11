"""Exercise Windows filename rules independently of the host filesystem."""
import ctypes, subprocess, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class WindowsNames(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        source=Path(cls.tmp.name)/'names.c'; library=source.with_suffix('.so')
        source.write_text('#include "windows-names.h"\nint supported(const wchar_t *s) { return windows_name_supported(s); }\n')
        subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),str(source),'-o',str(library)],check=True)
        cls.lib=ctypes.CDLL(str(library)); cls.lib.supported.argtypes=[ctypes.c_wchar_p]
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def test_devices_and_invalid_characters(self):
        for name in ('CON','con.txt','NUL','COM1.txt','LPT9','COM¹.bin','CON .txt','CONIN$','a:b','a/b','a\\b','x?','x*','x.','x ','','a\t.txt','.'*256):
            self.assertEqual(self.lib.supported(name),0,name)
    def test_ordinary_unicode_names(self):
        for name in ('hello.txt','COM10.txt','.gitignore','å-λ.bin','console.txt','a'*255):
            self.assertEqual(self.lib.supported(name),1,name)
