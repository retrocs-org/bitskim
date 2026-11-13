/* BitSkim 0.2 - native Workbench client. Requires OS 3.x and bsdsocket.library v4. */
#include "bitskim.h"
#include "../shared/single-send.h"
#include "themes.h"
#include <intuition/screens.h>
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include <workbench/workbench.h>
#include <workbench/startup.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/wb.h>
#include <proto/icon.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <dos/dostags.h>
#include <rexx/storage.h>
#include <proto/rexxsyslib.h>
/* Shell, ARexx and dock launchers use the caller's stack (often 4 KiB), not
   the icon's; libnix swaps to this size at startup when the current one is smaller. */
long __stack=32768;
#ifdef BITSKIM_PREFS
const char bitskim_version_tag[] = "$VER: BitSkimPrefs 0.2 (01.11.2026)";
#else
const char bitskim_version_tag[] = "$VER: BitSkim 0.2 (01.11.2026)";
#endif
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase,*WorkbenchBase,*IconBase;
struct RxsLib *RexxSysBase;
static struct MsgPort *rexx_port;
static char rexx_name[32];
static int quit_pending=0, reload_pending=0;
static int prefs_mode=0,async_ready=0;
static void client_notice(const char *s);
static void transfers_observe(void);
static void transfer_tools(void);
static void begin_send_file(const char *path);
static void launch_preferences(void);
static void rexx_poll(void);
static void rexx_close(void);
static BSClient *client;
static struct Window *window;
static struct Gadget *gadgets,*ipg,*peer_rows[5];
static int peer_offset;
static UWORD main_pens[7];
static int main_obtained[7];
static APTR visual;
static struct Screen *screen;
static int theme_index=BS_THEME_DEFAULT, theme_pending=0;
static STRPTR theme_labels[BS_THEME_COUNT+1];
static struct MsgPort *app_port;
static struct AppWindow *app_window;
static struct AppIcon *app_icon;
static struct DiskObject *app_disk_object;
static unsigned long send_limit=0,receive_limit=0;
static int completion_sound;static unsigned long completion_serial;
static int start_iconified=0, close_to_iconify=0, iconified=0, iconify_pending=0;
static int restore_ui(void);
static int iconify_ui(void);
static struct TextAttr font={(STRPTR)"topaz.font",8,0,0};
static char peer_labels[5][120];
static unsigned short listen_port=BS_PORT;
static int peer_dirty=1, visible=1, receiving=1, auto_accept=0, ask_location=1;
static char client_name[64],installation[33],download[512]="RAM:",manual_ip[48];
static char old_message[256];
static char wire_name[64];
/* Selection follows the peer itself; catalog order changes as peers come and go. */
static char selected_id[33],selected_address[48];
static int selected_peer(void) {
    int i,count;
    if(!selected_id[0] || !client) return -1;
    count=bs_peer_count(client);
    for(i=0;i<count;i++) { const BSPeer *p=bs_peer(client,i); if(!strcmp(p->id,selected_id) && !strcmp(p->address,selected_address)) return i; }
    return -1;
}
static void select_peer(int index) {
    const BSPeer *p=index>=0 && client?bs_peer(client,index):NULL;
    selected_id[0]=selected_address[0]=0;
    if(p) { strncpy(selected_id,p->id,32); selected_id[32]=0; strncpy(selected_address,p->address,47); selected_address[47]=0; }
}
static void copy(char *d,const char *s,size_t n) { strncpy(d,s,n-1); d[n-1]=0; }
static void alert(const char *s) {
    struct EasyStruct e={sizeof(e),0,(STRPTR)"BitSkim 0.2",(STRPTR)"%s",(STRPTR)"OK"};
    if(async_ready && client) { client_notice(s); return; }
    EasyRequest(window,&e,NULL,s);
}
/* Mix several independent sources and use the high bits of a xorshift
   generator; the low bits of a C library LCG can have a very short period. */
static void new_installation_id(char *out) {
    ULONG seconds=0,micros=0,x; struct DateStamp now; int i;
    CurrentTime(&seconds,&micros); DateStamp(&now);
    x=(ULONG)time(NULL)^(ULONG)FindTask(NULL)^(micros<<12)^((ULONG)now.ds_Tick<<20)^((ULONG)VBeamPos()<<7)^0x9e3779b9UL;
    if(!x) x=0x2545f491UL;
    for(i=0;i<32;i++) { x^=x<<13; x^=x>>17; x^=x<<5; out[i]="0123456789abcdef"[(x>>28)&15]; }
    out[32]=0;
}
#include "locations.h"
static char archive_prefs[1024]="ENVARC:BitSkim/BitSkim.prefs";
static char live_prefs[1024]="ENV:BitSkim/BitSkim.prefs";
static void configure_archive_prefs(void) {
    BPTR lock; const char *dirs[]={"ENV:BitSkim","ENVARC:BitSkim"}; int i;
    for(i=0;i<2;i++) { lock=CreateDir((STRPTR)dirs[i]); if(lock) UnLock(lock); }
}
static void trim(char *s) { char *p=strchr(s,'\n'); if(p) *p=0; p=strchr(s,'\r'); if(p) *p=0; }
static void load_prefs(void) {
    FILE *f; char line[600],old_shared[1024],*v; int i,theme_defaults=0;
    /* Missing opt-in settings must not retain an earlier live reload's values. */
    start_iconified=0; close_to_iconify=0;completion_sound=0;send_limit=receive_limit=0;
    if(!client_name[0]) copy(client_name,"Amiga",sizeof(client_name));
    new_installation_id(installation);
    f=fopen(live_prefs,"r"); if(!f) f=fopen(archive_prefs,"r"); if(!f && amiga_install_path("shared.path",old_shared,sizeof old_shared) && AddPart(old_shared,"Config/BitSkim.prefs",sizeof old_shared)) f=fopen(old_shared,"r"); if(!f && strcmp(live_prefs,"ENV:BitSkim.prefs")) f=fopen("ENV:BitSkim.prefs","r"); if(!f && strcmp(archive_prefs,"ENVARC:BitSkim.prefs")) f=fopen("ENVARC:BitSkim.prefs","r"); if(!f) return;
    while(fgets(line,sizeof(line),f)) {
        trim(line); v=strchr(line,'='); if(!v) continue; *v++=0;
        if(!strcmp(line,"name")) copy(client_name,v,sizeof(client_name));
        else if(!strcmp(line,"id") && strlen(v)==32) copy(installation,v,sizeof(installation));
        else if(!strcmp(line,"visible")) visible=atoi(v)!=0;
        else if(!strcmp(line,"receiving")) receiving=atoi(v)!=0;
        else if(!strcmp(line,"auto_accept")) auto_accept=atoi(v)!=0;
        else if(!strcmp(line,"ask_location")) ask_location=atoi(v)!=0;
        else if(!strcmp(line,"completion_sound"))completion_sound=atoi(v)!=0;
        else if(!strcmp(line,"start_iconified")) start_iconified=atoi(v)!=0;
        else if(!strcmp(line,"close_to_iconify")) close_to_iconify=atoi(v)!=0;
        else if(!strcmp(line,"send_limit_kib")) {if(!bs_limit_parse(v,&send_limit))send_limit=0;}
        else if(!strcmp(line,"receive_limit_kib")) {if(!bs_limit_parse(v,&receive_limit))receive_limit=0;}
        else if(!strcmp(line,"download")) copy(download,v,sizeof(download));
        else if(!strcmp(line,"theme_defaults")) theme_defaults=atoi(v);
        else if(!strcmp(line,"theme")) { for(i=0;i<BS_THEME_COUNT;i++) if(!strcmp(v,bs_themes[i].id)) theme_index=i; }
    }
    fclose(f);
    /* Older builds saved Classic automatically. Migrate that old default once;
       an explicit Classic choice saved by this version remains Classic. */
    if(!theme_defaults && theme_index==BS_THEME_CLASSIC) theme_index=BS_THEME_DEFAULT;
}
static int write_prefs(const char *path) {
    FILE *f=fopen(path,"w"); int ok;
    if(!f) return 0;
    ok=fprintf(f,"name=%s\nid=%s\nvisible=%ld\nreceiving=%ld\nauto_accept=%ld\nask_location=%ld\ndownload=%s\ntheme=%s\ntheme_defaults=1\nstart_iconified=%ld\nclose_to_iconify=%ld\ncompletion_sound=%ld\nsend_limit_kib=%lu\nreceive_limit_kib=%lu\n",client_name,installation,(long)visible,(long)receiving,(long)auto_accept,(long)ask_location,download,bs_themes[theme_index].id,(long)start_iconified,(long)close_to_iconify,(long)completion_sound,send_limit/1024UL,receive_limit/1024UL)>=0;
    if(fclose(f)) ok=0;
    return ok;
}
static void save_prefs(void) { int a=write_prefs(live_prefs),b=write_prefs(archive_prefs); if(!a || !b) alert("Could not save all preferences. Check ENV:BitSkim and ENVARC:BitSkim."); }
#include "controls.h"
static struct Gadget *add(struct Gadget *last,ULONG kind,int id,int x,int y,int w,int h,const char *label,ULONG tag,ULONG value) {
    struct NewGadget n;
    if(kind==BUTTON_KIND || kind==CYCLE_KIND || kind==CHECKBOX_KIND || kind==UI_ROW_KIND)
        return ui_control_add(last,kind,id,x,y,w,h,label,value);
    memset(&n,0,sizeof(n)); n.ng_LeftEdge=x; n.ng_TopEdge=y; n.ng_Width=w; n.ng_Height=h;
    n.ng_GadgetText=(STRPTR)label; n.ng_TextAttr=&font; n.ng_GadgetID=id; n.ng_VisualInfo=visual; n.ng_Flags=(kind==STRING_KIND || kind==CYCLE_KIND)?PLACETEXT_LEFT:(kind==CHECKBOX_KIND?PLACETEXT_RIGHT:PLACETEXT_IN);
    return CreateGadget(kind,last,&n,tag,value,TAG_DONE);
}

static void send_file(const char *path) { begin_send_file(path); }
static void close_ui(void);
#include "arexx.h"
#include "transfer_dialogs.h"
static void callback(void *unused,BSEvent event) {
    (void)unused;
    if(event==BS_EVENT_PEERS) peer_dirty=1;
    record_event(event);
    if(event==BS_EVENT_COMPLETE || event==BS_EVENT_ERROR)transfers_observe();
}
/* Binary display units, using integer arithmetic on classic hardware. */
static void transfer_size(unsigned long bytes,char *out) {
    static const char *units[]={"B","KiB","MiB","GiB"};
    unsigned long whole=bytes,remainder=0,fraction; int unit=0;
    while(whole>=1024 && unit<3) { remainder=whole%1024; whole/=1024; unit++; }
    if(!unit) sprintf(out,"%lu B",whole);
    else { fraction=(remainder*10+512)/1024; if(fraction==10) { whole++; fraction=0; } sprintf(out,"%lu.%lu %s",whole,fraction,units[unit]); }
}
static void transfer_details(unsigned long done,unsigned long total,unsigned long rate,char *out) {
    char sent[20],size[20],left[20],speed[20]; unsigned long percent;
    if(done>total) done=total;
    /* 32-bit only (no libgcc 64-bit division on the 68000): exact below
       ~41 MiB, within one percent above it. */
    if(!total) percent=0;
    else if(total<=42949672UL) percent=done*100/total;
    else percent=done/(total/100);
    if(percent>100) percent=100;
    transfer_size(done,sent); transfer_size(total,size); transfer_size(total-done,left); transfer_size(rate,speed);
    sprintf(out,"%s / %s | %lu%% | %s left | %s/s",sent,size,percent,left,speed);
}
#include "transfer_panel.h"
#include "queue_menu.h"
static void redraw(void) {
    const BSStatus *s=bs_status(client); int i,count,selected; const BSPeer *p;
    if(!window) return;
    if(peer_dirty) {
        count=bs_peer_count(client); selected=selected_peer();
        if(peer_offset>(count>peer_slots?count-peer_slots:0)) peer_offset=count>peer_slots?count-peer_slots:0;
        for(i=0;i<peer_slots;i++) {
            int index=peer_offset+i; peer_labels[i][0]=0;
            if(index<count) {
                p=bs_peer(client,index);
                if(!bs_to_local(p->name,wire_name,sizeof(wire_name))) copy(wire_name,"Unicode peer",sizeof(wire_name));
                sprintf(peer_labels[i],"%.28s  [%.12s]  %s",wire_name,p->platform,p->address);
            }
            ui_label(peer_rows[i],window,peer_labels[i],index==selected && index<count);
        }
        peer_dirty=0;
    }
    summary_draw(!old_message[0]);
    copy(old_message,s->message,sizeof old_message);

}
#include "help.h"
#include "preferences.h"
static void main_background(void) {
    SetAPen(window->RPort,main_pens[0]);
    RectFill(window->RPort,window->BorderLeft,window->BorderTop,
        window->Width-window->BorderRight-1,window->Height-window->BorderBottom-1);
}
static void draw_labels(void) {
    char footer[110]; struct RastPort *rp=window->RPort;
    SetDrMd(rp,JAM1); SetAPen(rp,main_pens[2]); Move(rp,16,33);
    Text(rp,(STRPTR)"NEARBY COMPUTERS - select a recipient",37);
    Move(rp,14,151-main_shift); Text(rp,(STRPTR)"IP or IP:port",13);
    sprintf(footer,"TCP %lu / Discovery UDP %lu / Unencrypted",(unsigned long)listen_port,(unsigned long)BS_PORT);
    Move(rp,16,254-main_shift); Text(rp,(STRPTR)footer,strlen(footer));
}
static void close_ui(void) {
    queue_menu_close();
    if(ipg) copy(manual_ip,((struct StringInfo *)ipg->SpecialInfo)->Buffer,sizeof(manual_ip));
    if(app_window) RemoveAppWindow(app_window);
    app_window=NULL;
    if(window) CloseWindow(window);
    window=NULL;
    ui_controls_free(&gadgets);
    if(gadgets) FreeGadgets(gadgets);
    gadgets=ipg=NULL;
    if(screen) ui_palette_release(main_pens,main_obtained);
    if(visual) FreeVisualInfo(visual);
    visual=NULL;
    if(screen) UnlockPubScreen(NULL,screen);
    screen=NULL;
}
static int open_workbench(void) {
    screen=LockPubScreen((STRPTR)"Workbench"); if(!screen) return 0;
    visual=GetVisualInfo(screen,TAG_DONE); if(!visual) return 0;
    ui_palette_get(theme_index,main_pens,main_obtained);
    memcpy(control_pens,main_pens,sizeof control_pens);
    ScreenToFront(screen); return 1;
}
static int open_ui(void) {
    struct Gadget *g; int i;
    if(!open_workbench()) return 0;
    peer_slots=screen->Height<280?4:5;main_shift=(5-peer_slots)*17;
    g=CreateContext(&gadgets);
    g=add(g,BUTTON_KIND,13,510,17,74,20,"Help",TAG_IGNORE,0); if(!g) return 0;
    for(i=0;i<peer_slots;i++) {
        g=peer_rows[i]=add(g,UI_ROW_KIND,100+i,14,42+i*17,542,17,"",TAG_IGNORE,0);
        if(!g) return 0;
    }
    g=add(g,BUTTON_KIND,110,562,42,22,peer_slots*17/2-3,"^",TAG_IGNORE,0); if(!g) return 0;
    g=add(g,BUTTON_KIND,111,562,45+peer_slots*17/2,22,peer_slots*17/2-3,"v",TAG_IGNORE,0); if(!g) return 0;
    g=ipg=add(g,STRING_KIND,2,123,140-main_shift,290,15,NULL,GTST_MaxChars,47);
    g=add(g,BUTTON_KIND,3,14,163-main_shift,110,22,"Send file...",TAG_IGNORE,0);
    g=add(g,BUTTON_KIND,4,132,163-main_shift,76,22,"Refresh",TAG_IGNORE,0);
    g=add(g,BUTTON_KIND,5,216,163-main_shift,110,22,"Preferences",TAG_IGNORE,0);
    g=add(g,BUTTON_KIND,6,334,163-main_shift,72,22,"Cancel",TAG_IGNORE,0);
    g=add(g,BUTTON_KIND,11,414,163-main_shift,80,22,"Iconify",TAG_IGNORE,0);
    if(!g) return 0;
    g=add(g,BUTTON_KIND,12,502,163-main_shift,82,22,"Quit",TAG_IGNORE,0);
    if(!g) return 0;
    window=OpenWindowTags(NULL,WA_PubScreen,screen,WA_Left,(screen->Width-600)/2,WA_Top,screen->BarHeight+1,WA_Title,"BitSkim 0.2 - local file sharing",WA_Width,600,WA_Height,260-main_shift,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,
        WA_Activate,TRUE,WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW|LISTVIEWIDCMP|STRINGIDCMP|IDCMP_MENUPICK,TAG_DONE);
    if(!window) return 0;
    if(!queue_menu_open())return 0;
    main_background();
    AddGList(window,gadgets,~0UL,-1,NULL);
    RefreshGList(gadgets,window,NULL,-1); GT_RefreshWindow(window,NULL); draw_labels();
    GT_SetGadgetAttrs(ipg,window,NULL,GTST_String,manual_ip,TAG_DONE);
    ui_string_colors(ipg,window);
    old_message[0]=0; peer_dirty=1; select_peer(-1); peer_offset=0;
    app_window=AddAppWindow(1,0,window,app_port,TAG_DONE);
    return 1;
}
/* Register the desktop icon before hiding any windows. The message port,
   DiskObject and transfer engine outlive every visible window/screen. */
static int iconify_ui(void) {
    if(iconified) return 1;
    if(!IconBase || !app_port) { alert("Iconify is unavailable: icon.library or the Workbench message port could not be opened."); return 0; }
    if(!app_disk_object) {
        app_disk_object=GetDiskObject((STRPTR)"PROGDIR:BitSkim");
        if(!app_disk_object) app_disk_object=GetDefDiskObject(WBTOOL);
    }
    if(!app_disk_object) { alert("Could not load an icon. BitSkim will stay open."); return 0; }
    app_icon=AddAppIconA(2,0,(STRPTR)"BitSkim",app_port,0,app_disk_object,NULL);
    if(!app_icon) { alert("Workbench could not create the background icon. BitSkim will stay open."); return 0; }
    close_ui(); iconified=1; bs_discovery_search(client,bs_queue_count(client)>0);
    return 1;
}
static int restore_ui(void) {
    if(window) {
        WindowToFront(window); ActivateWindow(window);
        ScreenToFront(screen);
        return 1;
    }
    if(!open_ui()) {
        close_ui();
        alert("Could not reopen the BitSkim window. The background icon and network service remain active; double-click the icon to try again.");
        return 0;
    }
    if(app_icon) RemoveAppIcon(app_icon);
    app_icon=NULL; iconified=0; bs_discovery_search(client,1);
    return 1;
}
int main(int argc,char **argv) {
    struct IntuiMessage *m; struct AppMessage *drop;
    BSConfig config; ULONG cls; UWORD id,app_type; LONG app_args; int running=1,arg;UWORD menu_code=0; char path[1024];
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37); GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",37);
    GadToolsBase=OpenLibrary("gadtools.library",37); WorkbenchBase=OpenLibrary("workbench.library",37);
    IconBase=OpenLibrary("icon.library",37);
    if(!IntuitionBase || !GfxBase || !GadToolsBase || !WorkbenchBase) goto cleanup;
    control_font=OpenFont(&font);
    configure_archive_prefs();
#ifdef BITSKIM_PREFS
    prefs_mode=1;
#endif
    if(prefs_mode) {
        for(arg=0;arg<BS_THEME_COUNT;arg++) theme_labels[arg]=(STRPTR)bs_themes[arg].name;
        theme_labels[BS_THEME_COUNT]=NULL; load_prefs();
        screen=LockPubScreen((STRPTR)"Workbench"); if(!screen) goto cleanup;
        visual=GetVisualInfo(screen,TAG_DONE); if(!visual) goto cleanup;
        ScreenToFront(screen);
        preferences(); goto cleanup;
    }
    if(!bs_platform_init()) { alert("Start a TCP/IP stack providing bsdsocket.library v4, then open BitSkim again."); goto cleanup; }
    for(arg=1;arg<argc;arg++) if(!strncmp(argv[arg],"PORT=",5)) { long value=atol(argv[arg]+5); if(value>=1 && value<=65535) listen_port=(unsigned short)value; }
    for(arg=0;arg<BS_THEME_COUNT;arg++) theme_labels[arg]=(STRPTR)bs_themes[arg].name;
    theme_labels[BS_THEME_COUNT]=NULL;
    bs_hostname(client_name,sizeof(client_name));
    load_prefs(); memset(&config,0,sizeof(config)); if(!bs_to_utf8(client_name,config.name,sizeof(config.name))) copy(config.name,"Amiga",sizeof(config.name)); copy(config.id,installation,sizeof(config.id)); copy(config.platform,"AmigaOS",sizeof(config.platform)); config.port=listen_port; config.discovery_port=BS_PORT; config.visible=visible; config.receiving=receiving;
    { ULONG seconds,micros; char base[25]; CurrentTime(&seconds,&micros);
      snprintf(base,sizeof(base),"%08lx%08lx%08lx",(unsigned long)seconds,(unsigned long)micros,(unsigned long)FindTask(NULL)); rexx_session_init(base); }
    client=bs_create(&config,callback,NULL);
    if(!client && listen_port==BS_PORT) { listen_port=BS_PORT+1; config.port=listen_port; client=bs_create(&config,callback,NULL); }
    if(!client) { alert("Cannot listen on TCP/UDP port 46440. For same-Mac test system testing, run Bin:BitSkim/BitSkim PORT=46441."); goto cleanup; }
    bs_folders_configure(client,"ENVARC:BitSkim/BSFOLD.DAT");
    bs_limits_configure(client,send_limit,receive_limit);
    app_port=CreateMsgPort();
    if(!app_port) { alert("Could not create the Workbench message port."); goto cleanup; }
    if(!start_iconified || !iconify_ui()) { if(!open_ui()) goto cleanup; }
    rexx_open();
    bs_batch_enable(client,1);
    async_ready=1;
    save_prefs();
    while(running) {
        rexx_poll();
        if(iconified) bs_discovery_search(client,bs_queue_count(client)>0);
        if(SetSignal(0,SIGBREAKF_CTRL_E)&SIGBREAKF_CTRL_E) reload_pending=1;
        if(SetSignal(0,SIGBREAKF_CTRL_C)&SIGBREAKF_CTRL_C) quit_pending=1;
        if(reload_pending && !bs_status(client)->active) {
            int previous=theme_index;
            load_prefs();bs_limits_configure(client,send_limit,receive_limit);
            if(bs_to_utf8(client_name,wire_name,sizeof(wire_name))) bs_configure(client,wire_name,visible,receiving);
            if(previous!=theme_index && !iconified) theme_pending=1;
            reload_pending=0;
        }
        if(quit_pending) break;
        bs_tick(client);if(bs_completion_notice(client,&completion_serial) && completion_sound)DisplayBeep(NULL); transfers_observe(); redraw(); dialog_tick();
        while(window && (m=GT_GetIMsg(window->UserPort))) {
            menu_code=m->Code;
            cls=m->Class; id=(cls==IDCMP_GADGETUP && m->IAddress)?((struct Gadget *)m->IAddress)->GadgetID:0; GT_ReplyIMsg(m);
            if(cls==IDCMP_MENUPICK) {queue_menu_command(menu_code);old_message[0]=0;}
            if(cls==IDCMP_CLOSEWINDOW) { if(close_to_iconify) iconify_pending=1; else running=0; }
            if(cls==IDCMP_REFRESHWINDOW) { GT_BeginRefresh(window); main_background(); RefreshGList(gadgets,window,NULL,-1); draw_labels(); GT_EndRefresh(window,TRUE); old_message[0]=0; peer_dirty=1; }
            if(cls==IDCMP_GADGETUP) {
                if(id>=100 && id<100+peer_slots) { int index=peer_offset+id-100; if(index<bs_peer_count(client)) { select_peer(index); peer_dirty=1; } }
                else if(id==110) { if(peer_offset>0) peer_offset--; peer_dirty=1; }
                else if(id==111) { if(peer_offset+peer_slots<bs_peer_count(client)) peer_offset++; peer_dirty=1; }
                else if(id==2) { select_peer(-1); peer_dirty=1; }
                else if(id==3) send_file(NULL);
                else if(id==4) bs_refresh(client);
                else if(id==5) launch_preferences();
                else if(id==6) bs_cancel(client);
                else if(id==11) iconify_pending=1;
                else if(id==12) running=0;
                else if(id==13) launch_help();
            }
        }
        if(app_port) while((drop=(struct AppMessage *)GetMsg(app_port))) {
            path[0]=0; app_type=drop->am_Type; app_args=drop->am_NumArgs;
            if(app_type==AMTYPE_APPWINDOW && window && drop->am_ArgList) {
                LONG arg;
                if(app_args>1) alert("Drop one file or directory.");
                else for(arg=0;arg<app_args;arg++) {
                    path[0]=0;
                    if(drop->am_ArgList[arg].wa_Lock && NameFromLock(drop->am_ArgList[arg].wa_Lock,(STRPTR)path,sizeof(path)) && AddPart((STRPTR)path,drop->am_ArgList[arg].wa_Name,sizeof(path))) send_file(path);
                    else alert("Could not read a dropped file path.");
                }
            }
            ReplyMsg((struct Message *)drop);
            /* Reply before removing/restoring any Workbench registrations. */
            if(app_type==AMTYPE_APPICON) {
                if(restore_ui() && app_args>0) alert("Select a computer, then drop the file into the BitSkim window or use Send file.");
            }
        }
        if(theme_pending) { close_ui(); if(!open_ui()) { running=0; } theme_pending=0; }
        if(running && iconify_pending) { iconify_ui(); iconify_pending=0; }
        if(running && bs_status(client)->awaiting_decision) incoming();
        /* Sleep until a socket, the window, Workbench, ARexx or a break signal
           needs attention (at most one second, for engine timers). */
        if(running && !quit_pending) {
            ULONG wait=SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_E,got;
            if(window) wait|=1UL<<window->UserPort->mp_SigBit;
            if(app_port) wait|=1UL<<app_port->mp_SigBit;
            if(rexx_port) wait|=1UL<<rexx_port->mp_SigBit;
            if(transfer_dialog.w) wait|=1UL<<transfer_dialog.w->UserPort->mp_SigBit;
            got=bs_wait(client,wait,1000);
            if(got&SIGBREAKF_CTRL_E) reload_pending=1;
            if(got&SIGBREAKF_CTRL_C) quit_pending=1;
        }
    }
cleanup:
    async_ready=0; dialog_close();
    rexx_close();
    if(app_icon) RemoveAppIcon(app_icon);
    app_icon=NULL;
    close_ui();
    if(app_port) {
        while((drop=(struct AppMessage *)GetMsg(app_port))) ReplyMsg((struct Message *)drop);
        DeleteMsgPort(app_port); app_port=NULL;
    }
    if(app_disk_object) FreeDiskObject(app_disk_object);
    app_disk_object=NULL;
    if(client) bs_destroy(client);
    if(!prefs_mode) bs_platform_cleanup();
    if(IconBase) CloseLibrary(IconBase);
    if(WorkbenchBase) CloseLibrary(WorkbenchBase);
    if(GadToolsBase) CloseLibrary(GadToolsBase);
    if(control_font) CloseFont(control_font);
    if(GfxBase) CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return 0;
}
