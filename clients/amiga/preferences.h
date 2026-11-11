/* Native standalone preferences UI. Draft edits never touch the running client. */
#include <graphics/view.h>
#define PREF_SECTION_COUNT 4
#define PREF_WIDTH 620
#define PREF_HEIGHT 244
static const char *pref_sections[]={"General","Appearance","Network","About"};
typedef struct {
    char name[64],folder[512];
    int visible,receiving,automatic,ask,start,close_iconify,theme,sound;
    unsigned long send_limit,receive_limit;
} PrefDraft;
typedef struct {
    struct Window *w;
    struct Gadget *head,*fields[15];
    struct Gadget *pages[PREF_SECTION_COUNT],*page_fields[PREF_SECTION_COUNT][15];
    int section,closing,invalid_limits,obtained[7],editor_obtained[7];
    UWORD pens[7],editor_pens[7];
    char navigation[PREF_SECTION_COUNT][24];
    PrefDraft draft,initial;
} PrefEditor;
static void pref_from_globals(PrefDraft *d) {
    copy(d->name,client_name,sizeof d->name); copy(d->folder,download,sizeof d->folder);
    d->send_limit=send_limit;d->receive_limit=receive_limit;
    d->visible=visible; d->receiving=receiving; d->automatic=auto_accept;
    d->ask=ask_location; d->start=start_iconified; d->close_iconify=close_to_iconify; d->theme=theme_index;d->sound=completion_sound;
}
static void pref_to_globals(const PrefDraft *d) {
    copy(client_name,d->name,sizeof client_name); copy(download,d->folder,sizeof download);
    send_limit=d->send_limit;receive_limit=d->receive_limit;
    visible=d->visible; receiving=d->receiving; auto_accept=d->automatic;
    ask_location=d->ask; start_iconified=d->start; close_to_iconify=d->close_iconify; theme_index=d->theme;completion_sound=d->sound;
}
static void pref_text(struct RastPort *rp,int x,int y,UWORD pen,const char *text) {
    SetAPen(rp,pen); SetDrMd(rp,JAM1); Move(rp,x,y); Text(rp,(STRPTR)text,strlen(text));
}
static void pref_fill(struct RastPort *rp,int x,int y,int right,int bottom,UWORD pen) {
    SetAPen(rp,pen); RectFill(rp,x,y,right,bottom);
}
static void pref_release_pens(PrefEditor *e) {
    int i;
    for(i=0;i<7;i++) if(e->obtained[i]) {
        ReleasePen(screen->ViewPort.ColorMap,e->pens[i]); e->obtained[i]=0;
    }
}
static void pref_obtain_palette(int theme,UWORD *pens,int *obtained) {
    ui_palette_get(theme,pens,obtained);
}
static void pref_preview_palette(PrefEditor *e) {
    pref_release_pens(e);
    pref_obtain_palette(e->draft.theme,e->pens,e->obtained);
}
static void pref_release_editor_palette(PrefEditor *e) {
    int i;
    for(i=0;i<7;i++) if(e->editor_obtained[i]) {
        ReleasePen(screen->ViewPort.ColorMap,e->editor_pens[i]); e->editor_obtained[i]=0;
    }
}
static void pref_background(PrefEditor *e) {
    pref_fill(e->w->RPort,e->w->BorderLeft,e->w->BorderTop,
        e->w->Width-e->w->BorderRight-1,e->w->Height-e->w->BorderBottom-1,e->editor_pens[0]);
}
static void pref_draw(PrefEditor *e) {
    struct RastPort *rp=e->w->RPort; UWORD *p=e->pens,muted=e->editor_pens[3],text=e->editor_pens[2];
    SetAPen(rp,e->editor_pens[6]); Move(rp,148,27); Draw(rp,148,204);
    pref_text(rp,16,203,muted,"BitSkim 0.1");
    pref_text(rp,168,37,e->editor_pens[4],pref_sections[e->section]);
    if(e->section==0) {
        pref_text(rp,202,63,text,"Name");
        pref_text(rp,207,88,text,"Start iconified");
        pref_text(rp,207,111,text,"Close window to iconify");
        pref_text(rp,207,134,text,"Automatically accept offers");
        pref_text(rp,207,157,text,"Always ask where to save");
        pref_text(rp,194,190,text,"Folder");
        pref_text(rp,207,209,text,"Completion beep");
    } else if(e->section==1) {
        pref_text(rp,194,67,text,"Theme");
        pref_fill(rp,172,83,598,194,p[6]);
        pref_fill(rp,174,85,596,192,p[0]);
        pref_fill(rp,182,93,588,111,p[1]);
        pref_text(rp,190,105,p[4],"BitSkim 0.1 - Preview");
        pref_fill(rp,184,118,586,137,p[1]);
        pref_text(rp,192,130,p[2],"Amiga     Mac     PC");
        pref_text(rp,184,150,p[3],"example.txt    64 KiB / 128 KiB");
        pref_fill(rp,184,156,586,163,p[1]);
        pref_fill(rp,184,156,385,163,p[4]);
        pref_fill(rp,460,170,586,185,p[4]);
        pref_text(rp,488,181,p[0],"Send file");
        pref_text(rp,172,207,muted,"Preview only; apply with Save.");
    } else if(e->section==2) {
        pref_text(rp,207,65,text,"Visible on network");
        pref_text(rp,207,95,text,"Allow incoming transfers");
        pref_text(rp,172,126,muted,"Transfer limits (KiB/s; 0 = Unlimited)");
        pref_text(rp,172,157,text,"Send");
        pref_text(rp,172,187,text,"Receive");
        pref_text(rp,172,209,muted,"Limits apply starting with the next file.");
    } else {
        pref_text(rp,172,65,text,"BitSkim 0.1");
        pref_text(rp,172,86,muted,"Source repository:");
        pref_text(rp,172,104,text,"https://github.com/retrocs-org/bitskim");
        pref_text(rp,172,126,muted,"Website:");
        pref_text(rp,172,144,text,"https://bitskim.retrocs.org");
        pref_text(rp,172,166,muted,"ARexx: BITSKIM - see ARexx/README.MD");
        pref_text(rp,172,188,muted,"MIT license. Catppuccin palette credits");
        pref_text(rp,172,204,muted,"are included in the release documentation.");
    }
}
static void pref_capture(PrefEditor *e) {
    struct Gadget **f=e->fields;
    if(e->section==0) {
        copy(e->draft.name,((struct StringInfo *)f[1]->SpecialInfo)->Buffer,sizeof e->draft.name);
        e->draft.automatic=(f[4]->Flags&GFLG_SELECTED)!=0;
        e->draft.ask=(f[5]->Flags&GFLG_SELECTED)!=0;
        copy(e->draft.folder,((struct StringInfo *)f[6]->SpecialInfo)->Buffer,sizeof e->draft.folder);
        e->draft.start=(f[10]->Flags&GFLG_SELECTED)!=0;
        e->draft.close_iconify=(f[11]->Flags&GFLG_SELECTED)!=0;
        e->draft.sound=(f[14]->Flags&GFLG_SELECTED)!=0;
    } else if(e->section==1) {
        e->draft.theme=ui_cycle_value(f[9]);
    } else if(e->section==2) {
        e->draft.visible=(f[2]->Flags&GFLG_SELECTED)!=0;
        e->draft.receiving=(f[3]->Flags&GFLG_SELECTED)!=0;
        e->invalid_limits=!bs_limit_parse(((struct StringInfo *)f[12]->SpecialInfo)->Buffer,&e->draft.send_limit) || !bs_limit_parse(((struct StringInfo *)f[13]->SpecialInfo)->Buffer,&e->draft.receive_limit);
    }
}
static int pref_build(PrefEditor *e) {
    struct Gadget *g; int i;char value[24];
    if(e->pages[e->section]) {
        e->head=e->pages[e->section];
        memcpy(e->fields,e->page_fields[e->section],sizeof e->fields);
        goto attach;
    }
    memset(e->fields,0,sizeof e->fields);
    g=CreateContext(&e->head);
    for(i=0;i<PREF_SECTION_COUNT;i++) {
        sprintf(e->navigation[i],"%s%s",i==e->section?"> ":"",pref_sections[i]);
        g=add(g,BUTTON_KIND,20+i,12,30+i*30,124,24,e->navigation[i],TAG_IGNORE,0);
        if(!g) goto failed;
        if(i==e->section) g->Flags|=GFLG_SELECTED;
    }
    if(e->section==0) {
        g=e->fields[1]=add(g,STRING_KIND,1,250,52,348,18,NULL,GTST_MaxChars,63);
        if(!g) goto failed;
        g=e->fields[10]=add(g,CHECKBOX_KIND,10,172,78,22,14,NULL,GTCB_Checked,e->draft.start);
        if(!g) goto failed;
        g=e->fields[11]=add(g,CHECKBOX_KIND,11,172,101,22,14,NULL,GTCB_Checked,e->draft.close_iconify);
        if(!g) goto failed;
        g=e->fields[4]=add(g,CHECKBOX_KIND,4,172,124,22,14,NULL,GTCB_Checked,e->draft.automatic);
        if(!g) goto failed;
        g=e->fields[5]=add(g,CHECKBOX_KIND,5,172,147,22,14,NULL,GTCB_Checked,e->draft.ask);
        if(!g) goto failed;
        g=e->fields[6]=add(g,STRING_KIND,6,250,179,348,18,NULL,GTST_MaxChars,511);
        if(!g) goto failed;
        g=e->fields[14]=add(g,CHECKBOX_KIND,14,172,197,22,14,NULL,GTCB_Checked,e->draft.sound);
        if(!g)goto failed;
    } else if(e->section==1) {
        g=e->fields[9]=add(g,CYCLE_KIND,9,250,53,348,20,NULL,GTCY_Labels,(ULONG)theme_labels);
        if(!g) goto failed;
    } else if(e->section==2) {
        g=e->fields[2]=add(g,CHECKBOX_KIND,2,172,55,22,14,NULL,GTCB_Checked,e->draft.visible);
        if(!g) goto failed;
        g=e->fields[3]=add(g,CHECKBOX_KIND,3,172,85,22,14,NULL,GTCB_Checked,e->draft.receiving);
        if(!g) goto failed;
        g=e->fields[12]=add(g,STRING_KIND,12,282,145,150,18,NULL,GTST_MaxChars,7);
        if(!g)goto failed;
        g=e->fields[13]=add(g,STRING_KIND,13,282,175,150,18,NULL,GTST_MaxChars,7);
        if(!g)goto failed;

    }
    if(!g) goto failed;
    g=add(g,BUTTON_KIND,32,172,217,70,22,"Help",TAG_IGNORE,0);
    if(!g) goto failed;
    g=add(g,BUTTON_KIND,8,248,217,80,22,"Cancel",TAG_IGNORE,0);
        if(!g) goto failed;
    g=add(g,BUTTON_KIND,31,334,217,80,22,"Revert",TAG_IGNORE,0);
        if(!g) goto failed;
    g=add(g,BUTTON_KIND,33,420,217,80,22,"Use",TAG_IGNORE,0);
    if(!g) goto failed;
    g=add(g,BUTTON_KIND,7,506,217,92,22,"Save",TAG_IGNORE,0);
    if(!g) goto failed;
    e->pages[e->section]=e->head;
    memcpy(e->page_fields[e->section],e->fields,sizeof e->fields);
attach:
    /* Reapply the draft on cached pages, including after Revert. */
    if(e->fields[2]) { if(e->draft.visible) e->fields[2]->Flags|=GFLG_SELECTED; else e->fields[2]->Flags&=~GFLG_SELECTED; }
    if(e->fields[3]) { if(e->draft.receiving) e->fields[3]->Flags|=GFLG_SELECTED; else e->fields[3]->Flags&=~GFLG_SELECTED; }
    if(e->fields[4]) { if(e->draft.automatic) e->fields[4]->Flags|=GFLG_SELECTED; else e->fields[4]->Flags&=~GFLG_SELECTED; }
    if(e->fields[5]) { if(e->draft.ask) e->fields[5]->Flags|=GFLG_SELECTED; else e->fields[5]->Flags&=~GFLG_SELECTED; }
    if(e->fields[10]) { if(e->draft.start) e->fields[10]->Flags|=GFLG_SELECTED; else e->fields[10]->Flags&=~GFLG_SELECTED; }
    if(e->fields[11]) { if(e->draft.close_iconify) e->fields[11]->Flags|=GFLG_SELECTED; else e->fields[11]->Flags&=~GFLG_SELECTED; }
    if(e->fields[14]) {if(e->draft.sound)e->fields[14]->Flags|=GFLG_SELECTED;else e->fields[14]->Flags&=~GFLG_SELECTED;}
    pref_background(e);
    AddGList(e->w,e->head,~0UL,-1,NULL);
    if(e->fields[1]) GT_SetGadgetAttrs(e->fields[1],e->w,NULL,GTST_String,e->draft.name,TAG_DONE);
    if(e->fields[6]) GT_SetGadgetAttrs(e->fields[6],e->w,NULL,GTST_String,e->draft.folder,TAG_DONE);
    if(e->fields[12]){sprintf(value,"%lu",e->draft.send_limit/1024UL);GT_SetGadgetAttrs(e->fields[12],e->w,NULL,GTST_String,value,TAG_DONE);ui_string_colors(e->fields[12],e->w);}
    if(e->fields[13]){sprintf(value,"%lu",e->draft.receive_limit/1024UL);GT_SetGadgetAttrs(e->fields[13],e->w,NULL,GTST_String,value,TAG_DONE);ui_string_colors(e->fields[13],e->w);}
    if(e->fields[9]) ui_cycle_set(e->fields[9],e->w,e->draft.theme);
    if(e->fields[1]) ui_string_colors(e->fields[1],e->w);
    if(e->fields[6]) ui_string_colors(e->fields[6],e->w);
    RefreshGList(e->head,e->w,NULL,-1); GT_RefreshWindow(e->w,NULL);
    if(e->section==1) pref_preview_palette(e);
    pref_draw(e); return 1;
failed:
    ui_controls_free(&e->head);
    if(e->head) FreeGadgets(e->head);
    e->head=NULL; return 0;
}
static void pref_remove(PrefEditor *e) {
    struct IntuiMessage *m;
    if(e->head) {
        RemoveGList(e->w,e->head,-1);
        /* Input may have arrived after the main drain. Reply while gadgets live. */
        while((m=GT_GetIMsg(e->w->UserPort))) {
            if(m->Class==IDCMP_CLOSEWINDOW) e->closing=1;
            GT_ReplyIMsg(m);
        }
        /* Keep each page/context alive until the window is closed. Intuition
         * and GadTools must not encounter freed controls during navigation. */
        e->head=NULL;
    }
}
static int pref_save(PrefEditor *e,int permanent) {
    char utf8[64]; int saved_env,saved_archive;
    if(e->invalid_limits){alert("Network speed limits: enter 0..1048575 KiB/s; 0 is Unlimited.");return 0;}
    if(!bs_to_utf8(e->draft.name,utf8,sizeof utf8)) {
        alert("Client name is too long in UTF-8."); return 0;
    }
    pref_to_globals(&e->draft);
    saved_env=write_prefs(live_prefs); saved_archive=!permanent || write_prefs(archive_prefs);
    if(!saved_env || !saved_archive) {
        pref_to_globals(&e->initial);
        alert("Could not save all preferences. Check ENV:BitSkim and ENVARC:BitSkim."); return 0;
    }
    {
        struct MsgPort *port;
        Forbid(); port=FindPort((STRPTR)"BITSKIM");
        if(port) Signal(port->mp_SigTask,SIGBREAKF_CTRL_E);
        Permit();
    }
    return 1;
}
static void preferences(void) {
    PrefEditor e; struct IntuiMessage *m; int done=0,rebuild,next,action,redraw,i;
    ULONG cls; UWORD id,qualifier;
    memset(&e,0,sizeof e); pref_from_globals(&e.draft); e.initial=e.draft;
    pref_obtain_palette(theme_index,e.editor_pens,e.editor_obtained);
    memcpy(control_pens,e.editor_pens,sizeof control_pens);
    e.w=OpenWindowTags(NULL,WA_PubScreen,screen,WA_Left,(screen->Width-PREF_WIDTH)/2,
        WA_Top,((screen->Height-PREF_HEIGHT)/3>screen->BarHeight)?(screen->Height-PREF_HEIGHT)/3:screen->BarHeight+1,WA_Title,"BitSkim Preferences",WA_Width,PREF_WIDTH,WA_Height,PREF_HEIGHT,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW|STRINGIDCMP|CHECKBOXIDCMP,TAG_DONE);
    if(!e.w) { pref_release_editor_palette(&e); return; }
    if(!pref_build(&e)) done=1;
    while(!done) {
        rebuild=0; next=e.section; action=0; redraw=0;
        while((m=GT_GetIMsg(e.w->UserPort))) {
            cls=m->Class; qualifier=m->Qualifier; id=(cls==IDCMP_GADGETUP && m->IAddress)?((struct Gadget *)m->IAddress)->GadgetID:0;
            GT_ReplyIMsg(m);
            if(cls==IDCMP_CLOSEWINDOW) action=8;
            else if(cls==IDCMP_REFRESHWINDOW) { GT_BeginRefresh(e.w); pref_background(&e); RefreshGList(e.head,e.w,NULL,-1); pref_draw(&e); GT_EndRefresh(e.w,TRUE); }
            else if(cls==IDCMP_GADGETUP) {
                if(id>=20 && id<20+PREF_SECTION_COUNT) { next=id-20; rebuild=next!=e.section; }
                else if(id==7 || id==8 || id==31 || id==32 || id==33) action=id;
                else if(id==9) { ui_cycle_set(e.fields[9],e.w,ui_cycle_value(e.fields[9])+((qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))?-1:1)); redraw=1; }
            }
        }
        /* No gadget is freed while an IDCMP message can still refer to it. */
        if(action==8) done=1;
        else {
            pref_capture(&e);
            if(action==32) launch_help();
            if(action==7 || action==33) done=pref_save(&e,action==7);
            else if(action==31) { e.draft=e.initial;e.invalid_limits=0; rebuild=1; }
            if(!done && rebuild) {
                pref_remove(&e); pref_release_pens(&e); e.section=next;
                pref_fill(e.w->RPort,e.w->BorderLeft,e.w->BorderTop,
                    e.w->Width-e.w->BorderRight-1,e.w->Height-e.w->BorderBottom-1,e.editor_pens[0]);
                if(e.closing || !pref_build(&e)) done=1;
            } else if(!done && redraw && e.section==1) { pref_preview_palette(&e); pref_draw(&e); }
        }
        /* Every message was drained above; sleep until Intuition sends another. */
        if(!done) Wait(1UL<<e.w->UserPort->mp_SigBit);
    }
    pref_remove(&e); CloseWindow(e.w);
    for(i=0;i<PREF_SECTION_COUNT;i++) if(e.pages[i]) {
        ui_controls_free(&e.pages[i]); FreeGadgets(e.pages[i]);
    }
    pref_release_pens(&e); pref_release_editor_palette(&e);
}
