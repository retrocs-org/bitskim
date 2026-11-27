#!/usr/bin/env python3
"""One Amiga payload manifest for ZIP and tested classic LH5/LHA releases."""
from pathlib import Path
from release_paths import output_dir
from zipfile import ZIP_DEFLATED
from archive_dates import PackageZipFile as ZipFile, current_timestamp
import argparse,hashlib,os,shutil,subprocess,tempfile
from build_provenance import verify
ROOT=Path(__file__).resolve().parents[1]

def payload(root=ROOT):
    files={'BitSkim.info':root/'assets/icons/BitSkimDrawer.info',
           'BitSkim/BUILD-PROVENANCE.json':root/'build/provenance/amiga.json'}
    for name in ('BitSkim','BitSkimPrefs','BitSkim.info','BitSkimPrefs.info','Install','Install.info','StartInstaller','StartBitSkim','StopBitSkim','BitSkim.guide','BitSkim.guide.info'):
        files['BitSkim/'+name]=root/'build/amiga'/name
    for source,target in [('docs/TRANSFER-TOOLS.md','TRANSFER-TOOLS.md'),('docs/TRANSFERS.md','TRANSFERS.md'),('README.MD','README.MD'),('LICENSE','LICENSE'),('THIRD-PARTY-NOTICES.MD','THIRD-PARTY-NOTICES.MD'),('docs/INSTALLATION.md','INSTALLATION.md'),('assets/themes/CATPPUCCIN-LICENSE','CATPPUCCIN-LICENSE'),('assets/licenses/NOVADOCK-LICENSE','NOVADOCK-LICENSE'),('docs/AMIGA-AREXX.md','ARexx/README.MD'),('docs/NOVADOCK.md','ARexx/NOVADOCK.md')]:
        files['BitSkim/'+target]=root/source
    for path in sorted((root/'examples/arexx').glob('*.rexx')):files['BitSkim/ARexx/'+path.name]=path
    for name in ('SettingsDrawer.info','BitSkim.prefs.info','Startup.info'):files['BitSkim/InstallAssets/'+name]=root/'build/amiga/InstallAssets'/name
    for name,path in files.items():
        if not path.is_file():raise RuntimeError('Missing Amiga package file: '+str(path))
        if len(name.encode('ascii'))>200:raise RuntimeError('Path too long for classic LHA: '+name)
    return dict(sorted(files.items()))

def writer():
    candidates=[os.environ['LHA']] if os.environ.get('LHA') else [str(Path(os.environ.get('LHA_PREFIX', str(Path.home()/'Development/tools/amiga/lha')))/'bin/lha'), str(Path.home()/'Development/tools/amiga/m68k-amigaos-gcc-16.2/bin/lha'), str(ROOT/'work/lha-tool/src/lha'), 'lha']
    for candidate in candidates:
        executable=shutil.which(candidate)
        if not executable:continue
        result=subprocess.run([executable,'--version'],capture_output=True,text=True)
        if result.returncode==0 and 'LHa for UNIX' in result.stdout:return executable
    raise RuntimeError('LHA creation tool unavailable: set LHA to a LHa-for-UNIX writer, or run scripts/bootstrap-lha.sh. Lhasa is extraction-only.')

def normalize_lha_paths(archive,expected_names):
    """Use Amiga-friendly slash paths in our newly created generic level-0 LHA.

    Inspect every header before writing anything. Only filename separator bytes
    and the enclosing header sum change; compressed data and file CRCs stay put.
    """
    archive=Path(archive);data=bytearray(archive.read_bytes());offset=0;seen=set();changed=0
    expected=set(expected_names)
    while True:
        if offset>=len(data):raise RuntimeError('LHA is missing its end marker')
        size=data[offset]
        if size==0:
            if offset!=len(data)-1:raise RuntimeError('Unexpected data after the LHA end marker')
            break
        end=offset+size+2
        if size<22 or end>len(data):raise RuntimeError('Truncated level-0 LHA header')
        if data[offset+20]!=0:raise RuntimeError('Expected generic level-0 LHA headers')
        if bytes(data[offset+2:offset+7]) not in (b'-lh0-',b'-lh5-'):
            raise RuntimeError('Unexpected LHA compression method')
        if sum(data[offset+2:end])&255!=data[offset+1]:raise RuntimeError('Invalid LHA header checksum')
        length=data[offset+21];start=offset+22;name_end=start+length
        if not length or name_end+2!=end:raise RuntimeError('Unexpected generic level-0 LHA filename layout')
        original=bytes(data[start:name_end]);normalized=original.replace(b'\xff',b'/')
        try:name=normalized.decode('ascii')
        except UnicodeDecodeError:raise RuntimeError('Unexpected non-ASCII LHA filename') from None
        if name not in expected or name in seen:raise RuntimeError('Unexpected or duplicate LHA member: '+name)
        seen.add(name)
        packed=int.from_bytes(data[offset+7:offset+11],'little')
        next_header=end+packed
        if next_header>len(data):raise RuntimeError('Truncated LHA file data')
        if normalized!=original:
            data[start:name_end]=normalized
            data[offset+1]=sum(data[offset+2:end])&255
            changed+=1
        offset=next_header
    if seen!=expected:raise RuntimeError('LHA member inventory differs from the package payload')
    if changed:archive.write_bytes(data)
    return changed


def write_zip(output,root=ROOT):
    verify('amiga',root);files=payload(root);output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as temp:
        archive=Path(temp)/output.name
        with ZipFile(archive,'w',ZIP_DEFLATED) as z:
            for name,path in files.items():z.write(path,name)
        verify('amiga',root)
        archive.replace(output)
    return output

def write_lha(output,root=ROOT):
    verify('amiga',root);files=payload(root);output=Path(output).resolve();output.parent.mkdir(parents=True,exist_ok=True)
    timestamp=current_timestamp()
    creator=writer();reader=shutil.which(os.environ.get('LHA_READER','lha')) or creator
    expected={name:hashlib.sha256(path.read_bytes()).digest() for name,path in files.items()}
    with tempfile.TemporaryDirectory(dir=output.parent) as temp:
        temp=Path(temp);stage=temp/'stage';stage.mkdir();archive=temp/output.name
        for name,path in files.items():
            target=stage/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
            os.utime(target,(timestamp,timestamp))
        # Generic level-0 headers and LH5 compression, without text conversion.
        subprocess.run([creator,'ao5g0q2',str(archive),*files],cwd=stage,check=True)
        normalize_lha_paths(archive,files)
        subprocess.run([reader,'t',str(archive)],check=True,stdout=subprocess.DEVNULL)
        extracted=temp/'extracted';extracted.mkdir()
        subprocess.run([reader,'x',str(archive)],cwd=extracted,check=True,stdout=subprocess.DEVNULL)
        actual={str(p.relative_to(extracted)):hashlib.sha256(p.read_bytes()).digest() for p in extracted.rglob('*') if p.is_file()}
        if actual!=expected:raise RuntimeError('LHA extraction differs from the verified package payload')
        if any(abs(p.stat().st_mtime-timestamp)>2 for p in extracted.rglob('*') if p.is_file()):
            raise RuntimeError('LHA extraction did not retain the current packaging date')
        if archive.read_bytes()[2:7] not in (b'-lh5-',b'-lh0-'):raise RuntimeError('Unexpected LHA compression format')
        verify('amiga',root)
        archive.replace(output)
    return output

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check-writer',action='store_true');args=parser.parse_args()
    if args.check_writer:writer();return
    if (ROOT/'VERSION').read_text().strip()!='0.2':raise RuntimeError('Version must remain 0.2')
    out=output_dir(ROOT);lha=write_lha(out/'BitSkim-0.2.lha');zipfile=write_zip(out/'BitSkim-0.2-AmigaOS.zip')
    digest=hashlib.sha256(lha.read_bytes()).hexdigest()
    (out/'BitSkim-0.2.lha.sha256').write_text(digest+'  '+lha.name+'\n')
    sums=out/'SHA256SUMS';lines=sums.read_text().splitlines() if sums.exists() else []
    lines=[line for line in lines if line.split() and line.split()[-1].lstrip('*') not in (lha.name,zipfile.name)]
    lines += [hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name for p in (lha,zipfile)]
    sums.write_text('\n'.join(lines)+'\n')
    (ROOT/'public-web/downloads').mkdir(parents=True,exist_ok=True)
    for p in (lha,zipfile,out/'BitSkim-0.2.lha.sha256'):
        shutil.copy2(p,ROOT/'public-web/downloads'/p.name)
    print('Built and extraction-verified '+str(lha)+' ('+str(lha.stat().st_size)+' bytes)')
    print('Matching ZIP: '+str(zipfile))

if __name__=='__main__':
    try:main()
    except (RuntimeError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))
