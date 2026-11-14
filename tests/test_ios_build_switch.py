"""Check build-switch wiring without downloading dependencies or invoking Xcode."""
from pathlib import Path
import os,plistlib,shutil,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class IOSBuildSwitch(unittest.TestCase):
 def run_build(self,*args):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'scripts').mkdir();(root/'tools').mkdir();(root/'VERSION').write_text('0.2\n')
   shutil.copyfile(ROOT/'scripts/build-ios.sh',root/'scripts/build-ios.sh')
   (root/'scripts/run-ios.py').write_text('import sys; print("RUN", *sys.argv[1:])\n')
   (root/'scripts/check-ios-build.py').write_text('import sys; print("CHECK", *sys.argv[1:])\n')
   for sdk in ('iphoneos','iphonesimulator'):
    lib=root/('work/openssl-ios-'+sdk)/'lib';lib.mkdir(parents=True);(lib/'libssl.a').touch()
   (root/'tools/xcodegen').write_text('#!/bin/sh\nexit 0\n')
   (root/'tools/xcodebuild').write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$CAPTURE"\n')
   for tool in (root/'tools').iterdir():tool.chmod(0o755)
   env=dict(os.environ,PATH=str(root/'tools')+os.pathsep+os.environ['PATH'],CAPTURE=str(root/'args'))
   result=subprocess.run(['sh','scripts/build-ios.sh',*args],cwd=root,env=env,text=True,capture_output=True)
   return result,(root/'args').read_text().splitlines() if (root/'args').exists() else []
 def test_default_and_explicit_off_remove_discovery_entitlement(self):
  for args in [(),('iphoneos','--multicast=off')]:
   result,settings=self.run_build(*args);self.assertEqual(result.returncode,0,result.stderr)
   self.assertIn('BITSKIM_BROADCAST=NO',settings);self.assertIn('BITSKIM_BROADCAST_DEFINE=BS_NO_BROADCAST',settings)
   self.assertTrue(any(x.endswith('/clients/ios/Manual.entitlements') for x in settings))
   self.assertIn('off',result.stdout)
 def test_on_enables_all_settings_and_preserves_signing_overrides(self):
  result,settings=self.run_build('iphoneos','--multicast=on','DEVELOPMENT_TEAM=EXAMPLE','BITSKIM_APP_GROUP=group.example')
  self.assertEqual(result.returncode,0,result.stderr)
  for expected in ['BITSKIM_BROADCAST=YES','BITSKIM_BROADCAST_DEFINE=','DEVELOPMENT_TEAM=EXAMPLE','BITSKIM_APP_GROUP=group.example']:self.assertIn(expected,settings)
  self.assertTrue(any(x.endswith('/clients/ios/Discovery.entitlements') for x in settings))
  self.assertIn('CHECK iphoneos on',result.stdout)
 def test_invalid_mode_and_conflicting_overrides_stop_before_build(self):
  for args in [('iphoneos','--multicast=maybe'),('iphoneos','--multicast=off','BITSKIM_BROADCAST=YES'),('iphoneos','CODE_SIGN_ENTITLEMENTS=custom')]:
   result,settings=self.run_build(*args);self.assertNotEqual(result.returncode,0);self.assertEqual(settings,[])
 def test_help_does_not_build(self):
  for flag in ['--help','-h']:
   result,settings=self.run_build(flag);self.assertEqual(result.returncode,0);self.assertIn('--multicast=off|on',result.stdout);self.assertEqual(settings,[])
 def test_run_uses_local_signing_and_forwards_device_as_one_argument(self):
  result,settings=self.run_build('--run','--device','iPhone Test')
  self.assertEqual(result.returncode,0,result.stderr);self.assertIn('CODE_SIGNING_ALLOWED=YES',settings)
  self.assertIn('RUN --device iPhone Test',result.stdout)
 def test_run_is_rejected_for_device_sdk_and_device_requires_run(self):
  for args in [('iphoneos','--run'),('--device','iPhone Test'),('--device',)]:
   result,settings=self.run_build(*args);self.assertNotEqual(result.returncode,0);self.assertEqual(settings,[])
 def test_listing_devices_skips_the_build(self):
  result,settings=self.run_build('--list-devices');self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual(settings,[]);self.assertIn('RUN --list-devices',result.stdout)
 def test_manual_entitlements_omit_multicast_and_share_extension_stays_manual(self):
  with (ROOT/'clients/ios/Manual.entitlements').open('rb') as f:manual=plistlib.load(f)
  with (ROOT/'clients/ios/Discovery.entitlements').open('rb') as f:discovery=plistlib.load(f)
  self.assertNotIn('com.apple.developer.networking.multicast',manual)
  self.assertIs(discovery['com.apple.developer.networking.multicast'],True)
  self.assertEqual(manual['com.apple.security.application-groups'],discovery['com.apple.security.application-groups'])
  share=(ROOT/'clients/ios/project.yml').read_text().split('  BitSkimShare:',1)[1].split('  BitSkimCoreTests:',1)[0]
  self.assertIn('clients/ios/Manual.entitlements',share)
