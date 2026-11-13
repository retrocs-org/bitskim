#!/usr/bin/env python3
"""Package verified local builds without changing the release version."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
import hashlib, shutil, struct, os, time, json, argparse
from build_provenance import verify,StaleBuild
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--primary-only',action='store_true',help='Package macOS, Amiga and modern Windows only; omit optional RISC OS builds explicitly')
args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[1]
assert (ROOT/'VERSION').read_text().strip()=='0.2', 'Version is locked at 0.2'
# Verify every existing published target before changing any package/download.
checks=['macos','amiga','windows']
if not args.primary_only:
    if (ROOT/'build/windows-legacy/BitSkim.exe').exists(): checks.append('windows-legacy')
    for variant in ('sting','mintnet','mintnet060'):
        if (ROOT/'build/atari'/variant/'BITSKIM.PRG').exists(): checks.append('atari-'+variant)
if (ROOT/'build/dos/BITSKIM.EXE').exists():checks.append('dos')
for cpu in (() if args.primary_only else ('arm2','strongarm')):
    if (ROOT/'build/riscos'/cpu/'!BitSkim/!RunImage').exists(): checks.append('riscos-'+cpu)
for target,name in [('dmg','BitSkim-0.2-macOS-arm64.dmg'),('setup','BitSkim-0.2-Windows10-x64-Setup.exe')]:
    if (ROOT/'outputs'/name).exists(): checks.append(target)
try:
    for target in checks: verify(target,ROOT)
except StaleBuild as error:
    raise SystemExit('Packaging refused: '+str(error))
OUT=ROOT/'outputs'; OUT.mkdir(exist_ok=True)
DOWNLOADS=ROOT/'public-web/downloads'; DOWNLOADS.mkdir(exist_ok=True)
MAC=ROOT/'build/macos/BitSkim.app'; AMIGA=ROOT/'build/amiga/BitSkim'
WINDOWS=ROOT/'build/windows/BitSkim.exe'
if not (MAC/'Contents/MacOS/BitSkim').is_file() or not AMIGA.is_file() or not WINDOWS.is_file() or not (ROOT/'build/amiga/StartInstaller').is_file():
    raise SystemExit('Build all three clients before packaging')
def add_tree(z,folder,prefix,exclude=()):
    for path in sorted(folder.rglob('*')):
        if path.is_file() and not any(part in exclude for part in path.relative_to(folder).parts):
            z.write(path,str(Path(prefix)/path.relative_to(folder)))
maczip=OUT/'BitSkim-0.2-macOS-arm64.zip'
with ZipFile(maczip,'w',ZIP_DEFLATED) as z:
    z.write(ROOT/'build/provenance/macos.json','BUILD-PROVENANCE.json')
    add_tree(z,MAC,'BitSkim.app')
    z.write(ROOT/'docs/TRANSFER-TOOLS.md','docs/TRANSFER-TOOLS.md'); z.write(ROOT/'docs/TRANSFERS.md','docs/TRANSFERS.md'); z.write(ROOT/'README.MD','README.MD'); z.write(ROOT/'LICENSE','LICENSE'); z.write(ROOT/'assets/themes/CATPPUCCIN-LICENSE','CATPPUCCIN-LICENSE'); z.write(ROOT/'THIRD-PARTY-NOTICES.MD','THIRD-PARTY-NOTICES.MD')
from amiga_release import write_zip,write_lha,writer
amigazip=write_zip(OUT/'BitSkim-0.2-AmigaOS.zip',ROOT)
amigaextras=[]
try: writer()
except RuntimeError:
    if (OUT/'BitSkim-0.2.lha').exists():raise SystemExit('Rebuilding the existing LHA requires a writer: run scripts/bootstrap-lha.sh')
else:
    amigaextras.append(write_lha(OUT/'BitSkim-0.2.lha',ROOT))
    # Aminet preparation checks this sidecar as well as the full payload.
    lha=amigaextras[-1]
    lha.with_suffix('.lha.sha256').write_text(hashlib.sha256(lha.read_bytes()).hexdigest()+'  '+lha.name+'\n')
winzip=OUT/'BitSkim-0.2-Windows10-x64.zip'
with ZipFile(winzip,'w',ZIP_DEFLATED) as z:
    z.write(ROOT/'build/provenance/windows.json','BitSkim/BUILD-PROVENANCE.json')
    z.write(WINDOWS,'BitSkim/BitSkim.exe')
    z.write(ROOT/'build/windows/BitSkim-CoreCheck.exe','BitSkim/Tools/BitSkim-CoreCheck.exe')
    for source, target in [('docs/TRANSFER-TOOLS.md','TRANSFER-TOOLS.md'),('docs/TRANSFERS.md','TRANSFERS.md'),('assets/licenses/OpenSSL-LICENSE','OpenSSL-LICENSE'),('docs/WINDOWS.md','README.MD'),('LICENSE','LICENSE'),('THIRD-PARTY-NOTICES.MD','THIRD-PARTY-NOTICES.MD'),('assets/themes/CATPPUCCIN-LICENSE','CATPPUCCIN-LICENSE')]:
        z.write(ROOT/source,'BitSkim/'+target)
from source_release import write_source
sourcezip,sourcetar=write_source(OUT,checks,ROOT)
from riscos_release import write_riscos
riscoszips=[] if args.primary_only else write_riscos(OUT,sourcezip,ROOT)
from dos_release import write_dos
doszips=[write_dos(OUT/'BitSkim-0.2-DOS-mTCP.zip',sourcezip,ROOT)] if 'dos' in checks else []
from atari_release import write_atari
from windows_legacy_release import write_windows_legacy
experimental=[]
if 'atari-sting' in checks: experimental.extend(write_atari(OUT,sourcezip,ROOT))
if 'windows-legacy' in checks: experimental.append(write_windows_legacy(OUT/'BitSkim-0.2-Windows-Classic-x86.zip',ROOT))
installer_files=[p for p in (OUT/'BitSkim-0.2-macOS-arm64.dmg',OUT/'BitSkim-0.2-Windows10-x64-Setup.exe') if p.is_file()]
for file in (maczip,amigazip,*amigaextras,winzip,sourcezip,sourcetar,*riscoszips,*doszips,*experimental,*installer_files): shutil.copy2(file,DOWNLOADS/file.name)
# Website archives carry a checksum list for their actual downloadable packages.
client_files=(maczip,amigazip,*amigaextras,winzip,sourcezip,sourcetar,*riscoszips,*doszips,*experimental,*installer_files)
(DOWNLOADS/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in client_files))
webzip=OUT/'BitSkim-0.2-website.zip'
with ZipFile(webzip,'w',ZIP_DEFLATED) as z: add_tree(z,ROOT/'public-web','public-web')
files=(maczip,amigazip,*amigaextras,winzip,sourcezip,sourcetar,*riscoszips,*doszips,*experimental,*installer_files,webzip)
(OUT/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in files))
shutil.copy2(OUT/'SHA256SUMS',DOWNLOADS/'SHA256SUMS')
for p in files: print(f'{p.name}: {p.stat().st_size:,} bytes')

# Refresh the configured local compiled-package mirror after successful packaging.
import subprocess,sys
subprocess.run([sys.executable,str(Path(__file__).resolve().parent/'sync-builds.py')],check=True)
