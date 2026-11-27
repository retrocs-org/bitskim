#!/usr/bin/env python3
"""Build a genuine segmented Win16 NE executable with shared Open Watcom."""
import argparse,os,struct,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--watcom',default=os.environ.get('WATCOM',str(Path.home()/'Development/tools/msdos/toolchain/open-watcom')));a=p.parse_args();tools=Path(a.watcom).resolve()
compiler=tools/('armo64' if __import__('platform').machine()=='arm64' else 'binl64')/'wcl'
if not compiler.is_file():p.error('Install Open Watcom in the shared tools directory or set --watcom')
from build_identity import write_identity
identity=write_identity(ROOT)
out=ROOT/'build/windows16';out.mkdir(parents=True,exist_ok=True);env=dict(os.environ,WATCOM=str(tools),INCLUDE=str(tools/'h/win')+':'+str(tools/'h'),PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
subprocess.run([str(compiler),'-bt=windows','-l=windows','-0','-ml','-k16384','-zW','-os','-s','-fi='+str(identity),'-dBS_WIN16','-i='+str(ROOT/'core'),'-fe=BITSKIM.EXE',*[str(ROOT/name) for name in ('clients/windows16/main.c','clients/windows16/platform.c','core/bitskim.c')],'winsock.lib','commdlg.lib'],cwd=out,env=env,check=True)
exe=out/'BITSKIM.EXE';data=exe.read_bytes();offset=struct.unpack_from('<I',data,0x3c)[0]
if data[:2]!=b'MZ' or data[offset:offset+2]!=b'NE':raise SystemExit('Expected a genuine Win16 NE executable')
modules=[]
count=struct.unpack_from('<H',data,offset+0x1e)[0];refs=offset+struct.unpack_from('<H',data,offset+0x28)[0];names=offset+struct.unpack_from('<H',data,offset+0x2a)[0]
for i in range(count):
 at=names+struct.unpack_from('<H',data,refs+i*2)[0];modules.append(data[at+1:at+1+data[at]].decode('ascii'))
if any(name.upper() not in {'KERNEL','USER','GDI','WINSOCK','COMMDLG','KEYBOARD','SOUND','SYSTEM'} for name in modules):raise SystemExit('Unexpected Win16 imports: '+repr(modules))
print('Imports: '+', '.join(modules))
print('PASS Win16 NE, 8086 large model, Winsock 1.1 (native acceptance pending)');print(exe)
