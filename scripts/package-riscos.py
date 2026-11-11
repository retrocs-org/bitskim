#!/usr/bin/env python3
"""Package both current RISC OS builds without rebuilding unrelated clients."""
from pathlib import Path
import hashlib,shutil
from build_provenance import ROOT,verify
from source_release import write_source
from riscos_release import write_riscos
out=ROOT/'outputs'
targets=['amiga','riscos-arm2','riscos-strongarm']
for target in targets:verify(target)
source,tar=write_source(out,targets)
packages=write_riscos(out,source,ROOT)
paths=[source,tar]+packages
sums=out/'SHA256SUMS';names={p.name for p in paths}
lines=[line for line in sums.read_text().splitlines() if line.split() and line.split()[-1].lstrip('*') not in names] if sums.exists() else []
for path in paths:
 digest=hashlib.sha256(path.read_bytes()).hexdigest()
 lines.append(digest+'  '+path.name)
 if path in packages:path.with_suffix('.zip.sha256').write_text(digest+'  '+path.name+'\n')
 shutil.copy2(path,ROOT/'public-web/downloads'/path.name)
 print('Verified '+str(path))
sums.write_text('\n'.join(lines)+'\n')

# Refresh the configured local compiled-package mirror after successful packaging.
import subprocess,sys
subprocess.run([sys.executable,str(Path(__file__).resolve().parent/'sync-builds.py')],check=True)
