"""Exercise the actual Amiga preferences reader's default-theme migration."""
import ctypes
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AmigaThemePreferences(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(); folder=Path(cls.temp.name)
        source=(ROOT/'clients/amiga/main.c').read_text()
        reader=source[source.index('static void trim('):source.index('static void save_prefs(')]
        limits=(ROOT/'core/rate.h').read_text();parser=limits[limits.index('int bs_limit_parse('):limits.index('int bs_limits_configure(')]
        harness=r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "themes.h"
#include "bitskim.h"
typedef unsigned long ULONG;
static char client_name[64],installation[33],download[512];
static char archive_prefs[1024]="ENVARC:BitSkim.prefs";
static char live_prefs[1024]="ENV:BitSkim.prefs";
static int theme_index=BS_THEME_DEFAULT,visible,receiving,auto_accept,ask_location,start_iconified,close_to_iconify,completion_sound;
static unsigned long send_limit,receive_limit;
static const char *testpath;
static int installed_paths;
static int amiga_install_path(const char *leaf,char *out,size_t n) {(void)leaf;(void)out;(void)n;return 0;}
static int AddPart(char *out,const char *leaf,size_t n) {(void)out;(void)leaf;(void)n;return 0;}
static void *FindTask(void *unused) {return NULL;}
static void new_installation_id(char *out) {memset(out,'0',32);out[32]=0;}
static void copy(char *d,const char *s,size_t n) {strncpy(d,s,n-1);d[n-1]=0;}
static FILE *test_open(const char *path,const char *mode) {return fopen(installed_paths && strcmp(path,"ENV:BitSkim.prefs") && strcmp(path,"ENVARC:BitSkim.prefs")?path:testpath,mode);}
#define fopen test_open
'''+parser+reader+r'''
int read_installed(const char *archive,const char *live,const char *legacy) {int result;testpath=legacy;installed_paths=1;copy(archive_prefs,archive,sizeof archive_prefs);copy(live_prefs,live,sizeof live_prefs);theme_index=BS_THEME_DEFAULT;load_prefs();result=theme_index;installed_paths=0;copy(archive_prefs,"ENVARC:BitSkim.prefs",sizeof archive_prefs);copy(live_prefs,"ENV:BitSkim.prefs",sizeof live_prefs);return result;}
int read_theme(const char *path) {testpath=path;theme_index=BS_THEME_DEFAULT;load_prefs();return theme_index;}
int roundtrip_close(const char *path,int value) {testpath=path;close_to_iconify=value;if(!write_prefs(path))return -1;close_to_iconify=0;load_prefs();return close_to_iconify;}
int read_close(const char *path) {testpath=path;close_to_iconify=0;load_prefs();return close_to_iconify;}
int read_start(const char *path) {testpath=path;start_iconified=0;load_prefs();return start_iconified;}
int reload_iconify(const char *path) {testpath=path;start_iconified=close_to_iconify=1;load_prefs();return 2*start_iconified+close_to_iconify;}
'''
        (folder/'reader.c').write_text(harness)
        subprocess.run(['cc','-shared','-fPIC','-I'+str(ROOT/'clients/amiga'),'-I'+str(ROOT/'core'),str(folder/'reader.c'),'-o',str(folder/'reader.so')],check=True)
        cls.lib=ctypes.CDLL(str(folder/'reader.so'));cls.lib.read_installed.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_char_p];cls.lib.read_theme.argtypes=[ctypes.c_char_p];cls.lib.read_start.argtypes=[ctypes.c_char_p];cls.lib.reload_iconify.argtypes=[ctypes.c_char_p];cls.lib.read_close.argtypes=[ctypes.c_char_p];cls.lib.roundtrip_close.argtypes=[ctypes.c_char_p,ctypes.c_int]
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def read(self,content):
        path=Path(self.temp.name)/'prefs';path.write_text(content)
        return self.lib.read_theme(str(path).encode())
    def test_shared_settings_win_over_legacy_after_reboot_and_live_settings_win_in_session(self):
        folder=Path(self.temp.name);archive=folder/'shared-config';live=folder/'installed-env';legacy=folder/'legacy-env'
        archive.write_text('theme=mint\ntheme_defaults=1\n');legacy.write_text('theme=classic\ntheme_defaults=1\n')
        args=[str(p).encode() for p in (archive,live,legacy)]
        self.assertEqual(self.lib.read_installed(*args),5)
        live.write_text('theme=latte\ntheme_defaults=1\n')
        self.assertEqual(self.lib.read_installed(*args),0)
        live.unlink();archive.unlink()
        self.assertEqual(self.lib.read_installed(*args),7)
    def test_fresh_and_legacy_default_use_mocha(self):
        self.assertEqual(self.read(''),3)
        self.assertEqual(self.read('theme=classic\n'),3)
    def test_explicit_classic_and_other_choices_are_preserved(self):
        self.assertEqual(self.read('theme=classic\ntheme_defaults=1\n'),7)
        self.assertEqual(self.read('theme=mint\n'),5)
        self.assertEqual(self.read('theme=latte\ntheme_defaults=1\n'),0)
    def test_start_iconified_is_read_without_changing_the_default(self):
        path=Path(self.temp.name)/'startup-prefs'
        for content,expected in [('start_iconified=1\n',1),('start_iconified=0\n',0),('',0)]:
            path.write_text(content)
            self.assertEqual(self.lib.read_start(str(path).encode()),expected)
    def test_close_to_iconify_defaults_off_and_roundtrips(self):
        path=Path(self.temp.name)/'close-prefs'
        for content,expected in [('',0),('close_to_iconify=0\n',0),('close_to_iconify=1\n',1)]:
            path.write_text(content)
            self.assertEqual(self.lib.read_close(str(path).encode()),expected)
        for value in (1,0):
            self.assertEqual(self.lib.roundtrip_close(str(path).encode(),value),value)
            self.assertIn('close_to_iconify='+str(value),path.read_text())
    def test_missing_iconify_keys_reset_off_even_after_enabled_live_settings(self):
        path=Path(self.temp.name)/'reload-iconify'
        for content,expected in [('',0),('start_iconified=1\n',2),('close_to_iconify=1\n',1),('start_iconified=1\nclose_to_iconify=1\n',3)]:
            path.write_text(content)
            self.assertEqual(self.lib.reload_iconify(str(path).encode()),expected)
        self.assertEqual(self.lib.reload_iconify(b'/no-such-bitskim-settings'),0)
    def test_missing_or_invalid_theme_uses_mocha(self):
        self.assertEqual(self.read('theme=missing\n'),3)
        self.assertEqual(self.lib.read_theme(b'/no-such-bitskim-prefs'),3)
if __name__=='__main__':unittest.main()
