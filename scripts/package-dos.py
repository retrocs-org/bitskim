#!/usr/bin/env python3
"""Package DOS and refresh local downloads/checksums."""
import hashlib,shutil,subprocess,sys
from build_provenance import ROOT
from dos_release import write_dos

def main():
    output=write_dos(ROOT/'outputs/BitSkim-0.2-DOS-mTCP.zip',ROOT/'outputs/BitSkim-0.2-source.zip')
    line=hashlib.sha256(output.read_bytes()).hexdigest()+'  '+output.name+'\n'
    output.with_suffix('.zip.sha256').write_text(line)
    sums=ROOT/'outputs/SHA256SUMS'
    lines=[v for v in sums.read_text().splitlines() if v.split() and v.split()[-1]!=output.name] if sums.exists() else []
    sums.write_text('\n'.join(lines)+'\n'+line)
    downloads=ROOT/'public-web/downloads';downloads.mkdir(parents=True,exist_ok=True)
    shutil.copy2(output,downloads/output.name);shutil.copy2(sums,downloads/'SHA256SUMS')
    print('Verified executable + corresponding source: '+str(output))
    subprocess.run([sys.executable,str(ROOT/'scripts/sync-builds.py')],check=True)
if __name__=='__main__':main()
