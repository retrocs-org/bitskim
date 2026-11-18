#!/usr/bin/env python3
"""Prepare a reviewed Aminet submission; uploading is always explicit."""
from pathlib import Path
from release_paths import output_dir
import argparse,email.utils,ftplib,hashlib,json,re,textwrap,os
from build_provenance import verify
ROOT=Path(__file__).resolve().parents[1]
HOST='main.aminet.net'
CATEGORY='comm/tcp'
ARCHIVE='bitskim.lha'
README='bitskim.readme'

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def contact(value):
    if not value or any(ord(c)<32 or ord(c)>126 for c in value):raise ValueError('Supply a plain ASCII uploader email/name without control characters')
    _,address=email.utils.parseaddr(value)
    if not re.fullmatch(r'[^\s@<>]+@[^\s@<>]+\.[^\s@<>]+',address):raise ValueError('A real, readable uploader email is required for Aminet')
    return address

def readme(uploader,author='BitSkim contributors',root=ROOT,version='0.2',tag=None):
    from release_notes import version_changes,plain
    contact(uploader)
    if not author or len(author)>64 or any(ord(c)<32 or ord(c)>126 for c in author):raise ValueError('Author must be a short ASCII name')
    headers=[('Short','Local network file sharing'),('Author',author),('Uploader',uploader),('Type',CATEGORY),('Version',version),('Architecture','m68k-amigaos >= 3.0.0'),('Requires','AmigaOS 3.x, 68000+, bsdsocket.library v4'),('Distribution','Aminet'),('URL','https://bitskim.retrocs.org/')]
    lines=[f'{key+":":14}{value}' for key,value in headers]
    def paragraph(text):lines.extend(['']+textwrap.wrap(plain(text),76,break_long_words=False,break_on_hyphens=False))
    paragraph('BitSkim shares files between classic AmigaOS and modern computers on an IPv4 local network. The Amiga package includes the app, standalone Preferences, AmigaGuide, Workbench icons and Installer script.')
    paragraph('FEATURES')
    paragraph('Automatic discovery, manual IPv4, receive approval, save paths, themed Workbench windows, iconified reception, ARexx and progress/speed. Amiga transfers are plaintext and names are unverified; use a trusted network. Features and validation differ between clients.')
    paragraph('INSTALLATION')
    paragraph('Unpack bitskim.lha and open Install-BitSkim. Main app, Preferences and shared resources have selectable destinations; help and ENV:/ENVARC: settings use standard locations. MultiView opens the guide. The system Installer V42 or later is required and is not redistributed. Keep the unpacked package for updates and uninstall.')
    paragraph('SOURCE AND LICENCE')
    paragraph('BitSkim-owned code is MIT licensed; bundled dependencies retain their own licences. See LICENSE and THIRD-PARTY-NOTICES.MD in the archive.')
    paragraph('Source: https://github.com/retrocs-org/bitskim')
    paragraph('Release: https://github.com/retrocs-org/bitskim/releases/tag/'+(tag or version))
    paragraph('VERSION CHANGELOG')
    paragraph(version)
    for group,items in version_changes(root,version).items():
        if not items:continue
        paragraph(group.upper())
        for item in items:
            lines.append('')
            lines.extend(textwrap.wrap(plain(item),76,initial_indent='  - ',subsequent_indent='    ',break_long_words=False,break_on_hyphens=False))
    if any(len(line)>76 for line in lines):raise ValueError('Aminet readme exceeds 76 columns; shorten metadata or changelog URLs')
    return '\n'.join(lines)+'\n'

def prepare(uploader,author='BitSkim contributors',root=ROOT):
    record=verify('amiga',root)
    archive=output_dir(root) / 'BitSkim-0.2.lha'
    checksum=output_dir(root) / 'BitSkim-0.2.lha.sha256'
    checksum_words=checksum.read_text().split() if checksum.is_file() else []
    if not archive.is_file() or not checksum_words or checksum_words[0]!=digest(archive):
        raise ValueError('Build the verified LHA first with scripts/release-amiga.sh')
    # Compare the release payload against the current build, not just its checksum.
    from amiga_release import payload,writer
    import shutil,subprocess,tempfile
    with tempfile.TemporaryDirectory() as temp:
        reader=shutil.which(os.environ.get('LHA_READER','lha')) or writer()
        subprocess.run([reader,'x',str(archive.resolve())],cwd=temp,check=True,stdout=subprocess.DEVNULL)
        extracted=Path(temp)
        actual={str(p.relative_to(extracted)):digest(p) for p in extracted.rglob('*') if p.is_file()}
        if actual!={name:digest(p) for name,p in payload(root).items()}:raise ValueError('LHA does not match this build; rebuild it before preparing Aminet files')
    text=readme(uploader,author,root)
    folder=output_dir(root) / 'aminet';folder.mkdir(parents=True,exist_ok=True)
    if (folder/'upload-receipt.json').exists():raise ValueError('An upload receipt already exists. Review it before preparing another submission; archive that receipt manually first.')
    (folder/ARCHIVE).write_bytes(archive.read_bytes())
    with (folder/README).open('w',encoding='ascii',newline='\n') as stream:stream.write(text)
    manifest={'version':'0.2','host':HOST,'directory':'/new','uploader':uploader,'category':CATEGORY,'source_sha256':record['source_sha256'],
              'files':{name:digest(folder/name) for name in (ARCHIVE,README)}}
    (folder/'submission.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared '+str(folder)+'; review bitskim.readme before an explicit upload.')
    return folder

def check(folder,root=ROOT,published=False):
    manifest=json.loads((folder/'submission.json').read_text())
    if (manifest['version']!='0.2' and not published) or manifest['host']!=HOST or manifest['directory']!='/new' or manifest['category']!=CATEGORY:raise ValueError('Unexpected Aminet destination or release metadata')
    if set(manifest['files'])!={ARCHIVE,README}:raise ValueError('Unexpected upload file list')
    if not published:
        record=verify('amiga',root)
        if record['source_sha256']!=manifest['source_sha256']:raise ValueError('Submission belongs to a different source build')
    for name,expected in manifest['files'].items():
        if digest(folder/name)!=expected:raise ValueError('Submission changed after preparation: '+name+'; prepare it again')
    text=(folder/README).read_text(encoding='ascii')
    if '\r' in (folder/README).read_bytes().decode('ascii') or any(len(line)>76 for line in text.splitlines()):raise ValueError('Readme must use LF lines of at most 76 columns')
    if not text.startswith('Short:') or f'Uploader:     {manifest["uploader"]}' not in text:raise ValueError('Readme uploader metadata does not match submission')
    contact(manifest['uploader'])
    return manifest

def incoming_clear(ftp):
    try:names={Path(name.rstrip('/')).name for name in ftp.nlst()}
    except Exception:raise RuntimeError('Aminet incoming listing is unreadable; no upload attempted') from None
    if names.intersection((ARCHIVE,README)):raise ValueError('Aminet incoming already contains bitskim.lha/readme; inspect the pending submission before uploading')

def preflight(folder,root=ROOT,ftp_factory=ftplib.FTP,published=False):
    manifest=check(folder,root,published)
    if (folder/'upload-receipt.json').exists():raise ValueError('Review the existing Aminet upload receipt instead of automatically retrying')
    with ftp_factory() as ftp:
        ftp.connect(HOST,21,timeout=30);ftp.login('anonymous',contact(manifest['uploader']));ftp.cwd('/new');incoming_clear(ftp)

def upload(folder,confirmed=False,root=ROOT,ftp_factory=ftplib.FTP,published=False):
    if not confirmed:raise ValueError('No upload performed. Add --confirm-upload only when ready to submit publicly to Aminet.')
    manifest=check(folder,root,published);receipt=folder/'upload-receipt.json'
    if receipt.exists():raise ValueError('This submission already has an upload attempt receipt; review it instead of automatically retrying')
    state={'host':HOST,'directory':'/new','completed':[],'status':'starting','files':manifest['files']}
    def save():receipt.write_text(json.dumps(state,indent=2)+'\n')
    attempted=False
    try:
        with ftp_factory() as ftp:
            ftp.connect(HOST,21,timeout=30);ftp.login('anonymous',contact(manifest['uploader']));ftp.cwd('/new');incoming_clear(ftp)
            save();attempted=True
            for name in (ARCHIVE,README):
                with (folder/name).open('rb') as stream:ftp.storbinary('STOR '+name,stream)
                state['completed'].append(name);save()
        state['status']='transferred-awaiting-aminet-review';save()
        print('Transferred archive and readme to Aminet incoming; this does not confirm publication.')
    except Exception:
        if not attempted:raise
        state['status']='failed-may-be-partial';save()
        raise RuntimeError('Aminet upload failed and may be partial. Review upload-receipt.json and the incoming server before retrying.') from None

def template(root=ROOT):
    path=output_dir(root) / 'aminet/bitskim.readme.template';path.parent.mkdir(parents=True,exist_ok=True)
    # Deliberately not named .readme: no invented uploader identity is submitted.
    example=readme('SET-YOUR-EMAIL@example.invalid').replace('SET-YOUR-EMAIL@example.invalid','<YOUR REAL EMAIL REQUIRED>')
    with path.open('w',encoding='ascii',newline='\n') as stream:stream.write(example)
    return path

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('action',choices=['template','prepare','check','upload']);p.add_argument('--uploader');p.add_argument('--author',default='BitSkim contributors');p.add_argument('--confirm-upload',action='store_true');a=p.parse_args()
    folder=output_dir(ROOT) / 'aminet'
    if a.action=='template':print(template())
    elif a.action=='prepare':prepare(a.uploader,a.author)
    elif a.action=='check':check(folder);print('Aminet submission checks passed; no network connection made.')
    else:upload(folder,a.confirm_upload)
if __name__=='__main__':
    try:main()
    except (ValueError,RuntimeError,KeyError,OSError) as error:raise SystemExit(str(error))
