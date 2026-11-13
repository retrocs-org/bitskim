#!/usr/bin/env python3
"""Fingerprint source before/after builds; reject stale or modified release payloads."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SOURCE_ROOTS=('VERSION','CONTRIBUTING.md','.gitignore','Makefile','README.MD','CHANGELOG.MD','LICENSE','LICENSE.md','THIRD-PARTY-NOTICES.MD','core','clients','assets','scripts','tests','docs','examples','installers','public-web','third_party')
ARTIFACTS={
 'dos':['build/dos/BITSKIM.EXE'],
 'macos':['build/macos/BitSkim.app'],
 'amiga':['build/amiga/'+x for x in ('BitSkim','BitSkimPrefs','BitSkim.info','BitSkimPrefs.info','BitSkimDrawer.info','StartInstaller','StartBitSkim','StopBitSkim','BitSkim.guide','BitSkim.guide.info','Install-BitSkim','Install-BitSkim.info','InstallAssets/SettingsDrawer.info','InstallAssets/BitSkim.prefs.info','InstallAssets/Startup.info')],
 'windows-legacy':['build/windows-legacy/BitSkim.exe'],
 'windows':['build/windows/BitSkim.exe','build/windows/BitSkim-CoreCheck.exe'],
 'dmg':['outputs/BitSkim-0.2-macOS-arm64.dmg'],
 'setup-legacy':['outputs/BitSkim-0.2-Windows-Classic-x86-Setup.exe'],
 'setup':['outputs/BitSkim-0.2-Windows10-x64-Setup.exe'],
}
for cpu in ('arm2','strongarm'):
 ARTIFACTS['riscos-'+cpu]=['build/riscos/'+cpu+'/'+p for p in ('!BitSkim','main.o','bitskim.o','platform.o','riscos-file.o')]
for variant in ('sting','mintnet','mintnet060'):
 ARTIFACTS['atari-'+variant]=['build/atari/'+variant+'/'+p for p in ('BITSKIM.PRG','bitskim.o','platform.o','main.o')+ (('sting.o',) if variant=='sting' else ())]
DEPENDENCIES={'setup-legacy':['windows-legacy'],'dmg':['macos'],'setup':['windows']}
class StaleBuild(RuntimeError):pass

def file_hash(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()

def tree_hash(path):
 if path.is_symlink():return hashlib.sha256(('link:'+os.readlink(path)).encode()).hexdigest()
 if path.is_file():return file_hash(path)
 if not path.is_dir():raise StaleBuild('Missing artifact: '+str(path))
 h=hashlib.sha256()
 for p in sorted(path.rglob('*')):
  if p.is_file() or p.is_symlink():
   h.update(str(p.relative_to(path)).encode()+b'\0'+tree_hash(p).encode()+b'\0'+str(p.lstat().st_mode&0o777).encode()+b'\n')
 return h.hexdigest()

def source_hash(root):
 h=hashlib.sha256()
 for name in SOURCE_ROOTS:
  base=root/name
  paths=sorted(base.rglob('*')) if base.is_dir() else [base]
  for p in paths:
   rel=p.relative_to(root)
   if '__pycache__' in rel.parts or p.name=='.DS_Store' or p.name.startswith('._') or rel.parts[:2]==('public-web','downloads'):continue
   if p.is_file() or p.is_symlink():h.update(str(rel).encode()+b'\0'+tree_hash(p).encode()+b'\n')
 return h.hexdigest()

def record_path(root,target,pending=False):return root/'build/provenance'/(target+('.pending.json' if pending else '.json'))
def write_json(path,data):
 path.parent.mkdir(parents=True,exist_ok=True);tmp=path.with_suffix('.tmp');tmp.write_text(json.dumps(data,indent=2)+'\n');tmp.replace(path)

def verify(target,root=ROOT):
 if target not in ARTIFACTS:raise StaleBuild('Unknown target: '+target)
 path=record_path(root,target)
 if not path.is_file():raise StaleBuild(target+': no build fingerprint; rebuild this target')
 data=json.loads(path.read_text())
 if data.get('version')!='0.2' or data.get('target')!=target or data.get('source_sha256')!=source_hash(root):raise StaleBuild(target+': source changed since build; rebuild this target')
 if set(data.get('artifacts',{}))!=set(ARTIFACTS[target]):raise StaleBuild(target+': incomplete artifact inventory; rebuild')
 for rel,digest in data['artifacts'].items():
  if tree_hash(root/rel)!=digest:raise StaleBuild(target+': artifact changed: '+rel+'; rebuild')
 for dep in DEPENDENCIES.get(target,[]):
  dependency=verify(dep,root)
  if data.get('dependencies',{}).get(dep)!=dependency['artifacts']:raise StaleBuild(target+': embedded '+dep+' payload changed; rebuild')
 return data

def begin(target,root=ROOT):
 if (root/'VERSION').read_text().strip()!='0.2':raise StaleBuild('Version must remain 0.2')
 dependencies={dep:verify(dep,root)['artifacts'] for dep in DEPENDENCIES.get(target,[])}
 path=record_path(root,target)
 if path.exists():path.unlink()
 write_json(record_path(root,target,True),dict(target=target,version='0.2',source_sha256=source_hash(root),dependencies=dependencies))

def finish(target,root=ROOT,tool=None,version_arg='--version'):
 path=record_path(root,target,True)
 if not path.is_file():raise StaleBuild(target+': build start was not recorded')
 data=json.loads(path.read_text())
 if data['source_sha256']!=source_hash(root):raise StaleBuild(target+': source changed during build; rebuild')
 for dep in DEPENDENCIES.get(target,[]):
  if verify(dep,root)['artifacts']!=data['dependencies'][dep]:raise StaleBuild(target+': dependency changed during build; rebuild')
 data['artifacts']={rel:tree_hash(root/rel) for rel in ARTIFACTS[target]}
 if tool:
  resolved=shutil.which(tool) or tool
  result=subprocess.run([resolved,version_arg],capture_output=True,text=True,check=True)
  data['toolchain']={'executable':Path(resolved).name,'version':(result.stdout+result.stderr).strip(),'sha256':file_hash(Path(resolved))}
 write_json(record_path(root,target),data);path.unlink()
 return data

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=('begin','finish','verify'));p.add_argument('targets',nargs='+',choices=sorted(ARTIFACTS));p.add_argument('--root',type=Path,default=ROOT);p.add_argument('--tool');p.add_argument('--version-arg',default='--version');a=p.parse_args()
 try:
  for target in a.targets:
   if a.action=='begin':begin(target,a.root)
   elif a.action=='finish':finish(target,a.root,a.tool,a.version_arg)
   else:verify(target,a.root)
 except (StaleBuild,ValueError,OSError,subprocess.SubprocessError) as e:p.exit(1,'Build verification failed: '+str(e)+'\n')
if __name__=='__main__':main()
