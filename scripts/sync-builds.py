#!/usr/bin/env python3
"""Refresh the local BitSkim installer drawer and retain packages beneath it."""
from pathlib import Path,PurePosixPath
from release_paths import output_dir
import argparse,datetime,hashlib,os,shutil
from zipfile import ZipFile
from build_provenance import verify,StaleBuild
from amiga_release import payload
ROOT=Path(__file__).resolve().parents[1]

def sync(destination,root=ROOT):
    base=Path(destination).expanduser().resolve();target=base/'BitSkim';out=output_dir(root)
    def safe(path):
        if any(part.is_symlink() for part in (path,*path.parents)):raise ValueError('Build destination cannot traverse symbolic links')
    safe(target);target.mkdir(parents=True,exist_ok=True)
    backup=root/'work/builds-sync-backups'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    def copy(data,path,mode=0o644):
        safe(path)
        if path.exists():
            if not path.is_file():raise ValueError('Destination conflicts with a directory: '+str(path))
            if path.read_bytes()==data:return
            saved=backup/path.relative_to(base);saved.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,saved)
        path.parent.mkdir(parents=True,exist_ok=True);temporary=path.with_name(path.name+'.bitskim-sync-tmp');safe(temporary)
        temporary.write_bytes(data);temporary.chmod(mode);temporary.replace(path)
        if path.read_bytes()!=data:raise ValueError('Build copy verification failed: '+str(path))
    archive=out/'BitSkim-0.2-AmigaOS.zip'
    if archive.is_file():
        try:verify('amiga',root)
        except StaleBuild:print('Amiga installer drawer not refreshed: rebuild the current Amiga target first.')
        else:
            expected=payload(root);updates=[]
            with ZipFile(archive) as package:
                if set(package.namelist())!=set(expected):raise ValueError('Amiga installation archive inventory differs')
                for name,source in expected.items():
                    relative=PurePosixPath(name)
                    if relative.is_absolute() or '..' in relative.parts or relative.parts[0] not in ('BitSkim','BitSkim.info'):raise ValueError('Unsafe installer path')
                    data=package.read(name)
                    if data!=source.read_bytes():raise ValueError('Amiga installation archive is stale: '+name)
                    updates.append((data,base/name,((package.getinfo(name).external_attr>>16)&0o777) or 0o644))
            for data,path,mode in updates:copy(data,path,mode)
            print('Verified unpacked installer: '+str(target)+' (open Install-BitSkim)')
    packages=[]
    for file in sorted(out.glob('BitSkim-*')):
        if file.is_file() and (file.name=='BitSkim-0.2.lha' or (file.suffix in ('.zip','.dmg','.exe') and any(part in file.name for part in ('AmigaOS','Atari-','DOS-mTCP','RISCOS3','Windows','macOS')))):packages.append(file)
    lines=[]
    for file in packages:
        data=file.read_bytes();copy(data,target/'packages'/file.name);digest=hashlib.sha256(data).hexdigest();lines.append(digest+'  '+file.name)
        copy((digest+'  '+file.name+'\n').encode(),target/'packages'/(file.name+'.sha256'))
    copy(('\n'.join(lines)+'\n').encode(),target/'packages/SHA256SUMS')
    print('Verified '+str(len(packages))+' packages in '+str(target/'packages'))
    return target

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--builds-dir',type=Path);args=parser.parse_args();config=ROOT/'work/builds-dir.txt'
    destination=args.builds_dir or os.environ.get('BITSKIM_BUILDS_DIR') or (config.read_text().strip() if config.exists() else '') or Path.home()/'Development/builds'
    sync(destination)
if __name__=='__main__':main()
