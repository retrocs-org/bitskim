"""Exercise actual tray code with shell failures; no native GUI claims."""
import ctypes,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class TrayFallback(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.tmp=tempfile.TemporaryDirectory();p=Path(cls.tmp.name)
  source=r'''
#include <stdint.h>
#include <string.h>
typedef uintptr_t HWND; typedef uintptr_t HMENU; typedef uintptr_t HICON;
typedef unsigned int UINT;
typedef struct { int x,y; } POINT;
typedef struct { unsigned long cbSize; HWND hWnd; UINT uID,uFlags,uCallbackMessage; HICON hIcon; char szTip[128]; } NOTIFYICONDATAA;
#define NOTIFYICONDATAA_V1_SIZE 88
#define WM_APP 0x8000
#define WM_COMMAND 0x111
#define WM_NULL 0
#define NIF_MESSAGE 1
#define NIF_ICON 2
#define NIF_TIP 4
#define NIM_ADD 0
#define SW_RESTORE 9
#define SW_HIDE 0
#define MF_STRING 0
#define MF_SEPARATOR 1
#define TPM_RETURNCMD 256
#define TPM_RIGHTBUTTON 2
#define MB_OK 0
#define MAKEINTRESOURCEA(x) ((char *)(uintptr_t)(x))
static HWND window=1,prefs; static uintptr_t instance=3;
static int succeed,hidden,restored,warnings,adds; static unsigned long captured_size;
static HICON LoadIconA(uintptr_t i,const char *name) { (void)i;(void)name;return 4; }
static char *lstrcpynA(char *d,const char *s,int n) { strncpy(d,s,n);d[n-1]=0;return d; }
static int Shell_NotifyIconA(int op,NOTIFYICONDATAA *data) { (void)op;adds++;captured_size=data->cbSize;return succeed; }
static void ShowWindow(HWND w,int state) { (void)w;if(state==SW_HIDE) hidden++;else restored++; }
static void SetForegroundWindow(HWND w) { (void)w; }
static void MessageBoxA(HWND w,const char *s,const char *t,int f) { (void)w;(void)s;(void)t;(void)f;warnings++; }
static HMENU CreatePopupMenu(void) { return 1; }
static void AppendMenuA(HMENU m,int f,int id,const char *s) { (void)m;(void)f;(void)id;(void)s; }
static void GetCursorPos(POINT *p) { p->x=p->y=0; }
static UINT TrackPopupMenu(HMENU m,int f,int x,int y,int z,HWND w,void *r) { (void)m;(void)f;(void)x;(void)y;(void)z;(void)w;(void)r;return 0; }
static void DestroyMenu(HMENU m) { (void)m; }
static void PostMessageA(HWND w,int m,UINT p,int l) { (void)w;(void)m;(void)p;(void)l; }
#include "HEADER"
int run_hide(int shell_ok,int existing) { succeed=shell_ok;tray_present=existing;hidden=restored=warnings=adds=0; hide_main(); return hidden+10*restored+100*warnings+1000*adds; }
unsigned long notification_size(void) { return captured_size; }
'''.replace('HEADER',str(ROOT/'clients/windows-legacy/tray.h'))
  (p/'tray.c').write_text(source)
  subprocess.run(['cc','-shared','-fPIC',str(p/'tray.c'),'-o',str(p/'tray.so')],check=True)
  cls.lib=ctypes.CDLL(str(p/'tray.so'));cls.lib.run_hide.argtypes=[ctypes.c_int,ctypes.c_int]
 @classmethod
 def tearDownClass(cls):cls.tmp.cleanup()
 def test_failed_tray_keeps_window_accessible(self): self.assertEqual(self.lib.run_hide(0,0),1110)
 def test_hide_requires_registered_icon_and_v1_size(self):
  self.assertEqual(self.lib.run_hide(1,0),1001)
  self.assertEqual(self.lib.notification_size(),88)
 def test_existing_icon_does_not_duplicate(self): self.assertEqual(self.lib.run_hide(1,1),1)
