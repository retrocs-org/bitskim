#!/usr/bin/env python3
"""Install and start a local Android debug build without erasing application data."""
import argparse,os,platform,subprocess,time
from pathlib import Path
from android_tools import CONFIG,ROOT,locations

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--device',help='ADB serial');p.add_argument('--avd',help='Existing local device name, or BitSkim_API_37 to create it');p.add_argument('--list-devices',action='store_true');p.add_argument('--test',action='store_true');args=p.parse_args()
 tools,sdk,env=locations();adb=sdk/'platform-tools/adb';runtime=sdk/'emulator/emulator'
 if not adb.is_file():raise SystemExit('Run python3 scripts/bootstrap-android.py --install-sdk first.')
 def run(*items,check=True):return subprocess.run([str(adb),*map(str,items)],env=env,text=True,capture_output=True,check=check)
 def devices():return [(line.split()[0],line.split()[1]) for line in run('devices').stdout.splitlines()[1:] if len(line.split())>=2]
 if args.list_devices:
  print(run('devices','-l').stdout,end='')
  if runtime.is_file():subprocess.run([str(runtime),'-list-avds'],env=env,check=True)
  return 0
 if args.avd:
  if args.device:p.error('Choose --device or --avd, not both.')
  if not runtime.is_file():raise SystemExit('Run python3 scripts/bootstrap-android.py --runtime first.')
  names=subprocess.check_output([str(runtime),'-list-avds'],env=env,text=True).splitlines()
  if args.avd not in names:
   if args.avd!='BitSkim_API_37':raise SystemExit('Unknown local device. Use --list-devices or --avd BitSkim_API_37.')
   image='system-images;'+CONFIG['platform']+';google_apis;arm64-v8a'
   if not (sdk/'system-images'/CONFIG['platform']/'google_apis/arm64-v8a').is_dir():raise SystemExit('Run python3 scripts/bootstrap-android.py --runtime first.')
   manager=sdk/'cmdline-tools'/CONFIG['command_tools']/'bin/avdmanager';(tools/'avd').mkdir(parents=True,exist_ok=True)
   subprocess.run([str(manager),'create','avd','--name',args.avd,'--package',image,'--path',str(tools/'avd'/(args.avd+'.avd')),'--device','pixel_8'],input='no\n',env=env,text=True,check=True)
  serial=None
  for candidate,state in devices():
   if state=='device' and candidate.startswith('emulator-') and run('-s',candidate,'emu','avd','name').stdout.splitlines()[0]==args.avd:serial=candidate
  if serial is None:
   log=ROOT/'build/android/device.log';log.parent.mkdir(parents=True,exist_ok=True)
   with log.open('ab') as output:subprocess.Popen([str(runtime),'-avd',args.avd,'-no-snapshot-load'],env=env,stdout=output,stderr=subprocess.STDOUT,start_new_session=True)
   deadline=time.monotonic()+180
   print('Starting local Android device…',flush=True)
   while time.monotonic()<deadline:
    for candidate,state in devices():
     if state=='device' and candidate.startswith('emulator-') and run('-s',candidate,'emu','avd','name',check=False).stdout.splitlines()[:1]==[args.avd]:serial=candidate;break
    if serial:break
    time.sleep(2)
   if serial is None:raise SystemExit('Device did not start. Inspect build/android/device.log; no data was erased.')
 else:
  available=devices()
  if args.device:
   if (args.device,'device') not in available:raise SystemExit('Selected device is unavailable or not authorized. Check --list-devices.')
   serial=args.device
  elif len(available)==1 and available[0][1]=='device':serial=available[0][0]
  else:raise SystemExit('Select a connected device with --device SERIAL, or start --avd BitSkim_API_37.')
 deadline=time.monotonic()+180
 while time.monotonic()<deadline:
  if run('-s',serial,'shell','getprop','sys.boot_completed',check=False).stdout.strip()=='1':break
  time.sleep(2)
 else:raise SystemExit('Android did not finish booting; no data was erased.')
 version=(ROOT/'VERSION').read_text().strip();apk=ROOT/'build/android'/('BitSkim-'+version+'-Android-debug.apk')
 if not apk.is_file():raise SystemExit('Build the Android app first: ./scripts/build-android.sh')
 print(run('-s',serial,'install','-r',apk).stdout,end='')
 if args.test:
  run('-s',serial,'shell','am','force-stop','org.bitskim.android')
  result=run('-s',serial,'shell','am','instrument','-w','-r','org.bitskim.android/.RuntimeChecks',check=False)
  print(result.stdout,end='');print(result.stderr,end='')
  if result.returncode or 'PASS Android runtime checks' not in result.stdout or 'INSTRUMENTATION_CODE: -1' not in result.stdout:raise SystemExit('Android runtime checks failed.')
 if serial.startswith('emulator-'):
  forwards=run('forward','--list').stdout.splitlines()
  existing=[line for line in forwards if len(line.split())>=3 and line.split()[1]=='tcp:46442']
  if not existing or existing==[serial+' tcp:46442 tcp:46440']:run('-s',serial,'forward','tcp:46442','tcp:46440');print('Mac → Android manual address: 127.0.0.1:46442')
  else:print('Port 46442 already forwarded elsewhere; leaving it unchanged.')
 run('-s',serial,'shell','am','force-stop','org.bitskim.android')
 print(run('-s',serial,'shell','am','start','-W','-n','org.bitskim.android/.MainActivity').stdout,end='')
 print('Launched BitSkim '+version+' on '+serial+'. Existing app data was retained.')
 return 0
if __name__=='__main__':raise SystemExit(main())
