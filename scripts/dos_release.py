"""Verified DOS executable and matching corresponding-source packaging."""
import json
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from build_provenance import ROOT,verify

def write_dos(output,source,root=ROOT):
    record=verify('dos',root)
    if (root/'build/dos/BITSKIM.EXE').read_bytes()[:2]!=b'MZ':raise RuntimeError('Not a DOS MZ executable')
    with ZipFile(source) as z:
        records=json.loads(z.read('BitSkim/BUILD-PROVENANCE.json'))
        if not records or any(r['source_sha256']!=record['source_sha256'] for r in records.values()):raise RuntimeError('Refresh the full source archive before packaging DOS')
        if z.read('BitSkim/third_party/mtcp/REVISION')!=(root/'third_party/mtcp/REVISION').read_bytes():raise RuntimeError('Source archive lacks matching mTCP source')
    files={'TOOLS.TXT':root/'docs/TRANSFER-TOOLS.md','BITSKIM.EXE':root/'build/dos/BITSKIM.EXE','README.TXT':root/'docs/DOS.md','MTCPSAMP.CFG':root/'clients/dos/MTCPSAMP.CFG','COPYING.TXT':root/'third_party/mtcp/COPYING','MIT.TXT':root/'LICENSE','THIRDPTY.TXT':root/'THIRD-PARTY-NOTICES.MD','WATCOM.TXT':root/'assets/licenses/WATCOM-LICENSE','SOURCE.ZIP':Path(source)}
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    with ZipFile(output,'w',ZIP_DEFLATED) as z:
        for name,path in files.items():z.write(path,'BITSKIM/'+name)
        z.writestr('BITSKIM/BUILD.TXT',json.dumps(record,indent=2)+'\n')
    with ZipFile(output) as z:
        for name,path in files.items():
            if z.read('BITSKIM/'+name)!=path.read_bytes():raise RuntimeError('Package verification failed: '+name)
    verify('dos',root)
    return output

