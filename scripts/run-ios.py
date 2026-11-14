#!/usr/bin/env python3
"""Install and launch the built app on an available local iOS test device."""
import argparse,json,plistlib,re,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def devices():
 result=subprocess.run(['xcrun','simctl','list','devices','available','--json'],check=True,text=True,capture_output=True)
 available=[]
 for runtime,entries in json.loads(result.stdout).get('devices',{}).items():
  match=re.search(r'\.iOS-(\d+)(?:-(\d+))?',runtime)
  if not match or int(match.group(1))<16:continue
  for item in entries:
   if item.get('isAvailable',False):available.append(dict(item,runtime=runtime,version=(int(match.group(1)),int(match.group(2) or 0))))
 return sorted(available,key=lambda item:item['version'],reverse=True)
def choose(available,requested=None):
 if not available:raise ValueError('No available iOS 16+ simulator device. Install an iOS runtime and create a device in Xcode first.')
 if requested:
  matches=[item for item in available if item['udid'].lower()==requested.lower() or item['name']==requested or (requested=='booted' and item['state']=='Booted')]
  if len(matches)!=1:raise ValueError('Device name is missing or ambiguous. Use --list-devices and select an exact device ID.')
  return matches[0]
 return next((item for item in available if item['state']=='Booted'),next((item for item in available if 'iPhone' in item['name']),available[0]))
def open_window(udid):
 # Current Xcode uses Device Hub; older installations provide Simulator.
 for name in ('Device Hub','DeviceHub','Simulator'):
  window=subprocess.run(['open','-a',name,'--args','-CurrentDeviceUDID',udid],text=True,capture_output=True)
  if window.returncode==0:return True
 return False
def launch(app,device):
 info=plistlib.loads((app/'Info.plist').read_bytes())
 bundle=info['CFBundleIdentifier'];udid=device['udid']
 print('Starting '+device['name']+' ('+udid+')',flush=True)
 subprocess.run(['xcrun','simctl','bootstatus',udid,'-b'],check=True)
 # A headless Xcode installation can run the app even if the window app is absent.
 if not open_window(udid):print('Device Hub/Simulator window unavailable. Open it from Xcode → Open Developer Tool.',file=sys.stderr)
 subprocess.run(['xcrun','simctl','install',udid,str(app)],check=True)
 subprocess.run(['xcrun','simctl','launch','--terminate-running-process',udid,bundle],check=True)
 print('Launched '+bundle+' on '+device['name']+'. Existing app data was retained.')
def main(argv=None):
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--device',help='Exact device name, device ID, or booted')
 parser.add_argument('--list-devices',action='store_true')
 parser.add_argument('--app',type=Path,default=ROOT/'build/ios-derived/Build/Products/Debug-iphonesimulator/BitSkim.app')
 args=parser.parse_args(argv)
 try:
  available=devices()
  if args.list_devices:
   for item in available:print(item['name']+' | iOS '+'.'.join(map(str,item['version']))+' | '+item['state']+' | '+item['udid'])
   if not available:print('No available iOS 16+ devices.')
   return 0
  if not (args.app/'Info.plist').is_file():raise ValueError('Build the local iOS app first: ./scripts/build-ios.sh --run')
  launch(args.app,choose(available,args.device));return 0
 except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as error:
  print('iOS launch failed: '+str(error),file=sys.stderr);return 1
if __name__=='__main__':raise SystemExit(main())
