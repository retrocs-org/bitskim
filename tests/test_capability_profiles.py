"""Compile the actual platform policy function with bounded host-only stubs."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CapabilityProfiles(unittest.TestCase):
 def test_actual_local_policy_for_every_platform(self):
  code=(ROOT/'core/caps.h').read_text();fn=code[code.index('void bs_local_capabilities'):code.index('static void cap_encode')]
  profiles=[([],0,255,1023,0),(['__amigaos__'],1,30,1023,0),(['__riscos__'],1,10,1023,0),(['BS_DOS'],2,12,127,0),(['BS_ATARI','BS_ATARI_STING'],2,12,127,0),(['BS_ATARI'],3,255,1023,0),(['BS_WINDOWS_LEGACY'],4,255,259,1252)]
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder);source=p/'profile.c'
   source.write_text('#include "bitskim.h"\n#include <stdio.h>\n#include <string.h>\n#define GetACP() 1252\nstruct BSClient {BSConfig config;int batch_enabled,security_mode,pairing_set;};\nstatic unsigned local_features(const BSClient *c){(void)c;return 1884;}\n'+fn+'\nint main(void){BSClient c;BSCapabilities p;memset(&c,0,sizeof c);c.config.receiving=c.batch_enabled=1;bs_local_capabilities(&c,&p);printf("%u %u %u %u %lu %lu",p.name_policy,p.local_name_bytes,p.path_bytes,p.code_page,p.send_features,p.receive_features);return 0;}\n')
   for flags,policy,names,paths,page in profiles:
    with self.subTest(flags=flags):
     subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'core'),*['-D'+f for f in flags],str(source),'-o',str(p/'profile')],check=True)
     self.assertEqual(subprocess.check_output([str(p/'profile')],text=True),f'{policy} {names} {paths} {page} 1884 1884')
