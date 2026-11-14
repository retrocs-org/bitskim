"""Keep persistent device identity setup on the normal Windows startup path."""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class WindowsTrustStartup(unittest.TestCase):
 def test_identity_is_configured_before_security_and_message_loop(self):
  source=(ROOT/'clients/windows/main.c').read_text()
  startup=source[source.index('int WINAPI wWinMain('):]
  normal=startup[startup.index('load_preferences();'):]
  self.assertLess(normal.index('bs_trust_configure(client,path)'),normal.index('bs_security_configure(client,prefs.security,key)'))
  self.assertLess(normal.index('bs_trust_configure(client,path)'),normal.index('CreateWindowExW('))
  self.assertIn('GetLastError()!=ERROR_ALREADY_EXISTS',normal)
  preferences=source[source.index('else if(id==PREF_SAVE)'):source.index('int WINAPI wWinMain(')]
  self.assertNotIn('bs_trust_configure(',preferences)
