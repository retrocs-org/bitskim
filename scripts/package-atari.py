#!/usr/bin/env python3
"""Package experimental Atari clients and matching source."""
from pathlib import Path
import hashlib,shutil
from build_provenance import ROOT
from source_release import write_source
from atari_release import write_atari
out=ROOT/'outputs'
source,_=write_source(out,['amiga','atari-sting','atari-mintnet','atari-mintnet060'])
archives=write_atari(out,source)
paths=[source,out/'BitSkim-src.tar.gz',*archives]
sums=out/'SHA256SUMS';names={p.name for p in paths}
lines=[line for line in sums.read_text().splitlines() if line.split() and line.split()[-1].lstrip('*') not in names] if sums.exists() else []
for path in paths:
 lines.append(hashlib.sha256(path.read_bytes()).hexdigest()+'  '+path.name)
 shutil.copy2(path,ROOT/'public-web/downloads'/path.name)
sums.write_text('\n'.join(lines)+'\n')

import subprocess,sys
subprocess.run([sys.executable,str(Path(__file__).resolve().parent/"sync-builds.py")],check=True)
