#!/usr/bin/env python3
"""Fail closed on accidental modern CRT/network imports; not a runtime test."""
import re,struct,subprocess,sys
from pathlib import Path

def check(path):
    data=Path(path).read_bytes(); pe=struct.unpack_from('<I',data,0x3c)[0]
    assert data[pe:pe+4]==b'PE\0\0'
    assert struct.unpack_from('<H',data,pe+4)[0]==0x14c,'Not x86'
    optional=pe+24
    assert struct.unpack_from('<H',data,optional)[0]==0x10b,'Not PE32'
    assert struct.unpack_from('<HH',data,optional+48)==(4,0),'Requires newer subsystem'
    assert struct.unpack_from('<II',data,optional+96+9*8)==(0,0),'Unexpected static TLS'
    output=subprocess.check_output(['i686-w64-mingw32-objdump','-p',str(path)],text=True)
    dlls={d.lower() for d in re.findall(r'DLL Name: (\S+)',output)}
    assert dlls=={'kernel32.dll','user32.dll','gdi32.dll','comdlg32.dll','shell32.dll','msvcrt.dll','wsock32.dll'},dlls
    for symbol in ('GetTickCount64','GetAdaptersAddresses','_time64','_stat64','__acrt_iob_func','InitializeCriticalSectionEx'):
        assert symbol not in output,symbol
    print('PASS: x86 PE32 / subsystem 4.0 / no TLS / legacy DLL import gate')
if __name__=='__main__': check(sys.argv[1])
