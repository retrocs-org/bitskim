#!/usr/bin/env python3
"""Check the classic Mac port prerequisites; this does not produce a transfer client."""
import argparse,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--toolchain',type=Path,default=Path.home()/'Development/tools/classic-mac/build/toolchain');p.add_argument('--interfaces',type=Path,default=os.environ.get('CLASSIC_MAC_INTERFACES'));a=p.parse_args();compiler=a.toolchain/'bin/powerpc-apple-macos-gcc'
if not compiler.is_file():raise SystemExit('Build shared Retro68 first: python3 scripts/bootstrap-classic-mac.py')
subprocess.run([str(compiler),'--version'],check=True)
if not a.interfaces:raise SystemExit('Classic Mac networking SDK missing. Supply --interfaces PATH to your Universal Interfaces 3.x CIncludes. The bundled Multiversal Interfaces lack Open Transport; no network client is advertised.')
headers=a.interfaces.expanduser().resolve()
for name in ('OpenTransport.h','OpenTptInternet.h','Files.h'):
 if not (headers/name).is_file():raise SystemExit('Missing SDK header: '+str(headers/name))
code='#include <OpenTransport.h>\n#include <OpenTptInternet.h>\n#include <Files.h>\nint check(void){InetAddress a;return OTInitInetAddress(&a,46440,0); }\n'
subprocess.run([str(compiler),'-x','c','-I'+str(headers),'-fsyntax-only','-'],input=code,text=True,check=True)
print('SDK header check passed. Open Transport adapters, safe HFS file operations and native transfers still need implementation/acceptance; this is not a release build.')
