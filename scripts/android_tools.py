#!/usr/bin/env python3
"""Shared Android tool locations; SDKs and downloaded tools never live in Git."""
import hashlib,json,os,shutil,subprocess,urllib.request
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
CONFIG=json.loads((ROOT/'clients/android/toolchain.json').read_text())
def locations():
 tools=Path(os.environ.get('BITSKIM_ANDROID_TOOLS',Path.home()/'Development/tools/android')).expanduser().resolve()
 sdk=Path(os.environ.get('ANDROID_SDK_ROOT',os.environ.get('ANDROID_HOME',tools/'sdk'))).expanduser().resolve()
 if ROOT==sdk or ROOT in sdk.parents or ROOT==tools or ROOT in tools.parents:raise SystemExit('Keep Android SDK/build tools outside the source checkout.')
 java=os.environ.get('JAVA_HOME')
 if not java:
  brew=shutil.which('brew')
  if brew:
   prefix=subprocess.run([brew,'--prefix','openjdk@21'],capture_output=True,text=True)
   if prefix.returncode==0:java=str(Path(prefix.stdout.strip())/'libexec/openjdk.jdk/Contents/Home')
 if not java or not (Path(java)/'bin/java').is_file():raise SystemExit('Install JDK 21 (brew install openjdk@21 on macOS), or set JAVA_HOME.')
 env=os.environ.copy();env.update(JAVA_HOME=java,ANDROID_HOME=str(sdk),ANDROID_SDK_ROOT=str(sdk),GRADLE_USER_HOME=str(tools/'gradle-user-home'),ANDROID_USER_HOME=str(tools/'user'),ANDROID_AVD_HOME=str(tools/'avd'),ANDROID_EMULATOR_HOME=str(tools/'user'))
 env['PATH']=str(Path(java)/'bin')+os.pathsep+env.get('PATH','')
 return tools,sdk,env
def sdkmanager(sdk):return sdk/'cmdline-tools'/CONFIG['command_tools']/'bin/sdkmanager'
def gradle(tools):return tools/('gradle-'+CONFIG['gradle'])/'bin/gradle'

def android_cli(sdk):return sdk/'cmdline-tools'/CONFIG['command_tools']/'bin/android'

def download(url,destination,digest):
 destination.parent.mkdir(parents=True,exist_ok=True)
 if not destination.is_file() or hashlib.sha256(destination.read_bytes()).hexdigest()!=digest:
  partial=destination.with_suffix(destination.suffix+'.partial')
  urllib.request.urlretrieve(url,partial)
  if hashlib.sha256(partial.read_bytes()).hexdigest()!=digest:partial.unlink();raise SystemExit('Download checksum mismatch: '+destination.name)
  partial.replace(destination)
 return destination
