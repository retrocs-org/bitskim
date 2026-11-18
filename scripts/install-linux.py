#!/usr/bin/env python3
"""Install a locally built Linux client into a prefix or packaging staging root."""
import argparse,platform,shutil,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--prefix',type=Path,default=Path('/usr/local'));p.add_argument('--destdir',type=Path);p.add_argument('--arch',choices=('arm64','x86_64'),default='arm64' if platform.machine() in ('aarch64','arm64') else 'x86_64');p.add_argument('--cli-only',action='store_true');a=p.parse_args()
 if not a.prefix.is_absolute():p.error('--prefix must be absolute')
 base=(a.destdir/a.prefix.relative_to('/')) if a.destdir else a.prefix
 def copy(source,destination,mode=0o644):
  source=ROOT/source;destination=base/destination;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,destination);destination.chmod(mode)
 copy(Path('build/linux')/a.arch/'bitskim-cli',Path('bin/bitskim-cli'),0o755)
 if not a.cli_only:
  copy(Path('build/linux')/a.arch/'bitskim',Path('bin/bitskim'),0o755);copy(Path('clients/linux/org.bitskim.BitSkim.desktop'),Path('share/applications/org.bitskim.BitSkim.desktop'));copy(Path('public-web/assets/bitskim.png'),Path('share/icons/hicolor/512x512/apps/org.bitskim.BitSkim.png'))
 for source,name in [('LICENSE','LICENSE'),('THIRD-PARTY-NOTICES.MD','THIRD-PARTY-NOTICES.MD'),('assets/themes/CATPPUCCIN-LICENSE','CATPPUCCIN-LICENSE'),('assets/licenses/QRCODEGEN-LICENSE','QRCODEGEN-LICENSE'),('docs/LINUX.md','README.md'),('docs/TRANSFERS.md','TRANSFERS.md')]:copy(Path(source),Path('share/doc/bitskim')/name)
 print('Installed BitSkim under '+str(base))
if __name__=='__main__':main()
