#!/usr/bin/env python3
"""Build 16-bit DOS with Open Watcom, on Windows or inside a private DOS runtime drive.
No personal DOS runtime config, disk images or test profiles are used.
"""
import argparse,os,shutil,subprocess
from pathlib import Path
from build_provenance import ROOT,begin,finish,file_hash,write_json,record_path
p=argparse.ArgumentParser();p.add_argument('--native',action='store_true',help='Run installed Watcom compiler under Windows instead of DOS runtime');p.add_argument('--watcom',default=os.environ.get('WATCOM',str(ROOT/'work/dos-tools/watcom')));p.add_argument('--runner',default=os.environ.get('DOS_RUNNER',str(ROOT/'work/dos-tools/runner-image/4.arm64')));a=p.parse_args()
watcom=Path(a.watcom).resolve();runner=Path(a.runner).resolve(); drive=ROOT/'work/dos-build';stage=drive/'build';stage.mkdir(parents=True,exist_ok=True)
# The drive contains only this project's disposable build inputs and compiler.
link=drive/'watcom'
if not a.native and not link.exists():link.symlink_to(watcom,target_is_directory=True)
mtcp=drive/'mtcp'
if mtcp.exists():shutil.rmtree(mtcp)
shutil.copytree(ROOT/'third_party/mtcp/src',mtcp)
for obj in stage.glob('*.obj'):obj.unlink()
for name in ('bitskim.c','bitskim.h','platform.h','dos-net.h','queue.h','folders.h','netcheck.h','batch.h','caps.h','rate.h','activity.h','trust.h','fs.h','storage.h','run.h','treeq.h','tree.h','resume.h','tls-api.h'):shutil.copy2(ROOT/'core'/name,stage/name)
for path in (ROOT/'clients/dos').iterdir():
 if path.is_file():shutil.copy2(path,stage/path.name)
units=('packet','arp','eth','ip','tcp','tcpsockm','udp','utils','timer','trace')
for unit in units:shutil.copy2(mtcp/'TCPLIB'/(unit.upper()+'.CPP'),stage/(unit+'.cpp'))
shutil.copy2(mtcp/'TCPLIB/IPASM.ASM',stage/'ipasm.asm')
flags='-0 -ml -os -s -ei -zp2 -fi=bsdos.cfg'
batch=['@echo off','set WATCOM=C:\\WATCOM','set PATH=C:\\WATCOM\\BINW','set INCLUDE=C:\\WATCOM\\H;C:\\MTCP\\TCPINC;C:\\MTCP\\INCLUDE;C:\\BUILD','cd \\build','echo Compiling > build.log']
for name in ('bitskim','files','main'):
 batch += [f'wcc {name}.c -0 -ml -os -s -dBS_DOS >> build.log','if errorlevel 1 goto failed']
for name in ('net',)+units:
 batch += [f'wpp {name}.cpp {flags} -dBS_DOS >> build.log','if errorlevel 1 goto failed']
batch += ['wasm -0 -ml ipasm.asm >> build.log','if errorlevel 1 goto failed','wlink @link.lnk >> build.log','if errorlevel 1 goto failed','echo success > result.txt','goto done',':failed','echo failed > result.txt',':done','exit']
if a.native:
 batch[1:4]=['set WATCOM='+str(watcom),'set PATH='+str(watcom/'binnt')+';%PATH%','set INCLUDE='+str(watcom/'h')+';'+str(mtcp/'TCPINC')+';'+str(mtcp/'INCLUDE')+';'+str(stage)]
 batch[4]='cd /d '+str(stage)
 batch[-1]='exit /b'
(stage/'build.bat').write_bytes(('\r\n'.join(batch)+'\r\n').encode('ascii'))
(stage/'link.lnk').write_text('system dos\noption stack=16384\noption map=bitskim.map\noption eliminate\nname bitskim.exe\nfile '+','.join(x+'.obj' for x in ('bitskim','files','main','net')+units+('ipasm',))+'\n')
config=drive/'build.conf';config.write_text('[sdl]\noutput=texture\n[mixer]\nnosound=true\n[cpu]\ncycles=max\n[autoexec]\nmount c "'+str(drive)+'"\nc:\ncall c:\\build\\build.bat\nexit\n')
result=stage/'result.txt'
if result.exists():result.unlink()
begin('dos')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
with (drive/'test system.log').open('w') as log:
 command=['cmd.exe','/c',str(stage/'build.bat')] if a.native else [str(runner),'--noprimaryconf','--nolocalconf','--conf',str(config)]
 subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=240)
print((stage/'build.log').read_text(errors='replace'))
for error in stage.glob('*.ERR'):print(error.read_text(errors='replace'))
if not result.exists() or result.read_text().strip()!='success':raise SystemExit('DOS build failed; see work/dos-build/build/build.log')
out=ROOT/'build/dos';out.mkdir(parents=True,exist_ok=True)
for name in ('bitskim.exe','bitskim.map'):shutil.copy2(stage/name,out/name.upper())
record=finish('dos')
record['toolchain']={'executable':'Open Watcom wcc/wpp/wasm/wlink (DOS)','compiler_sha256':file_hash(watcom/('binnt/wcc.exe' if a.native else 'binw/wcc.exe')),'version':(stage/'build.log').read_text(errors='replace').splitlines()[2]}
write_json(record_path(ROOT,'dos'),record)
print('Built '+str(out/'BITSKIM.EXE'))
