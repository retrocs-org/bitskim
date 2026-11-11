"""Portable checks of the actual Amiga launcher and generated installer manifest."""
from pathlib import Path
import ctypes,re,struct,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class AmigaInstaller(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  subprocess.run(['python3','scripts/build-amiga-guide.py'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['python3','scripts/build-amiga-installer.py'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
  cls.tmp=tempfile.TemporaryDirectory();p=Path(cls.tmp.name)
  (p/'test.c').write_text('#define BS_INSTALLER_TEST\n#include "'+str(ROOT/'installers/amiga/start-installer.c')+'"\nint quote_path(char *out,int n,const char *s) {return quoted(out,n,s); }\nint build_command(char *out,int n,const char *tool,const char *file) {return installer_command(out,n,tool,file);}\n')
  subprocess.run(['cc','-shared','-fPIC','-std=c89','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test.so')],check=True)
  cls.lib=ctypes.CDLL(str(p/'test.so'));cls.lib.quote_path.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_launcher_quotes_paths_without_shell_injection(self):
  out=ctypes.create_string_buffer(128)
  self.assertEqual(self.lib.quote_path(out,128,b'Work:My "Apps"/Bit*Skim'),1)
  self.assertEqual(out.value,b'"Work:My *"Apps*"/Bit**Skim"')
  self.assertEqual(self.lib.quote_path(out,128,b'RAM:bad\nEcho hacked'),0)
  self.assertEqual(self.lib.quote_path(out,4,b'long'),0)
 def test_installer_command_names_script_and_rejects_drawer(self):
  out=ctypes.create_string_buffer(2200)
  self.lib.build_command.argtypes=[ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_char_p]
  self.assertEqual(self.lib.build_command(out,len(out),b'SYS:System/Installer',b'Bin:BitSkim/Install-BitSkim'),1)
  self.assertTrue(out.value.startswith(b'"SYS:System/Installer" "Bin:BitSkim/Install-BitSkim" '))
  self.assertNotIn(b'\n',out.value)
  self.assertEqual(self.lib.build_command(out,len(out),b'C:Installer',b'Bin:BitSkim/'),0)
  self.assertEqual(self.lib.build_command(out,10,b'C:Installer',b'Bin:BitSkim/Install-BitSkim'),0)
 def test_script_owned_paths_and_balanced_forms(self):
  text=(ROOT/'build/amiga/Install-BitSkim').read_text();depth=0;quoted=False;escape=False
  for line in text.splitlines():
   if line.lstrip().startswith(';'):continue
   for c in line:
    if quoted:
     if escape:escape=False
     elif c=='\\':escape=True
     elif c=='"':quoted=False
    elif c=='"':quoted=True
    elif c=='(':depth+=1
    elif c==')':depth-=1;self.assertGreaterEqual(depth,0)
   self.assertFalse(quoted,"Installer strings must end on the same physical line")
  self.assertEqual(depth,0);self.assertFalse(quoted)
  copied=set(re.findall(r'\(source \(tackon #src "([^"]+)"\)\)',text))
  deleted=set(re.findall(r'\(delete \(tackon #[a-z]+ "([^"]+)"\)',text))
  copied={name for name in copied if not name.startswith("InstallAssets/") and name not in ("StartBitSkim","StopBitSkim")}
  self.assertTrue(copied<=deleted)
  self.assertEqual(deleted-copied,{'BitSkim.prefs-path','BitSkimPrefs.guide-path','Install-BitSkim','Install-BitSkim.info','StartInstaller'})
  self.assertTrue({'BitSkim','BitSkimPrefs','BitSkim.guide'}<=copied)
  self.assertNotRegex(text,r'\(delete "(?:ENV|ENVARC):BitSkim\.prefs"')
  self.assertNotIn(' ALL ',text)
  self.assertNotIn('"Cancel"',text)
  self.assertIn('Abort Install to stop',text)
  self.assertNotIn('\\\\n',text)
 def test_three_destinations_and_package_only_installer(self):
  text=(ROOT/'build/amiga/Install-BitSkim').read_text()
  self.assertEqual(text.count('(askdir'),3)
  self.assertIn('(set #appdefault "SYS:Utilities")',text)
  self.assertIn('(set #prefsdefault "SYS:Prefs")',text)
  self.assertRegex(text,r'\(source \(tackon #src "BitSkimPrefs"\)\) \(dest #prefdest\)')
  self.assertRegex(text,r'\(source \(tackon #src "BitSkim"\)\) \(dest #dest\)')
  for name in ('Install-BitSkim','Install-BitSkim.info','StartInstaller'):
   self.assertNotIn('(source (tackon #src "'+name+'"))',text)
  self.assertIn('(set #shareddefault "SYS:")',text)
  self.assertIn('(set #shared (tackon #sharedparent "BitSkim"))',text)
  self.assertNotIn('(tackon #dest "BitSkim") (infos)',text)
  self.assertIn('(set #guide "HELP:")',text)
  self.assertRegex(text,r'\(source \(tackon #src "BitSkim.guide"\)\) \(dest #guide\)')
  self.assertRegex(text,r'\(source \(tackon #src "README.MD"\)\) \(dest #shared\)')
  self.assertNotIn('(dest #dest) (newname',text)
  for base in ('ENV:BitSkim','ENVARC:BitSkim'):
   for leaf in ('app.path','prefs.path','shared.path','guide.path'):
    self.assertIn('(textfile (dest "'+base+'/'+leaf+'")',text)
  self.assertIn('(append (tackon #guide "BitSkim.guide"))',text)
  for leaf in ('Plugins','Images','Tools','Themes'):
   self.assertIn('(makedir (tackon #shared "'+leaf+'"))',text)
   self.assertNotIn('(delete (tackon #shared "'+leaf+'")',text)
  self.assertLess(text.index('A destination cannot'),text.index('(makedir #dest'))
 def test_fixed_settings_help_icons_and_hidden_shared_drawer(self):
  text=(ROOT/'build/amiga/Install-BitSkim').read_text()
  self.assertNotIn('Installing shared drawer icon',text)
  self.assertIn('(delete (cat #shared ".info")',text)
  self.assertIn('(setdefaulttool (tackon #prefdest "BitSkimPrefs"))',text)
  self.assertIn('ENVARC:BitSkim/BitSkim.prefs.info',text)
  # Path records are replaced rather than appended on repeated updates.
  self.assertLess(text.index('(delete "ENVARC:BitSkim/app.path"'),text.index('(textfile (dest "ENVARC:BitSkim/app.path")'))
 def test_novice_startup_is_owned_and_uses_a_fixed_safe_launch_command(self):
  text=(ROOT/'build/amiga/Install-BitSkim').read_text()
  self.assertIn('(set #novice (= @user-level 0))',text)
  self.assertIn('(run "T:BitSkim-InstallLaunch")',text)
  self.assertIn('(newname "BitSkimStart")',text)
  self.assertIn('(settooltype "DONOTWAIT" "YES")',text)
  self.assertIn('(delete "SYS:WBStartup/BitSkimStart"',text)
  self.assertIn('(if #startup-owned',text)
 def test_distribution_drawer_explicitly_uses_icons_only_view(self):
  data=(ROOT/'assets/icons/BitSkimDrawer.info').read_bytes()
  self.assertEqual(struct.unpack_from('>I',data,44)[0]&255,1)
  self.assertEqual(struct.unpack_from('>IH',data,len(data)-6),(1,1))
 def test_installation_templates_have_native_icon_types(self):
  for name,kind in [('SettingsDrawer.info',2),('BitSkim.prefs.info',4)]:
   data=(ROOT/'build/amiga/InstallAssets'/name).read_bytes()
   self.assertEqual(struct.unpack_from('>HH',data),(0xe310,1))
   self.assertEqual(data[48],kind)
   if kind==4:self.assertTrue(data.endswith(b'SYS:Prefs/BitSkimPrefs\0'))
 def test_supplied_icons_keep_images_and_workbench_metadata(self):
  data=(ROOT/'build/amiga/Install-BitSkim.info').read_bytes()
  self.assertEqual(struct.unpack_from('>H',data)[0],0xe310)
  self.assertEqual(data[48],4)
  self.assertEqual(struct.unpack_from('>I',data,50)[0],1)
  self.assertTrue(data.endswith(b'StartInstaller\0'))
  import importlib.util
  spec=importlib.util.spec_from_file_location('package_icons',ROOT/'scripts/amiga_package_icons.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  template=(ROOT/'assets/icons/generic-amiga-installer/Install.info').read_bytes()
  self.assertEqual(module.images(template)[0],module.images(data)[0])
  drawer=(ROOT/'assets/icons/BitSkimDrawer.info').read_bytes()
  self.assertEqual(drawer[48],2);self.assertNotEqual(struct.unpack_from('>I',drawer,66)[0],0)
  template=(ROOT/'assets/icons/generic-amiga-installer/SoftwarePackage.info').read_bytes()
  self.assertEqual(module.images(template)[0],module.images(drawer)[0])


