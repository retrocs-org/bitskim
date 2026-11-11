#!/usr/bin/env python3
"""Refresh Bin:BitSkim plus the compiled LHA and source tarball; no guest install."""
from pathlib import Path,PurePosixPath
from zipfile import ZipFile
import argparse,datetime,hashlib,json,os,shutil,struct,subprocess,tarfile
from amiga_release import payload,writer
from build_provenance import verify
ROOT=Path(__file__).resolve().parents[1]

def sync(destination,root=ROOT):
    verify('amiga',root);destination=Path(destination).expanduser().resolve()
    if not destination.is_dir():raise ValueError('Bin destination must already exist: '+str(destination))
    out=root/'outputs';names=('BitSkim-0.1.lha','BitSkim-src.tar.gz')
    sums={line.split()[-1].lstrip('*'):line.split()[0] for line in (out/'SHA256SUMS').read_text().splitlines() if line.split()}
    for name in names:
        if hashlib.sha256((out/name).read_bytes()).hexdigest()!=sums.get(name):raise ValueError('Missing/mismatched release checksum: '+name)
    expected=payload(root)
    updates={};modes={}
    with ZipFile(out/'BitSkim-0.1-AmigaOS.zip') as z:
        if set(z.namelist())!=set(expected):raise ValueError('Stale Amiga ZIP inventory')
        for name,path in expected.items():
            data=z.read(name)
            if data!=path.read_bytes():raise ValueError('Stale Amiga ZIP file: '+name)
            updates[name]=data
            modes[name]=(z.getinfo(name).external_attr>>16)&0o777 or 0o644
    reader=shutil.which(os.environ.get('LHA_READER','lha')) or writer()
    record=subprocess.run([reader,'pq2',str(out/'BitSkim-0.1.lha'),'BitSkim/BUILD-PROVENANCE.json'],capture_output=True,check=True).stdout
    if record!=(root/'build/provenance/amiga.json').read_bytes():raise ValueError('LHA belongs to a different build')
    # Source archive must also describe the current source build.
    with tarfile.open(out/'BitSkim-src.tar.gz','r:gz') as t:
        records=json.load(t.extractfile('BitSkim/BUILD-PROVENANCE.json'))
        if records['amiga']['source_sha256']!=verify('amiga',root)['source_sha256']:raise ValueError('Source tarball is stale')
    for name in names:updates[name]=(out/name).read_bytes()
    backup=root/'work/bin-sync-backups'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    for name,data in updates.items():
        rel=PurePosixPath(name)
        if rel.is_absolute() or '..' in rel.parts:raise ValueError('Unsafe package path')
        target=destination/name
        # Keep existing desktop positions, while updating artwork and other flags.
        if name in ('BitSkim.info','BitSkim/BitSkim.info','BitSkim/BitSkimPrefs.info') and target.is_file():
            old=target.read_bytes()
            if len(old)>=78 and old[:4]==b'\xe3\x10\x00\x01':data=data[:58]+old[58:66]+data[66:]
        if target.exists():
            if not target.is_file():raise ValueError('Destination file conflicts with a drawer: '+str(target))
            old=backup/name;old.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(target,old)
        target.parent.mkdir(parents=True,exist_ok=True)
        temp=target.with_name(target.name+'.bitskim-sync-tmp');temp.write_bytes(data);temp.chmod(modes.get(name,0o644));temp.replace(target)
        if target.read_bytes()!=data:raise ValueError('Copy verification failed: '+str(target))
    print('Updated '+str(destination/'BitSkim'))
    for name in names:print('Updated '+str(destination/name))
    print('All copied bytes verified; replaced files backed up under '+str(backup))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--bin-dir');a=p.parse_args()
    setting=ROOT/'work/amiga-dev-bin.txt'
    destination=a.bin_dir or os.environ.get('AMIGA_DEV_BIN') or (setting.read_text().strip() if setting.exists() else None)
    if not destination:raise ValueError('Set AMIGA_DEV_BIN or pass --bin-dir')
    sync(destination)
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
