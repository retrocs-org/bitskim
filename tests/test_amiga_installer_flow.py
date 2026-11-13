"""Execute the generated Installer discovery branch with a simulated filesystem.
This checks branching and directory reuse, not native Installer compatibility.
"""
from pathlib import Path
import json,re,subprocess,unittest
ROOT=Path(__file__).resolve().parents[1]
class Literal(str):pass
class Stop(Exception):pass
class Flow:
 def __init__(self,files,novice=False,expert=False,boot=True,launch=True,action=0):
  self.files=dict(files);self.vars={'#src':'RAM:Package','@user-level':0 if novice else 2 if expert else 1};self.boot=boot;self.launch=launch;self.action=action;self.questions=[];self.directories=[];self.choices=[];self.messages=[]
 def run(self,text):
  text='\n'.join(line for line in text.splitlines() if not line.lstrip().startswith(';'))
  tokens=re.findall(r'"(?:\\.|[^"\\])*"|[()]|[^\s()]+',text);at=0
  def parse():
   nonlocal at
   token=tokens[at];at+=1
   if token=='(':
    out=[]
    while tokens[at]!=')':out.append(parse())
    at+=1;return out
   if token.startswith('"'):return Literal(json.loads(token))
   try:return int(token)
   except ValueError:return token
  while at<len(tokens):self.ev(parse())
 def ev(self,x):
  if isinstance(x,Literal) or isinstance(x,int):return x
  if isinstance(x,str):return self.vars.get(x,0)
  if not x:return 0
  if isinstance(x[0],list):
   result=0
   for expr in x:result=self.ev(expr)
   return result
  op=x[0];args=x[1:]
  if op=='user':self.vars['@user-level']=self.ev(args[0]);return self.vars['@user-level']
  if op=='set':self.vars[args[0]]=self.ev(args[1]);return self.vars[args[0]]
  if op=='if':return self.ev(args[1]) if self.ev(args[0]) else self.ev(args[2]) if len(args)>2 else 0
  if op=='and':return all(self.ev(a) for a in args)
  if op=='or':return any(self.ev(a) for a in args)
  if op=='not':return not self.ev(args[0])
  if op in ('askdir','askchoice','askbool'):
   tags={a[0]:a[1:] for a in args}
   if op=='askdir':
    value=self.ev(tags['default'][0]);self.directories.append(value);return value
   if op=='askchoice' and self.vars['@user-level']==0:return 0
   if op=='askchoice':self.choices.append([self.ev(a) for a in tags['choices']]);return self.action
   if self.vars['@user-level']==0:return self.ev(tags['default'][0])
   prompt=self.ev(tags['prompt'][0]);self.questions.append(prompt)
   if prompt=='Start BitSkim at boot?':return self.boot
   if prompt=='Start BitSkim after installation?':return self.launch
   return 1
  if op=='copyfiles':
   tags={a[0]:a[1:] for a in args};source=self.ev(tags['source'][0]);dest=self.ev(tags['dest'][0])
   name=self.ev(tags['newname'][0]) if 'newname' in tags else source.rsplit('/',1)[-1]
   self.files[dest+('' if dest.endswith((':','/')) else '/')+name]=self.files[source];return 1
  if op=='delete':self.files.pop(self.ev(args[0]),None);return 1
  if op=='run':return getattr(self,'run_result',0)
  a=[self.ev(v) for v in args]
  if op=='=':return a[0]==a[1]
  if op=='<>':return a[0]!=a[1]
  if op=='>':return a[0]>a[1]
  if op=='exists':return 0 if a[0] not in self.files else 2 if self.files[a[0]] is None else 1
  if op=='getsize':return len(self.files[a[0]])
  if op=='getenv':return self.files['ENV:'+a[0]]
  if op=='rename':
   old,new=a
   if new in self.files:raise AssertionError('Overwriting rename destination')
   entries={key:value for key,value in self.files.items() if key==old or key.startswith(old+'/')}
   for key in entries:del self.files[key]
   for key,value in entries.items():self.files[new+key[len(old):]]=value
   return 1
  if op=='makedir':self.files[a[0]]=None;return 1
  if op=='tackon':return a[0]+('' if a[0].endswith((':','/')) else '/')+a[1]
  if op=='pathonly':return a[0].rsplit('/',1)[0] if '/' in a[0] else a[0].split(':')[0]+':'
  if op=='fileonly':return a[0].rsplit('/',1)[-1].rsplit(':',1)[-1]
  if op=='expandpath':return a[0].rstrip('/')
  if op=='cat':return ''.join(map(str,a))
  if op=='message':self.messages.append(a[0]);return 1
  if op in ('abort','exit'):raise Stop(a[0] if a else '')
  if op=='quiet':return 0
  raise AssertionError('Unhandled Installer operation: '+str(op))
class InstallerDiscovery(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  subprocess.run(['python3','scripts/build-amiga-installer.py'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
  text=(ROOT/'build/amiga/Install-BitSkim').read_text()
  cls.flow=text.split('; BEGIN installation discovery',1)[1].split('; END installation discovery',1)[0]
  cls.stopping=text.split('; BEGIN stop running clients',1)[1].split('; END stop running clients',1)[0]
  cls.pathchecks=text.split('; BEGIN installation path checks',1)[1].split('; END installation path checks',1)[0]
  cls.preserve=text.split('; BEGIN preserve blocking drawer',1)[1].split('; END preserve blocking drawer',1)[0]
  cls.confirmation=text.split('; BEGIN uninstall confirmation',1)[1].split('; END uninstall confirmation',1)[0]
 def recorded(self,base='ENV:BitSkim',main=True):
  files={base+'/'+leaf:value for leaf,value in [('app.path','Work:Apps/BitSkim'),('prefs.path','Work:Prefs/BitSkimPrefs'),('shared.path','Data:BitSkim'),('guide.path','Docs:Guides/BitSkim.guide')]}
  if main:files['Work:Apps/BitSkim']='binary'
  return files
 def test_recorded_installation_skips_destinations_and_shows_all_directories(self):
  f=Flow(self.recorded());f.run(self.flow)
  self.assertEqual(f.directories,[]);self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
  for directory in ('Work:Apps','Work:Prefs','Data:BitSkim','HELP:'):self.assertIn(directory,f.vars['#locations'])
 def test_missing_main_is_a_repair_update_or_uninstall_not_new_install(self):
  f=Flow(self.recorded(main=False));f.run(self.flow)
  self.assertEqual(f.vars['#installed'],1);self.assertEqual(f.directories,[])
  self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
 def test_archived_records_are_restored_before_detection(self):
  f=Flow(self.recorded('ENVARC:BitSkim'));f.run(self.flow)
  self.assertEqual(f.directories,[]);self.assertEqual(f.vars['#dest'],'Work:Apps')
  self.assertEqual(f.files['ENV:BitSkim/app.path'],'Work:Apps/BitSkim')
 def test_partial_records_stop_without_offering_a_fresh_install(self):
  f=Flow({'ENV:BitSkim/app.path':'Work:Apps/BitSkim'})
  with self.assertRaisesRegex(Stop,'Incomplete'):f.run(self.flow)
  self.assertEqual(f.directories,[]);self.assertEqual(f.choices,[])
 def test_wrong_executable_in_records_is_rejected(self):
  files=self.recorded();files['ENV:BitSkim/app.path']='Work:Apps/OtherApp'
  with self.assertRaisesRegex(Stop,'Invalid'):Flow(files).run(self.flow)
 def test_fresh_install_has_three_directory_choices_and_no_uninstall(self):
  f=Flow({},expert=True);f.run(self.flow)
  self.assertEqual(f.directories,['SYS:Utilities','SYS:Prefs','SYS:'])
  self.assertEqual(f.vars['#dest'],'SYS:Utilities');self.assertEqual(f.vars['#prefdest'],'SYS:Prefs');self.assertEqual(f.vars['#shared'],'SYS:BitSkim')
  self.assertEqual(f.choices,[]);self.assertEqual(f.vars['#installed'],0);self.assertEqual(f.vars['#action'],0)
 def test_legacy_combined_layout_only_offers_management(self):
  f=Flow({'SYS:Utilities/BitSkim':None,'SYS:Utilities/BitSkim/BitSkim':'binary','SYS:Utilities/BitSkim/BitSkimPrefs':'binary'},expert=True)
  f.run(self.flow)
  self.assertEqual(f.directories,[])
  self.assertEqual(f.vars['#dest'],'SYS:Utilities/BitSkim');self.assertEqual(f.vars['#shared'],'SYS:Utilities/BitSkim')
  self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
 def test_novice_fresh_install_uses_defaults_without_path_questions(self):
  f=Flow({},novice=True);f.run(self.flow)
  self.assertEqual(f.directories,[]);self.assertEqual(f.vars['#dest'],'SYS:Utilities')
  self.assertEqual(f.vars['#prefdest'],'SYS:Prefs');self.assertEqual(f.vars['#shared'],'SYS:BitSkim')
  self.assertEqual(f.vars['#novice'],True);self.assertEqual(f.vars['#action'],0)
 def test_novice_update_reuses_registered_custom_locations(self):
  f=Flow(self.recorded(),novice=True);f.run(self.flow)
  self.assertEqual(f.directories,[]);self.assertEqual(f.vars['#dest'],'Work:Apps')
  self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
 def test_novice_refuses_to_take_over_unowned_startup_entry(self):
  with self.assertRaisesRegex(Stop,'unowned'):
   Flow({'SYS:WBStartup/BitSkimStart':'foreign'},novice=True).run(self.flow)
 def test_owned_startup_is_retained_for_updates(self):
  files=self.recorded();files['ENVARC:BitSkim/startup-owned']='BitSkimStart 0.2';files['SYS:WBStartup/BitSkimStart']='owned'
  f=Flow(files);f.run(self.flow);self.assertTrue(f.vars['#startup-owned'])
 def test_intermediate_only_asks_two_paths_and_both_start_questions(self):
  f=Flow({},boot=False,launch=True);f.run(self.flow)
  self.assertEqual(f.directories,['SYS:Utilities','SYS:Prefs'])
  self.assertEqual(f.vars['#shared'],'SYS:BitSkim')
  self.assertEqual(f.questions[-2:],['Start BitSkim at boot?','Start BitSkim after installation?'])
  self.assertFalse(f.vars['#install-boot']);self.assertTrue(f.vars['#launch-now'])
 def test_expert_can_enable_boot_without_launching_now(self):
  f=Flow({},expert=True,boot=True,launch=False);f.run(self.flow)
  self.assertEqual(len(f.directories),3);self.assertTrue(f.vars['#install-boot']);self.assertFalse(f.vars['#launch-now'])
 def test_uninstall_never_asks_directories_or_startup_questions(self):
  for expert in (False,True):
   for files in (self.recorded(),{'SYS:Utilities/BitSkim':'binary'},
                 {'SYS:Utilities/BitSkim':None,'SYS:Utilities/BitSkim/BitSkim':'binary'}):
    f=Flow(files,expert=expert,action=1);f.run(self.flow)
    self.assertEqual(f.directories,[]);self.assertEqual(f.questions,[])
    self.assertEqual(f.vars['#action'],1);self.assertEqual(f.vars['#install-boot'],0);self.assertEqual(f.vars['#launch-now'],0)
 def test_novice_can_explicitly_choose_uninstall_and_returns_to_novice(self):
  f=Flow(self.recorded(),novice=True,action=1);f.run(self.flow)
  self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
  self.assertEqual(f.vars['#action'],1);self.assertEqual(f.vars['@user-level'],0)
  self.assertEqual(f.directories,[]);self.assertEqual(f.questions,[])
  f.run(self.confirmation);self.assertTrue(f.vars['#remove-approved']);self.assertEqual(f.vars['@user-level'],0)
  self.assertEqual(f.questions,[])
  self.assertTrue(any('Proceed to remove' in text for text in f.messages))
 def test_leftover_drawer_is_preserved_before_installing_direct_executable(self):
  f=Flow({'SYS:Utilities/BitSkim':None,'SYS:Utilities/BitSkim/unknown.txt':'keep me','SYS:Utilities/BitSkim.info':'icon'},novice=True)
  f.run(self.flow);self.assertTrue(f.vars['#blocking-drawer'])
  f.run(self.preserve)
  self.assertNotIn('SYS:Utilities/BitSkim',f.files)
  self.assertEqual(f.files['SYS:Utilities/BitSkim.previous/unknown.txt'],'keep me')
  self.assertEqual(f.files['SYS:Utilities/BitSkim.previous.info'],'icon')
 def test_existing_backup_prevents_renaming_or_overwriting_anything(self):
  files={'SYS:Utilities/BitSkim':None,'SYS:Utilities/BitSkim/unknown.txt':'keep','SYS:Utilities/BitSkim.previous':None}
  f=Flow(files,novice=True);f.run(self.flow)
  with self.assertRaisesRegex(Stop,'already exists'):f.run(self.preserve)
  self.assertEqual(f.files,files)
 def test_uninstall_is_not_blocked_by_installation_path_collisions(self):
  files=self.recorded();files['ENV:BitSkim/shared.path']='Work:Prefs/BitSkimPrefs'
  f=Flow(files,novice=True,action=1);f.run(self.flow)
  self.assertEqual(f.vars['#action'],1);self.assertEqual(f.directories,[])
  self.assertEqual(f.choices,[['Update BitSkim','Uninstall BitSkim']])
 def test_removed_legacy_app_with_installer_remnants_can_be_uninstalled(self):
  files={'SYS:Utilities/BitSkim':None,'SYS:Utilities/BitSkim/Install-BitSkim':'old installer'}
  f=Flow(files,novice=True,action=1);f.run(self.flow)
  self.assertEqual(f.vars['#installed'],1);self.assertEqual(f.vars['#action'],1)
  self.assertEqual(f.vars['#dest'],'SYS:Utilities/BitSkim');self.assertEqual(f.directories,[])
 def test_nonexistent_paths_do_not_cause_false_install_collisions(self):
  class MissingPaths(Flow):
   def ev(self,x):
    if isinstance(x,list) and x and x[0]=='expandpath':return ''
    return super().ev(x)
  f=MissingPaths({});f.vars.update({'#dest':'SYS:Utilities','#prefdest':'SYS:Prefs','#shared':'SYS:BitSkim'})
  f.run(self.pathchecks)
 def test_recorded_source_drawer_is_rejected(self):
  files=self.recorded();files['ENV:BitSkim/app.path']='RAM:Package/BitSkim'
  with self.assertRaisesRegex(Stop,'source drawer'):Flow(files).run(self.flow)

class UpdateLifecycle(unittest.TestCase):
 setUpClass=InstallerDiscovery.__dict__["setUpClass"]
 recorded=InstallerDiscovery.recorded
 def test_updates_always_restart_at_every_level(self):
  for novice,expert in [(True,False),(False,False),(False,True)]:
   f=Flow(self.recorded(),novice=novice,expert=expert,launch=False);f.run(self.flow)
   self.assertEqual(f.vars['#launch-now'],1)
   self.assertNotIn('Start BitSkim after installation?',f.questions)
 def test_stop_success_and_timeout(self):
  for result in (0,20):
   f=Flow({'RAM:Package/StopBitSkim':'helper'});f.vars['#installed']=1;f.run_result=result
   if result:
    with self.assertRaisesRegex(Stop,'did not exit'):f.run(self.stopping)
   else:f.run(self.stopping)
   self.assertNotIn('T:BitSkim-InstallStop',f.files)
 def test_shutdown_precedes_installed_file_changes(self):
  text=(ROOT/'build/amiga/Install-BitSkim').read_text()
  self.assertLess(text.index('; END stop running clients'),text.index('; BEGIN preserve blocking drawer'))
  self.assertLess(text.index('; END stop running clients'),text.index('(source (tackon #src "BitSkim"))'))
