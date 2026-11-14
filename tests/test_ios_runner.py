"""Exercise device selection and install/launch sequencing without changing devices."""
import importlib.util,io,json,plistlib,subprocess,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('ios_runner',ROOT/'scripts/run-ios.py');runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
def device(name='iPhone Test',state='Shutdown',udid='ONE'):
 return dict(name=name,state=state,udid=udid,isAvailable=True,version=(27,2))
class IOSRunner(unittest.TestCase):
 def test_default_prefers_booted_then_iphone(self):
  self.assertEqual(runner.choose([device('iPad'),device()])['name'],'iPhone Test')
  self.assertEqual(runner.choose([device(),device('iPad','Booted','TWO')])['udid'],'TWO')
 def test_explicit_ids_and_ambiguous_names(self):
  entries=[device(),device(udid='TWO')]
  self.assertEqual(runner.choose(entries,'two')['udid'],'TWO')
  for value in ['iPhone Test','missing','booted']:
   with self.assertRaises(ValueError):runner.choose(entries,value)
  with self.assertRaises(ValueError):runner.choose([])
 def test_inventory_excludes_old_and_non_ios_devices(self):
  data={'devices':{'com.apple.CoreSimulator.SimRuntime.iOS-27-2':[device()], 'com.apple.CoreSimulator.SimRuntime.iOS-15-0':[device(udid='OLD')], 'com.apple.CoreSimulator.SimRuntime.tvOS-27-2':[device(udid='TV')]}}
  with patch.object(runner.subprocess,'run',return_value=subprocess.CompletedProcess([],0,json.dumps(data))):
   self.assertEqual([item['udid'] for item in runner.devices()],['ONE'])
 def test_viewer_supports_device_hub_and_legacy_simulator(self):
  for first_success in range(3):
   results=[subprocess.CompletedProcess([],1,'') for _ in range(first_success)]+[subprocess.CompletedProcess([],0,'')]
   with patch.object(runner.subprocess,'run',side_effect=results) as run:
    self.assertTrue(runner.open_window('ONE'))
    self.assertEqual(run.call_args_list[-1].args[0][2],('Device Hub','DeviceHub','Simulator')[first_success])
  with patch.object(runner.subprocess,'run',return_value=subprocess.CompletedProcess([],1,'')):
   self.assertFalse(runner.open_window('ONE'))
 def test_migration_failure_is_distinguished_from_successful_launch(self):
  with tempfile.TemporaryDirectory() as d:
   app=Path(d);(app/'Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'org.example.bitskim'}))
   boot=subprocess.CompletedProcess([],0,'Data Migration Failed','')
   good=subprocess.CompletedProcess([],0,'','')
   with patch.object(runner.subprocess,'run',side_effect=[boot,good,good,good]) as run,patch('sys.stderr',new_callable=io.StringIO) as error:
    runner.launch(app,device());self.assertIn('installation and launch succeeded',error.getvalue())
    self.assertFalse(any('erase' in call.args[0] or 'uninstall' in call.args[0] for call in run.call_args_list))
 def test_boot_failure_stops_before_install(self):
  with tempfile.TemporaryDirectory() as d:
   app=Path(d);(app/'Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'org.example.bitskim'}))
   with patch.object(runner.subprocess,'run',side_effect=subprocess.CalledProcessError(1,['bootstatus'])) as run:
    with self.assertRaises(subprocess.CalledProcessError):runner.launch(app,device())
    self.assertEqual(run.call_count,1)
 def test_launch_preserves_existing_data_and_uses_built_bundle_identifier(self):
  with tempfile.TemporaryDirectory() as d:
   app=Path(d)/'App with spaces.app';app.mkdir();(app/'Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier':'org.example.bitskim'}))
   with patch.object(runner.subprocess,'run',return_value=subprocess.CompletedProcess([],0,'')) as run:
    runner.launch(app,device());calls=[item.args[0] for item in run.call_args_list]
   self.assertEqual(calls[0],['xcrun','simctl','bootstatus','ONE','-b'])
   self.assertEqual(calls[2],['xcrun','simctl','install','ONE',str(app)])
   self.assertEqual(calls[3],['xcrun','simctl','launch','--terminate-running-process','ONE','org.example.bitskim'])
   self.assertFalse(any('erase' in command or 'uninstall' in command for command in calls))
