"""Package the verified experimental Windows Classic client."""
import json,hashlib
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from build_provenance import ROOT,verify
def write_windows_legacy(out, root=ROOT):
 ROOT=Path(root)
 record=verify('windows-legacy',ROOT)
 with ZipFile(out,'w',ZIP_DEFLATED) as z:
     for source,target in [('docs/TRANSFER-TOOLS.md','TRANSFER-TOOLS.md'),('build/windows-legacy/BitSkim.exe','BitSkim.exe'),('docs/WINDOWS-LEGACY.md','README.MD'),('LICENSE','LICENSE'),('assets/themes/CATPPUCCIN-LICENSE','CATPPUCCIN-LICENSE'),('THIRD-PARTY-NOTICES.MD','THIRD-PARTY-NOTICES.MD')]:
         z.write(ROOT/source,'BitSkim/'+target)
     z.writestr('BitSkim/BUILD-PROVENANCE.json',json.dumps(record,indent=2)+'\n')
 verify('windows-legacy',ROOT)
 out.with_suffix('.zip.sha256').write_text(hashlib.sha256(out.read_bytes()).hexdigest()+'  '+out.name+'\n')
 print(out)
 
 return out
