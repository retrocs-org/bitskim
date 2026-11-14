#!/usr/bin/env python3
"""Validate Android package metadata, signature, resources and native page alignment."""
import argparse,re,struct,subprocess,zipfile
from pathlib import Path
from android_tools import CONFIG,ROOT,locations

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('apk',type=Path);p.add_argument('--abis',default='arm64-v8a');p.add_argument('--discovery',choices=['on','off'],default='on');args=p.parse_args()
 _,sdk,env=locations();tools=sdk/'build-tools'/CONFIG['build_tools']
 def run(name,*arguments):return subprocess.check_output([str(tools/name),*map(str,arguments)],env=env,text=True,stderr=subprocess.STDOUT)
 badging=run('aapt2','dump','badging',args.apk)
 for pattern in [r"package: name='org.bitskim.android'.*versionName='"+re.escape((ROOT/'VERSION').read_text().strip())+"'",r"(?:minSdkVersion|sdkVersion):'26'",r"targetSdkVersion:'37'"]:
  if not re.search(pattern,badging):raise SystemExit('Android package metadata mismatch: '+pattern)
 permissions=run('aapt2','dump','permissions',args.apk)
 if 'android.permission.ACCESS_LOCAL_NETWORK' not in permissions:raise SystemExit('Missing local network permission')
 run('apksigner','verify','--verbose',args.apk)
 run('zipalign','-c','-P','16','4',args.apk)
 machines={'arm64-v8a':183,'x86_64':62};expected=set(args.abis.split(','))
 with zipfile.ZipFile(args.apk) as archive:
  names=set(archive.namelist());abis={name.split('/')[1] for name in names if name.startswith('lib/') and name.endswith('/libbitskim.so')}
  if abis!=expected:raise SystemExit('Unexpected packaged ABIs: '+str(abis))
  for abi in abis:
   data=archive.read('lib/'+abi+'/libbitskim.so')
   if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=machines[abi]:raise SystemExit('Wrong native architecture')
   phoff=struct.unpack_from('<Q',data,32)[0];entsize,count=struct.unpack_from('<HH',data,54)
   alignments=[struct.unpack_from('<Q',data,phoff+i*entsize+48)[0] for i in range(count) if struct.unpack_from('<I',data,phoff+i*entsize)[0]==1]
   if not alignments or min(alignments)<16384:raise SystemExit('Native library lacks 16 KiB page alignment')
  for name in ['assets/palettes.json','assets/OPENSSL-LICENSE.txt']:
   if name not in names or not archive.read(name):raise SystemExit('Missing package resource: '+name)
 print('PASS: Android 0.2 metadata, signature, local network permission, resources, ABIs and 16 KiB page alignment')
 return 0
if __name__=='__main__':raise SystemExit(main())
