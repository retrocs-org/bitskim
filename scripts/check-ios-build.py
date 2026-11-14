#!/usr/bin/env python3
"""Validate the development app and embedded extension as built, not just source."""
import hashlib,plistlib,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def platform(binary):
 data=binary.read_bytes()
 if data[:4]!=b'\xcf\xfa\xed\xfe':raise ValueError('Expected a thin arm64 Mach-O: '+binary.name)
 header=struct.unpack_from('<8I',data)
 if header[1]!=0x0100000c:raise ValueError('Expected arm64: '+binary.name)
 cursor=32
 for _ in range(header[4]):
  command,size=struct.unpack_from('<II',data,cursor)
  if size<8 or cursor+size>len(data):raise ValueError('Invalid Mach-O commands')
  if command==0x32:return struct.unpack_from('<I',data,cursor+8)[0]
  cursor+=size
 raise ValueError('Missing Apple build-platform record')
def check(sdk):
 if sdk not in ('iphoneos','iphonesimulator'):raise ValueError('Unknown SDK')
 version=(ROOT/'VERSION').read_text().strip()
 app=ROOT/'build/ios-derived/Build/Products'/('Debug-'+sdk)/'BitSkim.app'
 metadata=[]
 for folder,source in [(app,'App'),(app/'PlugIns/BitSkimShare.appex','Share')]:
  info=plistlib.loads((folder/'Info.plist').read_bytes());metadata.append(info)
  if info['CFBundleShortVersionString']!=version:raise ValueError('Incorrect application version')
  if info.get('UIDeviceFamily')!=[1,2]:raise ValueError('Expected iPhone and iPad families')
  expected=ROOT/'clients/ios'/source/'PrivacyInfo.xcprivacy'
  if hashlib.sha256((folder/'PrivacyInfo.xcprivacy').read_bytes()).digest()!=hashlib.sha256(expected.read_bytes()).digest():raise ValueError('Missing/stale privacy manifest: '+source)
  if platform(folder/info['CFBundleExecutable'])!=(2 if sdk=='iphoneos' else 7):raise ValueError('Wrong Apple platform in '+source)
 if metadata[1]['CFBundleIdentifier']!=metadata[0]['CFBundleIdentifier']+'.share':raise ValueError('Extension identifier must derive from the app identifier')
 if not metadata[0].get('BitSkimAppGroup') or metadata[0]['BitSkimAppGroup']!=metadata[1].get('BitSkimAppGroup') or '$(' in metadata[0]['BitSkimAppGroup']:raise ValueError('App Group configuration is inconsistent')
 print('PASS: iOS version, arm64 platform, iPhone/iPad support, embedded extension, App Group and privacy resources')
if __name__=='__main__':
 try:check(sys.argv[1])
 except (IndexError,KeyError,OSError,ValueError,struct.error) as error:raise SystemExit(str(error))
