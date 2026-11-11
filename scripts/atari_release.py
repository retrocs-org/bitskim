#!/usr/bin/env python3
"""Package experimental Atari executables, relink objects and matching source."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
import hashlib,shutil,sys
from build_provenance import ROOT,verify
from source_release import write_source
import importlib.util
spec=importlib.util.spec_from_file_location('atari_bootstrap',ROOT/'scripts/bootstrap-atari.py')
bootstrap=importlib.util.module_from_spec(spec);spec.loader.exec_module(bootstrap)
def write_atari(out, source, root=ROOT):
 ROOT=Path(root)
 variants=('sting','mintnet','mintnet060')
 for variant in variants:verify('atari-'+variant,ROOT)
 work=ROOT/'work/atari';archives=[]
 # Binary library/source snapshots and original build recipes are pinned together.
 for name in ('mintlib-0.60.1.tar.xz','gemlib-0.44.0.tar.xz','mintlib-0.60.1-20240718-build.sh','gemlib-0.44.0-20240425-build.sh'):
  bootstrap.fetch('https://tho-otto.m68k.eu/download/mint/'+name,work/name,bootstrap.FILES[name])
 for variant,label in [('sting','TOS-STinG-68000'),('mintnet','MiNTNet-68000'),('mintnet060','MiNTNet-68060')]:
  build=ROOT/'build/atari'/variant;archive=out/('BitSkim-0.1-Atari-'+label+'.zip')
  with ZipFile(archive,'w',ZIP_DEFLATED) as z:
   z.write(build/'BITSKIM.PRG','BITSKIM/BITSKIM.PRG')
   for path,name in [(ROOT/'docs/TRANSFER-TOOLS.md','TOOLS.TXT'),(ROOT/'docs/ATARI.md','README.TXT'),(ROOT/'LICENSE','MIT.TXT'),(ROOT/'THIRD-PARTY-NOTICES.MD','NOTICES.TXT'),(ROOT/'build/provenance'/('atari-'+variant+'.json'),'BUILD.JSN')]:z.write(path,'BITSKIM/'+name)
   for p in (ROOT/'assets/licenses/atari').iterdir():
    if p.is_file():z.write(p,'BITSKIM/RELINK/LICENCES/'+{'COPYMINT':'COPYMINT','COPYING.LIB':'COPYING.LIB','MINTLIB-NOTICES':'MINTLIB.TXT','GCC-RUNTIME':'GCCEXC.TXT','GCC-GPL3':'GCCGPL3.TXT'}[p.name])
   z.write(source,'BITSKIM/RELINK/SRC.ZIP')
   z.write(work/'mintlib-0.60.1.tar.xz','BITSKIM/RELINK/MLIB.TXZ');z.write(work/'gemlib-0.44.0.tar.xz','BITSKIM/RELINK/GEMLIB.TXZ')
   z.write(work/'mintlib-0.60.1-20240718-build.sh','BITSKIM/RELINK/MLBUILD.SH');z.write(work/'gemlib-0.44.0-20240425-build.sh','BITSKIM/RELINK/GMBUILD.SH')
   names=[]
   for original,packed in [('bitskim.o','CORE.O'),('platform.o','PLATFORM.O'),('main.o','MAIN.O')]+([('sting.o','STING.O')] if variant=='sting' else []):
    z.write(build/original,'BITSKIM/RELINK/'+packed);names.append(packed)
   flags='-m68020-60 -msoft-float' if variant=='mintnet060' else '-m68000'
   z.writestr('BITSKIM/RELINK/RELINK.SH','#!/bin/sh\nset -eu\ncd "$(dirname "$0")"\n${ATARI_CC:-m68k-atari-mint-gcc} '+flags+' ${LIBRARY_FLAGS:-} '+' '.join(names)+' -lgem -o BITSKIM.PRG\n')
   z.writestr('BITSKIM/RELINK/README.TXT','Own code is MIT. MiNTLib has component licences including LGPL. GEMlib identifies itself as Public Domain in gemlib.spec.in.\nLibrary sources and original build recipes are included. Unpack MLIB.TXZ and GEMLIB.TXZ with tar -xJf. Rebuild modified libraries with a compatible m68k-atari-mint SDK; see their README and included recipes.\nRun sh RELINK.SH here with ATARI_CC set to your compiler. LIBRARY_FLAGS="-L/path/to/replacement/libs -B/path/to/replacement/startup/" selects modified library/startup objects. Preserve the original CPU/ABI flags. SRC.ZIP contains full BitSkim sources/build instructions.\nSTinG itself, NetUSBee drivers and the external STinG SDK are not included; bootstrap-atari.py fetches the pinned SDK header for source builds. No public Atari hardware validation yet.\n')
  with ZipFile(archive) as z:
   if z.read('BITSKIM/BITSKIM.PRG')!= (build/'BITSKIM.PRG').read_bytes():raise SystemExit('Archive payload mismatch')
   if z.read('BITSKIM/BITSKIM.PRG')[:2]!=b'\x60\x1a':raise SystemExit('Not an Atari PRG')
  archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
  shutil.copy2(archive,ROOT/'public-web/downloads'/archive.name)
  archives.append(archive)
  print('Verified experimental '+str(archive))
 
 return archives
