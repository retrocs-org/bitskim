#!/usr/bin/env python3
"""Create the current full source ZIP and BitSkim-src.tar.gz, without build tools."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import datetime,hashlib,io,json,shutil,tarfile,tempfile
from build_provenance import verify
ROOT=Path(__file__).resolve().parents[1]

def private_release_file(path):
    name=Path(path).name.lower()
    return (name=='.env' or name.startswith('.env.') or name in ('release-config.json','id_rsa','id_ed25519')
            or Path(name).suffix in ('.p12','.pfx','.key','.pem'))

def write_source(out,targets=('amiga',),root=ROOT):
    records={target:verify(target,root) for target in targets}
    files={}
    for name in ('README.MD','CHANGELOG.MD','LICENSE','LICENSE.md','THIRD-PARTY-NOTICES.MD','VERSION','CONTRIBUTING.md','.gitignore','Makefile'):
        files['BitSkim/'+name]=root/name
    for folder in ('core','clients','assets','scripts','tests','docs','examples','installers','public-web','third_party'):
        for path in sorted((root/folder).rglob('*')):
            if path.is_file() and private_release_file(path):
                raise RuntimeError('Private/local credential file must not enter a source archive: '+str(path.relative_to(root)))
            if path.relative_to(root).parts[:3]==('assets','icons','legacy-generated'):continue
            if path.is_file() and not any(p in ('downloads','__pycache__','.DS_Store') for p in path.relative_to(root/folder).parts):
                files['BitSkim/'+str(path.relative_to(root))]=path
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    zipfile=out/'BitSkim-0.2-source.zip';tarfile_path=out/'BitSkim-src.tar.gz'
    with tempfile.TemporaryDirectory(dir=out) as temp:
        temp=Path(temp);zpath=temp/zipfile.name;tpath=temp/tarfile_path.name
        with ZipFile(zpath,'w',ZIP_DEFLATED) as z:
            z.writestr('BitSkim/BUILD-PROVENANCE.json',json.dumps(records,indent=2)+'\n')
            for name,path in sorted(files.items()):z.write(path,name)
        with ZipFile(zpath) as z,tarfile.open(tpath,'w:gz') as t:
            for item in z.infolist():
                data=z.read(item);info=tarfile.TarInfo(item.filename);info.size=len(data)
                info.mode=(item.external_attr>>16)&0o777 or 0o644
                info.mtime=int(datetime.datetime(*item.date_time).timestamp())
                t.addfile(info,io.BytesIO(data))
        with ZipFile(zpath) as z,tarfile.open(tpath,'r:gz') as t:
            if set(z.namelist())!=set(t.getnames()):raise RuntimeError('Source archive inventory mismatch')
            for name in z.namelist():
                if z.read(name)!=t.extractfile(name).read():raise RuntimeError('Source archive content mismatch: '+name)
        for target in targets:verify(target,root)
        zpath.replace(zipfile);tpath.replace(tarfile_path)
    return zipfile,tarfile_path

def main():
    paths=write_source(ROOT/'outputs')
    sums=ROOT/'outputs/SHA256SUMS';lines=sums.read_text().splitlines() if sums.exists() else []
    lines=[line for line in lines if line.split() and line.split()[-1].lstrip('*') not in {p.name for p in paths}]
    (ROOT/'public-web/downloads').mkdir(parents=True,exist_ok=True)
    for path in paths:
        lines.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name)
        shutil.copy2(path,ROOT/'public-web/downloads'/path.name)
        print('Verified source archive: '+str(path))
    sums.write_text('\n'.join(lines)+'\n')
if __name__=='__main__':main()
