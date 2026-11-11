"""Exercise actual native control cleanup with independently retained pages."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ControlLifetime(unittest.TestCase):
 def test_free_detached_page_preserves_other_page_and_native_gadgets(self):
  source=(ROOT/'clients/amiga/controls.h').read_text()
  find=source[source.index('static UIControl *ui_find('):source.index('static void ui_render_control(')]
  cleanup=source[source.index('static void ui_controls_free('):source.index('static void ui_string_colors(')]
  harness=r'''
#include <stdlib.h>
#include <assert.h>
struct Gadget { struct Gadget *NextGadget; };
typedef struct UIControl {struct UIControl *next;struct Gadget gadget;void *pixels;unsigned long image_bytes;} UIControl;
static UIControl *ui_controls;
static int released;
static void WaitBlit(void) {}
static void FreeMem(void *p,unsigned long n) {(void)n;released++;free(p);}
'''+find+cleanup+r'''
static UIControl *control(void) {
 UIControl *c=calloc(1,sizeof *c);assert(c);c->pixels=malloc(2);assert(c->pixels);c->image_bytes=1;
 c->next=ui_controls;ui_controls=c;return c;
}
int main(void) {
 int i;
 for(i=0;i<1000;i++) {
  struct Gadget context={0},native={0},other_context={0};
  struct Gadget *page=&context,*other=&other_context;
  UIControl *a=control(),*b=control(),*c=control();
  context.NextGadget=&a->gadget;a->gadget.NextGadget=&native;native.NextGadget=&b->gadget;
  other_context.NextGadget=&c->gadget;
  ui_controls_free(&page);
  assert(page==&context && context.NextGadget==&native && native.NextGadget==NULL);
  assert(ui_controls==c && c->next==NULL && other_context.NextGadget==&c->gadget);
  ui_controls_free(&page);assert(ui_controls==c);
  ui_controls_free(&other);assert(!ui_controls && !other_context.NextGadget);
 }
 assert(released==6000);return 0;
}
'''
  with tempfile.TemporaryDirectory() as d:
   p=Path(d);(p/'test.c').write_text(harness)
   subprocess.run(['cc','-std=c89','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
   subprocess.run([str(p/'test')],check=True)
