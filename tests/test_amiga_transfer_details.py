"""Check the production Amiga formatter against its fixed-width status line."""
import ctypes,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class AmigaTransferDetails(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory(); folder=Path(cls.tmp.name)
        source=(ROOT/'clients/amiga/main.c').read_text()
        helpers=source[source.index('static void transfer_size('):source.index('#include "transfer_panel.h"')]
        (folder/'format.c').write_text('#include <stdio.h>\n#include <stdint.h>\n'+helpers+'\nvoid format_details(unsigned long done,unsigned long total,unsigned long rate,char *out) {transfer_details(done,total,rate,out);}\n')
        subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',str(folder/'format.c'),'-o',str(folder/'format.so')],check=True)
        cls.lib=ctypes.CDLL(str(folder/'format.so'))
        cls.lib.format_details.argtypes=[ctypes.c_ulong]*3+[ctypes.c_char_p]
    @classmethod
    def tearDownClass(cls): cls.tmp.cleanup()
    def render(self,done,total,rate):
        out=ctypes.create_string_buffer(100);self.lib.format_details(done,total,rate,out);return out.value.decode()
    def test_amounts_percentage_and_speed(self):
        self.assertEqual(self.render(1024,2048,512),'1.0 KiB / 2.0 KiB | 50% | 1.0 KiB left | 512 B/s')
        self.assertIn('0% | 0 B left | 0 B/s',self.render(0,0,0))
        self.assertIn('100% | 0 B left',self.render(11,10,0))
    def test_maximum_file_sizes_fit_the_workbench_status_line(self):
        values=(0,1,1023,1024,1048575,1048576,1073741823,1073741824,2147483647)
        for total in values:
            for done in (0,total//2,total):
                for rate in values:
                    line=self.render(done,total,rate)
                    self.assertLessEqual(len(line),69,line)
