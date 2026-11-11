#include "gem.h"
#include <string.h>
short appl_init(void){return 1;}void appl_exit(void){}
short graf_handle(short*a,short*b,short*c,short*d){*a=*b=*c=*d=8;return 1;}
void v_opnvwk(short*i,short*h,short*o){(void)i;*h=1;memset(o,0,57*sizeof(short));o[13]=2;}void v_clsvwk(short h){(void)h;}
void vst_height(short h,short size,short*a,short*b,short*c,short*d){(void)h;(void)size;*a=*c=6;*b=*d=8;}
void wind_get(short w,short mode,short*x,short*y,short*a,short*b){(void)w;*x=*y=0;*a=mode==WF_NEXTXYWH?0:640;*b=mode==WF_NEXTXYWH?0:200;}
void wind_set(short w,short m,...){(void)w;(void)m;}void wind_set_str(short w,short m,const char*s){(void)w;(void)m;(void)s;}
short wind_create(short f,short x,short y,short w,short h){(void)f;(void)x;(void)y;(void)w;(void)h;return 1;}
void wind_open(short f,short x,short y,short w,short h){(void)f;(void)x;(void)y;(void)w;(void)h;}void wind_close(short w){(void)w;}void wind_delete(short w){(void)w;}
void graf_mouse(short n,void*p){(void)n;(void)p;}short form_alert(short n,const char*s){(void)n;(void)s;return 1;}
void vst_color(short h,short c){(void)h;(void)c;}void vsf_color(short h,short c){(void)h;(void)c;}void vsf_interior(short h,short c){(void)h;(void)c;}
void v_bar(short h,short*p){(void)h;(void)p;}void v_gtext(short h,short x,short y,const char*s){(void)h;(void)x;(void)y;(void)s;}void vs_clip(short h,short f,short*p){(void)h;(void)f;(void)p;}
void wind_update(short m){(void)m;}void fsel_input(char*p,char*f,short*b){(void)p;(void)f;*b=0;}
short evnt_multi(short t,short c,short b,short s,short a,short d,short e,short f,short g,short h,short i,short j,short k,short l,short*m,unsigned long timer,short*x,short*y,short*buttons,short*meta,short*key,short*clicks){(void)t;(void)c;(void)b;(void)s;(void)a;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;(void)j;(void)k;(void)l;(void)timer;*x=*y=*buttons=*meta=*key=*clicks=0;m[0]=AP_TERM;return MU_MESAG;}
void vq_color(short handle,short index,short flag,short*out){(void)handle;(void)flag;out[0]=out[1]=out[2]=index?0:1000;}
void vswr_mode(short handle,short mode){(void)handle;(void)mode;}
void vst_alignment(short h,short x,short y,short *a,short *b){(void)h;*a=x;*b=y;}
short objc_draw(OBJECT *tree,short object,short depth,short x,short y,short w,short h)
{(void)tree;(void)object;(void)depth;(void)x;(void)y;(void)w;(void)h;return 1;}
