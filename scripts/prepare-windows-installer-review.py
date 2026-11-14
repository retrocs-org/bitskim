#!/usr/bin/env python3
"""Build isolated installer lifecycle fixtures, never a public release installer."""
from pathlib import Path
from zipfile import ZipFile
import argparse,io,os,shutil,subprocess,tarfile
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--makensis',default=os.environ.get('MAKENSIS','makensis'))
 p.add_argument('--old-ref',default='0.1')
 p.add_argument('--old-archive',type=Path,default=ROOT/'outputs/BitSkim-0.1-Windows10-x64.zip')
 a=p.parse_args();out=ROOT/'build/windows-installer-review';out.mkdir(parents=True,exist_ok=True)
 stage=ROOT/'work/windows-installer-review';stage.mkdir(parents=True,exist_ok=True)
 old=stage/'old';new=stage/'new'
 for folder in (old,new):
  if folder.exists():shutil.rmtree(folder)
  folder.mkdir()
 raw=subprocess.check_output(['git','archive',a.old_ref],cwd=ROOT)
 with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
  for member in archive.getmembers():
   path=Path(member.name)
   if path.is_absolute() or '..' in path.parts or not (member.isdir() or member.isfile()):raise ValueError('Unsafe baseline archive member')
   target=old/path
   if member.isdir():target.mkdir(parents=True,exist_ok=True)
   else:target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(archive.extractfile(member).read())
 for name in ('assets','docs','installers'):shutil.copytree(ROOT/name,new/name)
 for name in ('LICENSE','THIRD-PARTY-NOTICES.MD'):shutil.copy2(ROOT/name,new/name)
 for folder in (old,new):(folder/'build/windows').mkdir(parents=True,exist_ok=True);(folder/'build/provenance').mkdir(parents=True,exist_ok=True)
 with ZipFile(a.old_archive) as archive:
  for src,dest in [('BitSkim/BitSkim.exe','build/windows/BitSkim.exe'),('BitSkim/Tools/BitSkim-CoreCheck.exe','build/windows/BitSkim-CoreCheck.exe'),('BitSkim/BUILD-PROVENANCE.json','build/provenance/windows.json')]:
   (old/dest).write_bytes(archive.read(src))
 for name in ('BitSkim.exe','BitSkim-CoreCheck.exe'):shutil.copy2(ROOT/'build/windows'/name,new/'build/windows'/name)
 shutil.copy2(ROOT/'build/provenance/windows.json',new/'build/provenance/windows.json')
 for version,folder in [('0.1',old),('0.2',new)]:
  script=folder/'installers/windows/BitSkim.nsi';text=script.read_text()
  # Exercise selecting the optional desktop shortcut, without changing release defaults.
  text=text.replace('Section /o "Desktop shortcut"','Section "Desktop shortcut"')
  text=text.replace('VIAddVersionKey "ProductName" "BitSkim"','VIAddVersionKey "ProductName" "${PRODUCT}"')
  script.write_text(text)
  subprocess.run([a.makensis,'-V2','-DPRODUCT=BitSkimInstallerTest','-DAPP_MUTEX=Local\\BitSkimInstallerTest-'+version,'-DPREVIOUS_APP_MUTEX=Local\\BitSkimInstallerTest-0.1','-DOUTPUT='+str(out/('InstallerReview-'+version+'.exe')),str(script)],check=True)
 shutil.copy2(ROOT/'tests/test-windows-installer.ps1',out/'test-windows-installer.ps1')
 launcher='@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-windows-installer.ps1" -OldSetup "%~dp0InstallerReview-0.1.exe" -Setup "%~dp0InstallerReview-0.2.exe" -Report "%~dp0installer-review.log"\r\necho Review complete. See installer-review.log beside this script.\r\npause\r\n'
 (out/'Run-installer-review.cmd').write_bytes(launcher.encode('ascii'))
 print('Prepared isolated native installer review:',out)
if __name__=='__main__':main()
