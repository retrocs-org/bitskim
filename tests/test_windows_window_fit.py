"""Exercise production work-area placement across DPI sizes and monitor origins."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class WindowFit(unittest.TestCase):
 def test_scaled_window_stays_inside_monitor_work_area(self):
  code=r'''
#include <assert.h>
typedef struct {long left,top,right,bottom;} RECT;
#include "window-fit.h"
int main(void){
    RECT areas[]={{0,0,1788,1240},{0,0,1280,720},{-1920,-100,0,980},{200,80,740,560}};
    int i,scale;
    for(i=0;i<4;i++)for(scale=1;scale<=3;scale++){
        RECT r=bs_window_fit(areas[i],720*scale,660*scale);
        assert(r.left>=areas[i].left && r.top>=areas[i].top);
        assert(r.right<=areas[i].right && r.bottom<=areas[i].bottom);
        assert(r.right>r.left && r.bottom>r.top);
        if(720*scale<=areas[i].right-areas[i].left)assert(r.right-r.left==720*scale);
        if(660*scale<=areas[i].bottom-areas[i].top)assert(r.bottom-r.top==660*scale);
    }
    return 0;
}'''
  with tempfile.TemporaryDirectory() as folder:
   source=Path(folder)/'fit.c';binary=Path(folder)/'fit';source.write_text(code)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-I'+str(ROOT/'clients/windows'),str(source),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
 def test_tools_controls_fit_scaled_client_and_report_does_not_cover_buttons(self):
  text=(ROOT/'clients/shared/tools-win.h').read_text()
  function=text[text.index('static void bs_tools_layout('):text.index('static LRESULT CALLBACK bs_tools_proc')]
  code=r'''
#include <assert.h>
typedef int HWND;
typedef struct {int left,top,right,bottom;} RECT;
#define TRUE 1
static int bs_tools_dpi=96,bs_tools_report=5,bs_tools_address=12;
static RECT area,controls[13];
static int MulDiv(int a,int b,int c){return a*b/c;}
static int bs_tools_scale(int n){return MulDiv(n,bs_tools_dpi,96);}
static void GetClientRect(HWND w,RECT *out){(void)w;*out=area;}
static HWND GetDlgItem(HWND w,int id){(void)w;return id;}
static void MoveWindow(HWND id,int x,int y,int width,int height,int repaint){
 (void)repaint;assert(id>0 && id<13);assert(x>=0 && y>=0 && width>0 && height>0);
 assert(x+width<=area.right && y+height<=area.bottom);
 controls[id].left=x;controls[id].top=y;controls[id].right=x+width;controls[id].bottom=y+height;
}
'''+function+r'''
int main(void){int dpi,height;for(dpi=96;dpi<=288;dpi+=48){for(height=450;height<=640;height+=95){
 bs_tools_dpi=dpi;area.left=area.top=0;area.right=bs_tools_scale(592);area.bottom=bs_tools_scale(height);
 bs_tools_layout(1);assert(controls[5].bottom<controls[6].top);assert(controls[6].bottom<controls[11].top);
}}
return 0;
}
'''
  with tempfile.TemporaryDirectory() as folder:
   source=Path(folder)/'tools.c';binary=Path(folder)/'tools';source.write_text(code)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror',str(source),'-o',str(binary)],check=True)
   subprocess.run([str(binary)],check=True)
