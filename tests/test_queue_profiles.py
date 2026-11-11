"""Compiled queue capacity is a local profile; wire manifest bounds stay fixed."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class QueueProfiles(unittest.TestCase):
 def test_platform_capacity_contract(self):
  profiles=[(flags,32760) for flags in [[],['BS_WINDOWS_LEGACY'],['__amigaos__'],['BS_DOS'],['__riscos__'],['BS_ATARI','BS_ATARI_STING'],['BS_ATARI']]]
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'profile.c').write_text('#include "bitskim.h"\n#include <stdio.h>\nint main(void){printf("%d %d",BS_QUEUE_MAX,BS_BATCH_MAX);return 0;}\n')
   for flags,expected in profiles:
    with self.subTest(flags=flags):
     subprocess.run(['cc','-std=c89','-I'+str(ROOT/'core'),*['-D'+flag for flag in flags],str(p/'profile.c'),'-o',str(p/'profile')],check=True)
     self.assertEqual(subprocess.check_output([str(p/'profile')],text=True),str(expected)+' 8')
