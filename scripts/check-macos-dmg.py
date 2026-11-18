#!/usr/bin/env python3
"""Check the mounted and copied Mac app, preserving existing installations/profiles."""
import argparse
import hashlib
import json
from pathlib import Path
import os
import plistlib
import subprocess
import tempfile
from release_paths import output_dir

ROOT=Path(__file__).resolve().parents[1]

def check_image(dmg, report, notarized=False, native_views=False):
    dmg=Path(dmg).resolve();report=Path(report).resolve();report.mkdir(parents=True,exist_ok=True)
    results={'version':'0.2','dmg_sha256':hashlib.sha256(dmg.read_bytes()).hexdigest(),'checks':[],'result':'failed'}
    def run(command):
        result=subprocess.run([str(item) for item in command],capture_output=True,text=True)
        results['checks'].append({'command':str(command[0]),'status':result.returncode,'output':result.stdout+result.stderr})
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    try:
        with tempfile.TemporaryDirectory(prefix='bitskim-package-check-') as temporary:
            root=Path(temporary);mount=root/'Mount';mount.mkdir();attached=False
            try:
                run(['hdiutil','verify',dmg]);run(['hdiutil','attach','-readonly','-nobrowse','-mountpoint',mount,dmg]);attached=True
                if not (mount/'Applications').is_symlink() or os.readlink(mount/'Applications')!='/Applications':raise ValueError('Missing drag-to-Applications link')
                original=mount/'BitSkim.app';installed=root/'Installed/BitSkim.app';installed.parent.mkdir()
                run(['ditto',original,installed])
                for app in (original,installed):
                    metadata=plistlib.loads((app/'Contents/Info.plist').read_bytes())
                    if metadata.get('CFBundleShortVersionString')!='0.2' or metadata.get('CFBundleIdentifier')!='org.bitskim.client':raise ValueError('Unexpected application/version in image')
                    run(['codesign','--verify','--deep','--strict',app])
                if notarized:
                    run(['xcrun','stapler','validate',installed]);run(['xcrun','stapler','validate',dmg])
                    run(['spctl','--assess','--type','execute','--verbose=2',installed]);run(['spctl','--assess','--type','open','--context','context:primary-signature','--verbose=2',dmg])
                if native_views:run([installed/'Contents/MacOS/BitSkim','--check-startup','--review-output',report/'views'])
            finally:
                if attached:run(['hdiutil','detach',mount])
        results['result']='passed'
        print('PASS Mac image, copied app, version and strict signatures'+('; notarization/Gatekeeper' if notarized else '')+('; native startup/views' if native_views else ''))
        return results
    finally:(report/'package-review.json').write_text(json.dumps(results,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dmg',type=Path,default=output_dir(ROOT)/'BitSkim-0.2-macOS-arm64.dmg');parser.add_argument('--review-output',type=Path,default=ROOT/'work/macos-package-review');parser.add_argument('--require-notarization',action='store_true');parser.add_argument('--native-views',action='store_true');args=parser.parse_args()
    check_image(args.dmg,args.review_output,args.require_notarization,args.native_views)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as error:raise SystemExit(str(error))
