#!/usr/bin/env python3
"""Mirror compiled release packages to an optional local build directory."""
from pathlib import Path
import hashlib,os,shutil
ROOT=Path(__file__).resolve().parents[1]
config=ROOT/'work/builds-dir.txt'
destination=os.environ.get('BITSKIM_BUILDS_DIR') or (config.read_text().strip() if config.exists() else '')
if destination:
    target=Path(destination).expanduser();target.mkdir(parents=True,exist_ok=True)
    packages=[]
    for p in sorted((ROOT/'outputs').glob('BitSkim-*')):
        if not p.is_file():continue
        if p.name=='BitSkim-0.2.lha' or (p.suffix in ('.zip','.dmg','.exe') and any(x in p.name for x in ('AmigaOS','Atari-','DOS-mTCP','RISCOS3','Windows','macOS'))):
            packages.append(p)
    lines=[]
    for p in packages:
        temp=target/(p.name+'.tmp');shutil.copy2(p,temp)
        digest=hashlib.sha256(p.read_bytes()).hexdigest()
        if hashlib.sha256(temp.read_bytes()).hexdigest()!=digest:raise RuntimeError('Copy verification failed: '+p.name)
        temp.replace(target/p.name);lines.append(digest+'  '+p.name)
    (target/'SHA256SUMS').write_text('\n'.join(lines)+'\n')
    print('Verified '+str(len(packages))+' compiled packages in '+str(target))
