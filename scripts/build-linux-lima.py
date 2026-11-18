#!/usr/bin/env python3
"""Copy a credential-free source snapshot into an existing Linux build instance."""
import argparse,hashlib,io,json,os,shutil,subprocess,sys,tarfile,tempfile,uuid
from pathlib import Path
from build_provenance import ROOT,SOURCE_ROOTS,source_hash,verify
from source_release import private_release_file

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--instance',default=os.environ.get('BITSKIM_LINUX_INSTANCE'));p.add_argument('--arch',choices=('arm64','x86_64'));p.add_argument('--cli-only',action='store_true');p.add_argument('--test',action='store_true');args=p.parse_args()
 if not args.instance or args.instance.startswith('-') or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in args.instance):p.error('Set --instance NAME or BITSKIM_LINUX_INSTANCE to an existing Linux build instance')
 env=os.environ.copy();env['LIMA_HOME']=env.get('BITSKIM_LIMA_HOME',str(Path.home()/'Development/tools/linux/lima'));prefix=['limactl','shell',args.instance];remote='/tmp/bitskim-build-'+uuid.uuid4().hex
 def run(arguments,**kwargs):return subprocess.run(prefix+arguments,env=env,check=True,**kwargs)
 original=source_hash(ROOT)
 with tempfile.TemporaryDirectory(prefix='bitskim-linux-') as temporary:
  temporary=Path(temporary);snapshot=temporary/'source.tar'
  with tarfile.open(snapshot,'w') as archive:
   for name in SOURCE_ROOTS:
    root=ROOT/name
    for path in sorted(root.rglob('*')) if root.is_dir() else [root]:
     rel=path.relative_to(ROOT)
     if any(part in ('__pycache__','.DS_Store','downloads') for part in rel.parts) or path.name.startswith('._'):continue
     if path.is_symlink():raise RuntimeError('Source snapshot refuses symlinks: '+str(rel))
     if path.is_file():
      if private_release_file(path):raise RuntimeError('Credential/local config cannot enter Linux build snapshot: '+str(rel))
      archive.add(path,arcname=str(rel),recursive=False)
  run(['mkdir','-p',remote])
  try:
   with snapshot.open('rb') as data:run(['tar','xf','-','-C',remote],stdin=data)
   build=(['env','BITSKIM_LINUX_PKG_CONFIG_PATH='+env['BITSKIM_LINUX_PKG_CONFIG_PATH']] if env.get('BITSKIM_LINUX_PKG_CONFIG_PATH') else [])+['python3','scripts/build-linux.py']+(['--arch',args.arch] if args.arch else [])+(['--cli-only'] if args.cli_only else [])
   run(['sh','-c','cd "$1" && shift && exec "$@"','sh',remote,*build])
   if args.test:
    testing=['env']+(['BITSKIM_LINUX_TEST_ARCH='+args.arch] if args.arch else [])+(['BITSKIM_LINUX_TEST_RUNNER=qemu-x86_64'] if args.arch=='x86_64' else [])+['python3','-m','unittest','discover','-s','tests','-p','test_linux_clients.py','-b']
    run(['sh','-c','cd "$1" && shift && exec "$@"','sh',remote,*testing])
   artifact=temporary/'artifacts.tar'
   with artifact.open('wb') as output:run(['tar','cf','-','-C',remote,'build/linux','build/provenance'],stdout=output)
   if source_hash(ROOT)!=original:raise RuntimeError('Source changed during Linux build; rebuild')
   copied_records=[]
   with tarfile.open(artifact) as archive:
    for member in archive.getmembers():
     path=Path(member.name)
     if path.is_absolute() or '..' in path.parts or member.issym() or member.islnk() or not (member.isdir() or member.isfile()) or path.parts[:2] not in (('build','linux'),('build','provenance')):raise RuntimeError('Unsafe Linux artifact path')
     destination=ROOT/path
     if member.isdir():destination.mkdir(parents=True,exist_ok=True)
     else:
      destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(archive.extractfile(member).read());destination.chmod(member.mode&0o777)
      if path.parts[:2]==('build','provenance'):copied_records.append(destination)
   for record in copied_records:
    data=json.loads(record.read_text());verify(data['target'],ROOT)
   print('Verified Linux artifacts copied to '+str(ROOT/'build/linux'))
  finally:run(['rm','-rf','--',remote])
if __name__=='__main__':
 try:main()
 except (RuntimeError,OSError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
