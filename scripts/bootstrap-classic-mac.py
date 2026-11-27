#!/usr/bin/env python3
"""Build pinned Retro68 PowerPC tools under the external shared tools directory."""
import argparse,os,subprocess
from pathlib import Path
PIN='a4a389c41e90b1095a40ccf13f28678b6cfbba86'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--directory',type=Path,default=Path.home()/'Development/tools/classic-mac');a=p.parse_args();base=a.directory.expanduser().resolve();source=base/'Retro68';build=base/'build';build.mkdir(parents=True,exist_ok=True)
if not source.exists():
 subprocess.run(['git','clone','--no-checkout','https://github.com/autc04/Retro68.git',str(source)],check=True)
 subprocess.run(['git','-C',str(source),'checkout','--detach',PIN],check=True)
if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()!=PIN:raise SystemExit('Existing Retro68 checkout differs from the pinned revision; review it manually rather than overwriting it.')
subprocess.run(['git','-C',str(source),'submodule','update','--init','--recursive'],check=True)
env=dict(os.environ);env['PATH']='/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/flex/bin:/opt/homebrew/opt/texinfo/bin:'+env['PATH'];env.setdefault('MAKEFLAGS','-j6');env.setdefault('CMAKE_POLICY_VERSION_MINIMUM','3.5')
subprocess.run([str(source/'build-toolchain.bash'),'--no-68k','--no-carbon','--multiversal'],cwd=build,env=env,check=True)
print('PowerPC compiler: '+str(build/'toolchain/bin/powerpc-apple-macos-gcc'))
print('Networking interfaces are a separate prerequisite; run scripts/check-classic-mac.py.')
