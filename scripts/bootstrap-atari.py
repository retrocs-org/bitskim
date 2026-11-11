#!/usr/bin/env python3
"""Pinned local macOS Atari cross SDK; installs nothing outside work/atari."""
from pathlib import Path
import hashlib,json,shutil,subprocess,urllib.request,tarfile
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'work/atari'
FILES={
'binutils-2.45-mint-20250812-bin-macos.tar.xz':'a8ed9e09d1696f321ec1734994b513aa36bc7cd3f926722d06f3a13a31c7f0e1',
'gcc-4.6.4-mint-20230719-bin-macos.tar.xz':'46f16756f8a9be8ffe57cf92029dcf0257be79fe3760af9aa1b04f9fbe2d5b69',
'mintlib-0.60.1-mint-20240718-dev.tar.xz':'60e5205cf11d94eaa024a4f11a39107833add073abdddbe1cb8bf46fc3cd0548',
'gemlib-0.44.0-mint-20240425-dev.tar.xz':'159cbf3eea2982b25c20cc5c8fa19df66c1b4245faa83560d5862d9615dee316',
'mintlib-0.60.1.tar.xz':'b48a7412fb5283d61711a01b0722fd8e9b0e2204e2233a69e21bf3b10d0c27dd',
'gemlib-0.44.0.tar.xz':'03574c60c246ed44ca5b6a01fc68565f0cec6ea63e7e5252ecabddcb2476925c',
'mintlib-0.60.1-20240718-build.sh':'77dbaed1c4c3bc0a49b998172b7efad25d356dc19c36c90a11bbf9edba90d889',
'gemlib-0.44.0-20240425-build.sh':'0f6fae86fd60a447c2398090929d033feafb88462d5749df8c740da43d28ba39'}
SDK_COMMIT='ab065dc94b01e9a5e29b584e2566a6552facd504'
def fetch(url,path,digest=None):
 if not path.exists():
  path.parent.mkdir(parents=True,exist_ok=True)
  with urllib.request.urlopen(url) as r,path.open('wb') as f:shutil.copyfileobj(r,f)
 if digest and hashlib.sha256(path.read_bytes()).hexdigest()!=digest:raise SystemExit('Checksum mismatch: '+str(path))
 return path
if __name__=='__main__':
 WORK.mkdir(exist_ok=True,parents=True);sdk=WORK/'sdk';sdk.mkdir(exist_ok=True)
 for name,digest in FILES.items():
  path=fetch('https://tho-otto.m68k.eu/download/mint/'+name,WORK/name,digest)
  if '-bin-' in name or '-dev.' in name:
   subprocess.run(['tar','-xJf',str(path),'-C',str(sdk)],check=True)
 prefix=sdk/'opt/cross-mint'
 shutil.copytree(sdk/'usr/m68k-atari-mint/sys-root',prefix/'m68k-atari-mint/sys-root',dirs_exist_ok=True)
 fetch('https://raw.githubusercontent.com/th-otto/STinG/'+SDK_COMMIT+'/include/transprt.h',WORK/'sting-include/transprt.h','e1063c53ba244c270b7d4b0495f11116dc2610d2ff063598c10df4c76b54a4e0')
 print('Prepared '+str(prefix)+' (x86_64 compiler; Rosetta required on Apple silicon)')
