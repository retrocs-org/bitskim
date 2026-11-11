/* Classic Intuition image gadgets with per-window theme colors.
 * Workbench's own pens and Intuition window chrome are never changed. */
#include <exec/memory.h>
#include <graphics/view.h>
#include <intuition/sghooks.h>
#define UI_ROW_KIND 1001
static UWORD control_pens[7];
static struct TextFont *control_font;
typedef struct UIControl {
    struct UIControl *next;
    struct Gadget gadget;
    struct Image image[2];
    UBYTE *pixels;
    ULONG image_bytes;
    UWORD pens[7];
    int kind,value,count,theme;
    STRPTR *choices;
    char label[128];
} UIControl;
static UIControl *ui_controls;
static void ui_palette_get(int theme,UWORD *pens,int *obtained) {
    const BSTheme *t=&bs_themes[theme]; ULONG rgb[7]; int i;
    rgb[0]=t->base; rgb[1]=t->surface; rgb[2]=t->text; rgb[3]=t->muted;
    rgb[4]=t->accent; rgb[5]=t->shine; rgb[6]=t->shadow;
    for(i=0;i<7;i++) {
        LONG p=ObtainBestPen(screen->ViewPort.ColorMap,
            ((rgb[i]>>16)&255)*0x01010101UL,((rgb[i]>>8)&255)*0x01010101UL,
            (rgb[i]&255)*0x01010101UL,OBP_Precision,PRECISION_IMAGE,TAG_DONE);
        pens[i]=p<0?(i==2 || i==3?1:0):(UWORD)p; obtained[i]=p>=0;
    }
}
static void ui_palette_release(UWORD *pens,int *obtained) {
    int i; for(i=0;i<7;i++) if(obtained[i]) {
        ReleasePen(screen->ViewPort.ColorMap,pens[i]); obtained[i]=0;
    }
}
static UIControl *ui_find(struct Gadget *g) {
    UIControl *c; for(c=ui_controls;c;c=c->next) if(&c->gadget==g) return c;
    return NULL;
}
static void ui_render_control(UIControl *c) {
    struct BitMap bitmap; struct RastPort rp; int state,i,w=c->gadget.Width,h=c->gadget.Height;
    int stride=((w+15)/16)*2; UWORD *p=c->pens;
    for(state=0;state<2;state++) {
        const char *label=c->kind==CYCLE_KIND && c->choices?(const char *)c->choices[c->value]:c->label;
        int accent=state || (c->kind==BUTTON_KIND && c->gadget.GadgetID==7);
        int x,length=(int)strlen(label),left=c->kind==CYCLE_KIND?26:4;
        InitBitMap(&bitmap,8,w,h);
        for(i=0;i<8;i++) bitmap.Planes[i]=c->pixels+state*c->image_bytes+i*stride*h;
        InitRastPort(&rp); rp.BitMap=&bitmap;
        SetFont(&rp,control_font?control_font:screen->RastPort.Font);
        SetRast(&rp,accent?p[4]:p[1]);
        if(c->kind!=UI_ROW_KIND) {
            SetAPen(&rp,state?p[6]:p[5]); Move(&rp,0,h-1); Draw(&rp,0,0); Draw(&rp,w-1,0);
            SetAPen(&rp,state?p[5]:p[6]); Move(&rp,w-1,0); Draw(&rp,w-1,h-1); Draw(&rp,0,h-1);
        }
        SetDrMd(&rp,JAM1); SetAPen(&rp,accent?(c->theme==BS_THEME_CLASSIC?p[5]:p[0]):p[2]);
        if(c->kind==CHECKBOX_KIND) {
            if(state) { Move(&rp,5,h/2); Draw(&rp,9,h-4); Draw(&rp,w-5,3); }
        } else {
            if(c->kind==CYCLE_KIND) {
                Move(&rp,8,h/2-3); Draw(&rp,13,h/2); Draw(&rp,8,h/2+3);
                Move(&rp,20,3); Draw(&rp,20,h-4);
            }
            while(length>0 && TextLength(&rp,(STRPTR)label,length)>w-left-5) length--;
            x=c->kind==UI_ROW_KIND?6:left+(w-left-TextLength(&rp,(STRPTR)label,length))/2;
            Move(&rp,x,(h-rp.Font->tf_YSize)/2+rp.Font->tf_Baseline);
            Text(&rp,(STRPTR)label,length);
        }
    }
    WaitBlit();
}
static struct Gadget *ui_control_add(struct Gadget *last,ULONG kind,int id,int x,int y,int w,int h,const char *label,ULONG value) {
    UIControl *c; int state;
    if(!last) return NULL;
    c=(UIControl *)AllocMem(sizeof *c,MEMF_PUBLIC|MEMF_CLEAR); if(!c) return NULL;
    c->image_bytes=(ULONG)(((w+15)/16)*2*h*8);
    c->pixels=(UBYTE *)AllocMem(c->image_bytes*2,MEMF_CHIP|MEMF_CLEAR);
    if(!c->pixels) { FreeMem(c,sizeof *c); return NULL; }
    c->kind=kind; c->theme=theme_index; memcpy(c->pens,control_pens,sizeof c->pens);
    if(label) copy(c->label,label,sizeof c->label);
    if(kind==CYCLE_KIND) {
        c->choices=(STRPTR *)value;
        if(c->choices) while(c->choices[c->count]) c->count++;
    }
    c->gadget.LeftEdge=x; c->gadget.TopEdge=y; c->gadget.Width=w; c->gadget.Height=h;
    c->gadget.Flags=GFLG_GADGIMAGE|GFLG_GADGHIMAGE;
    c->gadget.Activation=GACT_RELVERIFY; c->gadget.GadgetType=GTYP_BOOLGADGET; c->gadget.GadgetID=id;
    if(kind==CHECKBOX_KIND) {
        c->gadget.Activation|=GACT_TOGGLESELECT;
        if(value) c->gadget.Flags|=GFLG_SELECTED;
    }
    for(state=0;state<2;state++) {
        c->image[state].Width=w; c->image[state].Height=h; c->image[state].Depth=8;
        c->image[state].ImageData=(UWORD *)(c->pixels+state*c->image_bytes); c->image[state].PlanePick=255;
    }
    c->gadget.GadgetRender=&c->image[0]; c->gadget.SelectRender=&c->image[1];
    ui_render_control(c);
    c->next=ui_controls; ui_controls=c;
    last->NextGadget=&c->gadget; return &c->gadget;
}
static int ui_cycle_value(struct Gadget *g) { UIControl *c=ui_find(g); return c?c->value:0; }
static void ui_cycle_set(struct Gadget *g,struct Window *w,int value) {
    UIControl *c=ui_find(g); if(!c || !c->count) return;
    c->value=(value%c->count+c->count)%c->count; ui_render_control(c);
    if(w) RefreshGList(g,w,NULL,1);
}
static void ui_label(struct Gadget *g,struct Window *w,const char *label,int selected) {
    UIControl *c=ui_find(g); if(!c) return;
    copy(c->label,label,sizeof c->label);
    if(selected) g->Flags|=GFLG_SELECTED; else g->Flags&=~GFLG_SELECTED;
    ui_render_control(c); if(w) RefreshGList(g,w,NULL,1);
}
static void ui_controls_free(struct Gadget **head) {
    struct Gadget **link=head; UIControl *c,**owner;
    /* Free only controls owned by this detached list, never another page. */
    while(*link) {
        c=ui_find(*link);
        if(!c) { link=&(*link)->NextGadget; continue; }
        *link=c->gadget.NextGadget;
        for(owner=&ui_controls;*owner && *owner!=c;owner=&(*owner)->next) {}
        if(*owner) *owner=c->next;
        WaitBlit(); FreeMem(c->pixels,c->image_bytes*2); FreeMem(c,sizeof *c);
    }
}
static void ui_string_colors(struct Gadget *g,struct Window *w) {
    struct StringInfo *info=(struct StringInfo *)g->SpecialInfo;
    if(info && info->Extension) {
        info->Extension->Pens[0]=(UBYTE)control_pens[2]; info->Extension->Pens[1]=(UBYTE)control_pens[1];
        info->Extension->ActivePens[0]=(UBYTE)control_pens[0]; info->Extension->ActivePens[1]=(UBYTE)control_pens[4];
        if(w) RefreshGList(g,w,NULL,1);
    }
}
