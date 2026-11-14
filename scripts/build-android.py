#!/usr/bin/env python3
"""Build the experimental Android client with shared external tools."""
import argparse,json,os,platform,shutil,subprocess,sys,tarfile
from pathlib import Path
from android_tools import CONFIG,ROOT,locations,gradle,download

ABIS={'arm64-v8a':('aarch64-linux-android','android-arm64'),'x86_64':('x86_64-linux-android','android-x86_64')}
OPENSSL='3.5.9';OPENSSL_SHA='603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a'
def tls(tools,sdk,env,abi):
 prefix=tools/'openssl'/OPENSSL/('ndk-'+CONFIG['ndk'])/('api'+str(CONFIG['min_sdk']))/abi
 if (prefix/'lib/libssl.a').is_file() and (prefix/'lib/libcrypto.a').is_file():return prefix
 archive=download('https://github.com/openssl/openssl/releases/download/openssl-'+OPENSSL+'/openssl-'+OPENSSL+'.tar.gz',tools/'downloads'/('openssl-'+OPENSSL+'.tar.gz'),OPENSSL_SHA)
 source=tools/'openssl-src'/('openssl-'+OPENSSL)
 if not source.is_dir():
  source.parent.mkdir(parents=True,exist_ok=True)
  with tarfile.open(archive) as t:
   for member in t.getmembers():
    path=(source.parent/member.name).resolve()
    if source.parent.resolve() not in path.parents:raise SystemExit('Invalid OpenSSL archive path')
   t.extractall(source.parent)
 host=next((sdk/'ndk'/CONFIG['ndk']/'toolchains/llvm/prebuilt').iterdir());bin=host/'bin'
 build=tools/'build'/('openssl-'+OPENSSL+'-'+abi);build.mkdir(parents=True,exist_ok=True)
 native=env.copy();native.update(ANDROID_NDK_ROOT=str(sdk/'ndk'/CONFIG['ndk']),CC=str(bin/(ABIS[abi][0]+str(CONFIG['min_sdk'])+'-clang')),AR=str(bin/'llvm-ar'),RANLIB=str(bin/'llvm-ranlib'))
 native['PATH']=str(bin)+os.pathsep+native['PATH']
 subprocess.run(['perl',str(source/'Configure'),ABIS[abi][1],'-D__ANDROID_API__='+str(CONFIG['min_sdk']),'no-shared','no-tests','no-docs','no-module','--prefix='+str(prefix),'--libdir=lib'],cwd=build,env=native,check=True)
 subprocess.run(['make','-j'+str(min(os.cpu_count() or 4,8))],cwd=build,env=native,check=True)
 subprocess.run(['make','install_sw'],cwd=build,env=native,check=True)
 return prefix

def main():
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--abis',default='arm64-v8a',help='arm64-v8a, x86_64 or both, comma-separated')
 parser.add_argument('--discovery',choices=['on','off'],default='on')
 parser.add_argument('--run',action='store_true')
 parser.add_argument('--device',help='ADB serial to install on')
 parser.add_argument('--avd',help='Local test device name to start')
 parser.add_argument('--list-devices',action='store_true')
 parser.add_argument('--test',action='store_true',help='Run debug native runtime checks after installation; requires --run')
 args=parser.parse_args()
 version=(ROOT/'VERSION').read_text().strip()
 if args.list_devices:return subprocess.call([sys.executable,str(ROOT/'scripts/run-android.py'),'--list-devices'])
 if (args.device or args.avd or args.test) and not args.run:parser.error('Use --device, --avd and --test together with --run.')
 abis=args.abis.split(',')
 if not abis or len(set(abis))!=len(abis) or any(abi not in ABIS for abi in abis):parser.error('Supported ABIs: arm64-v8a,x86_64')
 tools,sdk,env=locations();ndk=sdk/'ndk'/CONFIG['ndk']
 if not ndk.is_dir() or not gradle(tools).is_file() or not (sdk/'platforms'/CONFIG['platform']/'android.jar').is_file():raise SystemExit('Prepare shared tools first: python3 scripts/bootstrap-android.py --install-sdk')
 native_root=ROOT/'build/android/jniLibs';native_root.mkdir(parents=True,exist_ok=True)
 for folder in native_root.iterdir():
  if folder.is_dir() and folder.name not in abis:shutil.rmtree(folder)
 host=next((ndk/'toolchains/llvm/prebuilt').iterdir());binary=host/'bin'
 for abi in abis:
  prefix=tls(tools,sdk,env,abi);output=native_root/abi/'libbitskim.so';output.parent.mkdir(parents=True,exist_ok=True)
  command=[str(binary/(ABIS[abi][0]+str(CONFIG['min_sdk'])+'-clang')),'-std=c99','-O2','-fPIC','-fvisibility=hidden','-shared','-DBS_OPENSSL','-I'+str(ROOT/'core'),'-I'+str(prefix/'include'),str(ROOT/'core/bitskim.c'),str(ROOT/'core/platform.c'),str(ROOT/'clients/android/native/bridge.c'),str(prefix/'lib/libssl.a'),str(prefix/'lib/libcrypto.a'),'-Wl,--no-undefined','-Wl,-soname,libbitskim.so','-Wl,-z,max-page-size=16384','-Wl,-z,common-page-size=16384','-ldl','-lm','-pthread','-o',str(output)]
  if args.discovery=='off':command.insert(1,'-DBS_NO_BROADCAST')
  subprocess.run(command,env=env,check=True)
 assets=ROOT/'build/android/assets';assets.mkdir(parents=True,exist_ok=True);shutil.copyfile(tools/'openssl-src'/('openssl-'+OPENSSL)/'LICENSE.txt',assets/'OPENSSL-LICENSE.txt')
 for notice in ['THIRD-PARTY-NOTICES.MD','LICENSE.md']:shutil.copyfile(ROOT/notice,assets/notice)
 key=tools/'debug.keystore'
 if not key.exists():subprocess.run([str(Path(env['JAVA_HOME'])/'bin/keytool'),'-genkeypair','-keystore',str(key),'-storepass','android','-keypass','android','-alias','androiddebugkey','-keyalg','RSA','-keysize','2048','-validity','10000','-dname','CN=Android Debug,O=Android,C=US'],env=env,check=True)
 key.chmod(0o600)
 subprocess.run([str(gradle(tools)),'--no-daemon','--project-cache-dir',str(ROOT/'build/android/gradle-state'),'-p',str(ROOT/'clients/android'),'-PbitskimDiscovery='+str(args.discovery=='on').lower(),'assembleDebug'],env=env,check=True)
 source=ROOT/'build/android/gradle-app/outputs/apk/debug/app-debug.apk';apk=ROOT/'build/android'/('BitSkim-'+version+'-Android-debug.apk');shutil.copyfile(source,apk)
 subprocess.run([sys.executable,str(ROOT/'scripts/check-android-build.py'),str(apk),'--abis',args.abis,'--discovery',args.discovery],env=env,check=True)
 print('Built experimental Android client:',apk,flush=True)
 if args.run:
  command=[sys.executable,str(ROOT/'scripts/run-android.py')]
  if args.device:command+=['--device',args.device]
  if args.avd:command+=['--avd',args.avd]
  if args.test:command+=['--test']
  subprocess.run(command,env=env,check=True)
 return 0
if __name__=='__main__':raise SystemExit(main())
