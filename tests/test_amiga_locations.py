"""Bounded literal companion paths and AmigaDOS launch quoting."""
from pathlib import Path
import ctypes,subprocess,tempfile,unittest,os
ROOT=Path(__file__).resolve().parents[1]
class Locations(unittest.TestCase):
 def test_literal_paths_bounds_and_quotes(self):
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);c=p/'test.c'
   c.write_text('#include <stdio.h>\n#include <string.h>\n#define BS_LOCATIONS_TEST\n#include "'+str(ROOT/'clients/amiga/locations.h')+'"\nint install_path(const char *p,char *out,size_t n){return amiga_install_path(p,out,n); }\nint read_path(const char *p,char *out,size_t n){return amiga_read_path(p,out,n);}\nint quote_path(char *out,size_t n,const char *p){return amiga_quote_path(out,n,p);}\n')
   subprocess.run(['cc','-shared','-fPIC','-std=c89','-Wall','-Wextra','-Werror',str(c),'-o',str(p/'test.so')],check=True)
   lib=ctypes.CDLL(str(p/'test.so'));lib.read_path.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t];lib.quote_path.argtypes=[ctypes.c_char_p,ctypes.c_size_t,ctypes.c_char_p]
   lib.install_path.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_size_t]
   out=ctypes.create_string_buffer(1024);file=p/'path'
   old=os.getcwd()
   try:
    os.chdir(p)
    Path('ENV:BitSkim').mkdir();Path('ENVARC:BitSkim').mkdir()
    Path('ENVARC:BitSkim/shared.path').write_bytes(b'Work:Shared BitSkim')
    self.assertEqual(lib.install_path(b'shared.path',out,len(out)),1)
    self.assertEqual(out.value,b'Work:Shared BitSkim')
    Path('ENV:BitSkim/shared.path').write_bytes(b'RAM:Live Shared')
    self.assertEqual(lib.install_path(b'shared.path',out,len(out)),1)
    self.assertEqual(out.value,b'RAM:Live Shared')
    Path('ENV:BitSkim/shared.path').write_bytes(b'bad\tpath')
    self.assertEqual(lib.install_path(b'shared.path',out,len(out)),1)
    self.assertEqual(out.value,b'Work:Shared BitSkim')
    self.assertEqual(lib.install_path(b'missing.path',out,len(out)),0)
   finally:os.chdir(old)
   for data,expected in [(b'Work:My Prefs/BitSkimPrefs',1),(b'Work:My "Prefs"/Bit*Skim\r\n',1),(b'Work:caf\xe9/BitSkimPrefs\n',1),(b'',0),(b'RAM:bad\tpath',0),(b'x'*1100,0)]:
    file.write_bytes(data);self.assertEqual(lib.read_path(str(file).encode(),out,len(out)),expected)
    if expected:self.assertEqual(out.value,data.rstrip(b'\r\n'))
   self.assertEqual(lib.quote_path(out,len(out),b'Work:My "Prefs"/Bit*Skim'),1)
   self.assertEqual(out.value,b'"Work:My *"Prefs*"/Bit**Skim"')
   self.assertEqual(lib.quote_path(out,4,b'Work:Prefs'),0)
   self.assertEqual(lib.quote_path(out,len(out),b'RAM:bad\nEcho'),0)
