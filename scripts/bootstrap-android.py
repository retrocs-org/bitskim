#!/usr/bin/env python3
"""Prepare official Android tools outside the source checkout."""
import argparse,hashlib,platform,shutil,subprocess,tempfile,urllib.request,zipfile
from pathlib import Path
from android_tools import CONFIG,locations,sdkmanager,gradle,android_cli,download

def extract(archive,destination):
 destination.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(archive) as z:
  for item in z.infolist():
   path=(destination/item.filename).resolve()
   if destination.resolve() not in path.parents and path!=destination.resolve():raise SystemExit('Invalid archive path')
  z.extractall(destination)

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--licenses',action='store_true',help='Compatibility option for older SDK setup instructions')
 parser.add_argument('--install-sdk',action='store_true',help='Install pinned build packages with the official Android CLI')
 parser.add_argument('--runtime',action='store_true',help='Also install local Android testing tools and an arm64 Android 17 image')
 args=parser.parse_args();tools,sdk,env=locations()
 if not sdkmanager(sdk).is_file():
  if platform.system()!='Darwin' or platform.machine()!='arm64':raise SystemExit('Automatic command-line tool bootstrap currently supports Apple silicon macOS; install SDK tools externally and set ANDROID_SDK_ROOT on other hosts.')
  archive=download('https://dl.google.com/android/repository/'+CONFIG['command_tools_archive'],tools/'downloads'/CONFIG['command_tools_archive'],CONFIG['command_tools_sha256'])
  with tempfile.TemporaryDirectory(dir=tools) as temp:
   extract(archive,Path(temp));target=sdk/'cmdline-tools'/CONFIG['command_tools'];target.parent.mkdir(parents=True,exist_ok=True);shutil.move(str(Path(temp)/'cmdline-tools'),target)
  for file in sdkmanager(sdk).parent.iterdir():file.chmod(0o755)
 if not gradle(tools).is_file():
  version=CONFIG['gradle'];archive=download('https://services.gradle.org/distributions/gradle-'+version+'-bin.zip',tools/'downloads'/('gradle-'+version+'-bin.zip'),CONFIG['gradle_sha256']);extract(archive,tools);gradle(tools).chmod(0o755)
 print('Android tools:',tools,flush=True)
 if args.licenses:
  print('Current Android CLI manages SDK installation without the old --licenses command. Use --install-sdk.');return 0
 if args.install_sdk or args.runtime:
  packages=['platform-tools','platforms;'+CONFIG['platform'],'build-tools;'+CONFIG['build_tools'],'ndk;'+CONFIG['ndk']]
  if args.runtime:packages+=['emulator','system-images;android-37.0;google_apis;arm64-v8a']
  return subprocess.call([str(android_cli(sdk)),'--no-metrics','--sdk='+str(sdk),'sdk','install',*packages],env=env)
 print('Next: python3 scripts/bootstrap-android.py --install-sdk')
 return 0
if __name__=='__main__':raise SystemExit(main())
