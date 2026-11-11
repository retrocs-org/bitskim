/* BitSkim 0.1 - conservative RISC OS 3.1 Wimp client, APCS-26. */
#include "bitskim.h"
#include <kernel.h>
#include <swis.h>
#include <unixlib/local.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#define PTR(p) ((int)(p))
#define MAX_ICONS 40
#define WIDTH 1200
#define HEIGHT 686
#define ICON_TEXT 1
#define ICON_BORDER 4
#define ICON_HCENTRE 8
#define ICON_VCENTRE 16
#define ICON_FILLED 32
#define ICON_INDIRECT 256
#define BUTTON (3<<12)
#define WRITABLE (15<<12)
typedef struct { int id; char text[512]; } TextIcon;
typedef struct { int handle,count; TextIcon icons[MAX_ICONS]; } View;
typedef struct {
    char name[64],folder[512],id[33];
    int visible,receiving,automatic,ask,start,close_iconify,theme,bell;
    unsigned long send_limit,receive_limit;
} Preferences;
static BSClient *engine;
static View main_view,pref_view,offer_view,network_view,tools_view;
static int tools_button,tools_mode=-1,tools_top,tools_lines[10],tools_buttons[7];
static unsigned long tools_serial,bell_serial;
static int pref_network,net_send,net_receive,net_save,net_cancel;
static int offer_conflict,offer_space;static unsigned long offer_space_clock;
static void tree_offer_rows(void);
static int offer_top,offer_previous,offer_next,offer_rows[8];
static Preferences prefs,draft;
static int task,iconbar=-1,running=1,hidden,selected=-1,offset,peers_dirty=1;
static int peer_icons[5],ip_icon,path_icon,status_icon,filename_icon,stats_icon,bar_icon;
static int pref_name,pref_folder,pref_checks[7],pref_theme,pref_cancel,pref_revert,pref_save,offer_path,offer_accept,offer_decline;
static char selected_id[33];
static char manual_ip[64],send_path[512],prefs_path[1024],icon_name[16]="!bitskim";
static char offer_filename[256]; static int offer_open;
static unsigned long last_ui;
static const char *theme_names[]={"Catppuccin Latte","Catppuccin Frappe","Catppuccin Macchiato","Catppuccin Mocha","BitSkim Rosewater","BitSkim Mint","Arctic","Classic"};
/* Standard Wimp colour approximations; never alter the desktop palette. */
static const int bases[]={1,6,6,7,1,6,6,1},foregrounds[]={7,0,0,0,7,0,0,7};
static void copy(char *d,const char *s,size_t n) { strncpy(d,s?s:"",n-1); d[n-1]=0; }
static int call(int swi,_kernel_swi_regs *r) { return _kernel_swi(swi,r,r)==NULL; }
static void report(const char *s) {
    _kernel_oserror e; _kernel_swi_regs r;
    e.errnum=0; copy(e.errmess,s,sizeof(e.errmess)); memset(&r,0,sizeof(r)); r.r[0]=PTR(&e); r.r[1]=1; r.r[2]=PTR("BitSkim 0.1"); call(Wimp_ReportError,&r);
}
static unsigned long ticks(void) { _kernel_swi_regs r; memset(&r,0,sizeof(r)); if(!call(OS_ReadMonotonicTime,&r)) return 0; return (unsigned long)r.r[0]; }
static int native_to_unix(const char *s,char *out,size_t cap) { return __unixify_std(s,out,cap,-1)!=NULL; }
static const char *recent_native(int incoming){static char native[512];const char *folder=bs_last_folder(engine,incoming);if(!folder[0] || !__riscosify_std(folder,0,native,sizeof native,NULL))return prefs.folder;return native;}
static void icon_set(View *v,int id,const char *s) {
    int b[4]; _kernel_swi_regs r;
    if(id<0 || id>=v->count) return;
    if(s) copy(v->icons[id].text,s,sizeof(v->icons[id].text));
    b[0]=v->handle; b[1]=v->icons[id].id; b[2]=b[3]=0;
    memset(&r,0,sizeof(r)); r.r[1]=PTR(b); call(Wimp_SetIconState,&r);
}
static int make_icon(View *v,int x,int top,int width,int height,const char *text,int type) {
    int b[9],index=v->count; _kernel_swi_regs r;
    if(index>=MAX_ICONS) return -1;
    copy(v->icons[index].text,text,sizeof(v->icons[index].text));
    memset(b,0,sizeof(b)); b[0]=v->handle; b[1]=x; b[2]=-top-height; b[3]=x+width; b[4]=-top;
    b[5]=ICON_TEXT|ICON_VCENTRE|ICON_INDIRECT|(foregrounds[prefs.theme]<<24)|(bases[prefs.theme]<<28);
    if(type) b[5]|=ICON_BORDER|ICON_FILLED|type;
    if(type==BUTTON) b[5]|=ICON_HCENTRE;
    b[6]=PTR(v->icons[index].text); b[7]=PTR(""); b[8]=sizeof(v->icons[index].text);
    memset(&r,0,sizeof(r)); r.r[1]=PTR(b);
    if(!call(Wimp_CreateIcon,&r)) return -1;
    v->icons[index].id=r.r[0]; v->count++; return index;
}
static int make_window(View *v,const char *title,int width,int height) {
    int b[22]; unsigned char *c=(unsigned char *)b; _kernel_swi_regs r;
    memset(v,0,sizeof(*v)); v->handle=-1; memset(b,0,sizeof(b));
    b[7]=(int)0x87002012UL; /* New-style back/close/title, moveable, fully Wimp drawn. */
    c[32]=7; c[33]=1; c[34]=(unsigned char)foregrounds[prefs.theme]; c[35]=(unsigned char)bases[prefs.theme]; c[36]=1; c[37]=3; c[38]=12;
    b[10]=0; b[11]=-height; b[12]=width; b[13]=0;
    b[14]=ICON_TEXT|ICON_HCENTRE|ICON_VCENTRE|ICON_INDIRECT;
    b[16]=1; b[17]=(height<<16)|width; b[18]=PTR(title); b[19]=PTR(""); b[20]=strlen(title)+1;
    memset(&r,0,sizeof(r)); r.r[1]=PTR(b); if(!call(Wimp_CreateWindow,&r)) return 0; v->handle=r.r[0]; return 1;
}
static void open_window(View *v,int width,int height) {
    int b[8]; _kernel_swi_regs r;
    b[0]=v->handle; b[1]=32; b[2]=160; b[3]=32+width; b[4]=160+height; b[5]=b[6]=0; b[7]=-1;
    memset(&r,0,sizeof(r)); r.r[1]=PTR(b); call(Wimp_OpenWindow,&r);
}
static void close_window(View *v) { int b=v->handle; _kernel_swi_regs r; memset(&r,0,sizeof(r)); r.r[1]=PTR(&b); call(Wimp_CloseWindow,&r); }
static void delete_window(View *v) { int b=v->handle; _kernel_swi_regs r; if(b<0) return; memset(&r,0,sizeof(r)); r.r[1]=PTR(&b); call(Wimp_DeleteWindow,&r); v->handle=-1; }
static void preferences_load(void) {
    FILE *f; char line[600],*value; int i;
    memset(&prefs,0,sizeof(prefs)); prefs.visible=prefs.receiving=prefs.ask=1; prefs.theme=3;
    bs_hostname(prefs.name,sizeof(prefs.name)); if(!prefs.name[0]) copy(prefs.name,"RISC OS",64);
    copy(prefs.folder,"<Wimp$ScrapDir>",512);
    {
        /* High bits of a xorshift generator; C library LCG low bits can cycle quickly. */
        unsigned long x=((unsigned long)time(NULL)^((unsigned long)ticks()<<11)^(unsigned long)(size_t)&prefs^0x9e3779b9UL)&0xffffffffUL;
        if(!x) x=0x2545f491UL;
        for(i=0;i<32;i++) { x^=(x<<13)&0xffffffffUL; x^=x>>17; x^=(x<<5)&0xffffffffUL; prefs.id[i]="0123456789abcdef"[(x>>28)&15]; }
        prefs.id[32]=0;
    }
    if(!native_to_unix("<BitSkim$Dir>.Choices",prefs_path,sizeof(prefs_path))) prefs_path[0]=0;
    f=prefs_path[0]?fopen(prefs_path,"r"):NULL; if(!f) return;
    while(fgets(line,sizeof(line),f)) {
        value=strchr(line,'='); if(!value) continue; *value++=0; value[strcspn(value,"\r\n")]=0;
        if(!strcmp(line,"name")) copy(prefs.name,value,64);
        else if(!strcmp(line,"folder")) copy(prefs.folder,value,512);
        else if(!strcmp(line,"id") && strlen(value)==32) { int valid=1; for(i=0;i<32;i++) if(!strchr("0123456789abcdef",value[i])) valid=0; if(valid) copy(prefs.id,value,33); }
        else if(!strcmp(line,"visible")) prefs.visible=atoi(value)!=0;
        else if(!strcmp(line,"receiving")) prefs.receiving=atoi(value)!=0;
        else if(!strcmp(line,"automatic")) prefs.automatic=atoi(value)!=0;
        else if(!strcmp(line,"ask")) prefs.ask=atoi(value)!=0;
        else if(!strcmp(line,"start")) prefs.start=atoi(value)!=0;
        else if(!strcmp(line,"close_iconify")) prefs.close_iconify=atoi(value)!=0;
        else if(!strcmp(line,"send_limit_kib")){if(!bs_limit_parse(value,&prefs.send_limit))prefs.send_limit=0;}
        else if(!strcmp(line,"receive_limit_kib")){if(!bs_limit_parse(value,&prefs.receive_limit))prefs.receive_limit=0;}
        else if(!strcmp(line,"bell"))prefs.bell=atoi(value)!=0;
        else if(!strcmp(line,"theme")) { i=atoi(value); if(i>=0 && i<8) prefs.theme=i; }
    }
    fclose(f);
}
static int preferences_write(void) {
    FILE *f=prefs_path[0]?fopen(prefs_path,"w"):NULL; int ok;
    if(!f) return 0;
    ok=fprintf(f,"name=%s\nid=%s\nfolder=%s\nvisible=%d\nreceiving=%d\nautomatic=%d\nask=%d\nstart=%d\nclose_iconify=%d\ntheme=%d\nbell=%d\nsend_limit_kib=%lu\nreceive_limit_kib=%lu\n",prefs.name,prefs.id,prefs.folder,prefs.visible,prefs.receiving,prefs.automatic,prefs.ask,prefs.start,prefs.close_iconify,prefs.theme,prefs.bell,prefs.send_limit/1024UL,prefs.receive_limit/1024UL)>=0;
    if(fclose(f)) ok=0; return ok;
}
static void main_create(void) {
    int i; make_window(&main_view,"BitSkim 0.1 - local file sharing",WIDTH,HEIGHT);
    make_icon(&main_view,20,12,1140,36,"NEARBY COMPUTERS - select a peer, then drop a Filer file",0);
    for(i=0;i<5;i++) peer_icons[i]=make_icon(&main_view,20,56+i*46,1060,40,"",BUTTON);
    make_icon(&main_view,1090,56,80,100,"Up",BUTTON); make_icon(&main_view,1090,176,80,100,"Down",BUTTON);
    make_icon(&main_view,20,302,180,36,"IP / IP:port",0); ip_icon=make_icon(&main_view,206,302,960,36,manual_ip,WRITABLE);
    make_icon(&main_view,20,348,180,36,"Send path",0); path_icon=make_icon(&main_view,206,348,960,36,send_path,WRITABLE);
    make_icon(&main_view,20,402,180,44,"Send file",BUTTON); make_icon(&main_view,212,402,170,44,"Refresh",BUTTON);
    make_icon(&main_view,394,402,220,44,"Preferences",BUTTON); make_icon(&main_view,626,402,170,44,"Cancel",BUTTON);
    make_icon(&main_view,808,402,170,44,"Iconify",BUTTON); make_icon(&main_view,990,402,180,44,"Quit",BUTTON);
    status_icon=make_icon(&main_view,20,464,1150,36,"Ready on your local network",0);
    filename_icon=make_icon(&main_view,20,506,1150,36,"",0); stats_icon=make_icon(&main_view,20,548,1150,36,"",0);
    bar_icon=make_icon(&main_view,20,590,1150,36,"",0);
    tools_button=make_icon(&main_view,20,642,170,36,"Tools...",BUTTON);
    make_icon(&main_view,200,642,780,36,"TCP / UDP 46440 - Unencrypted - sender names unverified",0);
}
static void hide_main(void) { if(iconbar<0) { report("The icon bar entry is unavailable; the window will stay open."); return; } copy(manual_ip,main_view.icons[ip_icon].text,sizeof(manual_ip)); copy(send_path,main_view.icons[path_icon].text,sizeof(send_path)); close_window(&main_view); hidden=1; bs_discovery_search(engine,bs_queue_count(engine)>0); }
static void show_main(void) { open_window(&main_view,WIDTH,HEIGHT); hidden=0; bs_discovery_search(engine,1); peers_dirty=1; }
static int send_file(const char *native) {
    char path[BS_PATH_MAX+1],address[64],*colon,*end; unsigned short port=BS_PORT; long p; const BSPeer *peer=NULL;
    if(bs_status(engine)->active || bs_queue_scanning(engine) || (bs_queue_count(engine) && !bs_queue_paused(engine))) {
        report("A transfer is already in progress."); return 0;
    }
    copy(address,main_view.icons[ip_icon].text,sizeof(address));
    if(address[0]) { colon=strchr(address,':'); if(colon) { *colon++=0; p=strtol(colon,&end,10); if(*end || p<1 || p>65535) { report("Invalid port."); return 0; } port=(unsigned short)p; } }
    else { int i;for(i=0;i<bs_peer_count(engine);i++){const BSPeer *candidate=bs_peer(engine,i);if(!strcmp(candidate->id,selected_id)){peer=candidate;break;}} if(!peer) { report("Select a computer or enter an IPv4 address."); return 0; } copy(address,peer->address,sizeof(address)); port=peer->port; }
    if(!native_to_unix(native,path,sizeof(path))) { report("Path cannot be converted or is too long."); return 0; }
    bs_queue_clear(engine);
    if(!(peer?bs_queue_peer(engine,peer->id,path):bs_queue_address(engine,address,port,path))){report(bs_last_error(engine));return 0;}bs_queue_continue(engine);return 1;
}
static void pref_labels(void) {
    const char *labels[]={"Visible on the network","Allow incoming transfers","Automatically accept offers","Always ask where to save","Start on the icon bar","Close window to icon bar","Completion sound"};
    int values[7],i; char text[100]; values[0]=draft.visible; values[1]=draft.receiving; values[2]=draft.automatic; values[3]=draft.ask; values[4]=draft.start; values[5]=draft.close_iconify;values[6]=draft.bell;
    for(i=0;i<7;i++) { sprintf(text,"[%c] %s",values[i]?'x':' ',labels[i]); icon_set(&pref_view,pref_checks[i],text); }
    icon_set(&pref_view,pref_theme,theme_names[draft.theme]);
}
static int clicked(View *v,int icon);
static void show_network(void){char value[32];if(network_view.handle>=0){open_window(&network_view,1000,340);return;}
    make_window(&network_view,"BitSkim Network",1000,340);
    make_icon(&network_view,20,20,950,36,"Transfer limits (KiB/s; 0 = Unlimited)",0);
    make_icon(&network_view,20,80,220,36,"Send",0);sprintf(value,"%lu",prefs.send_limit/1024UL);net_send=make_icon(&network_view,260,80,700,40,value,WRITABLE);
    make_icon(&network_view,20,140,220,36,"Receive",0);sprintf(value,"%lu",prefs.receive_limit/1024UL);net_receive=make_icon(&network_view,260,140,700,40,value,WRITABLE);
    make_icon(&network_view,20,204,950,36,"Next file only. Receive caps require an updated sender.",0);
    net_cancel=make_icon(&network_view,580,274,180,44,"Cancel",BUTTON);net_save=make_icon(&network_view,780,274,180,44,"Save",BUTTON);open_window(&network_view,1000,340);
}
static void click_network(int icon){int i=clicked(&network_view,icon);unsigned long up,down;Preferences old;
    if(i==net_cancel){delete_window(&network_view);return;}if(i!=net_save)return;
    if(!bs_limit_parse(network_view.icons[net_send].text,&up) || !bs_limit_parse(network_view.icons[net_receive].text,&down)){report("Enter 0..1048575 KiB/s; zero is Unlimited.");return;}
    old=prefs;prefs.send_limit=up;prefs.receive_limit=down;if(!preferences_write()){prefs=old;report("Cannot save preferences.");return;}
    draft.send_limit=up;draft.receive_limit=down;bs_limits_configure(engine,up,down);delete_window(&network_view);
}
static void show_preferences(void) {
    int i; if(pref_view.handle>=0) { open_window(&pref_view,1000,720); return; }
    draft=prefs; make_window(&pref_view,"BitSkim Preferences",1000,720);
    make_icon(&pref_view,20,16,150,36,"Name",0); pref_name=make_icon(&pref_view,180,16,790,36,draft.name,WRITABLE);
    for(i=0;i<6;i++) pref_checks[i]=make_icon(&pref_view,20,70+i*50,950,42,"",BUTTON);
    make_icon(&pref_view,20,382,150,36,"Folder",0); pref_folder=make_icon(&pref_view,180,382,790,36,draft.folder,WRITABLE);
    make_icon(&pref_view,20,434,150,36,"Theme",0); pref_theme=make_icon(&pref_view,180,434,790,42,"",BUTTON);
    pref_network=make_icon(&pref_view,20,492,950,36,"Network transfer limits...",BUTTON);
    pref_checks[6]=make_icon(&pref_view,20,534,950,36,"",BUTTON);
    make_icon(&pref_view,20,576,950,28,"https://github.com/retrocs-org/bitskim",0);
    make_icon(&pref_view,20,614,950,28,"Website: https://bitskim.retrocs.org",0);
    pref_cancel=make_icon(&pref_view,410,660,170,44,"Cancel",BUTTON); pref_revert=make_icon(&pref_view,600,660,170,44,"Revert",BUTTON); pref_save=make_icon(&pref_view,790,660,180,44,"Save",BUTTON);
    pref_labels(); open_window(&pref_view,1000,720);
}
static int destination(char *out,const char *wire) {
    char local[256],leaf[11]; int i;
    if(!bs_suggest_name(wire,0,local,sizeof(local))) return 0;
    for(i=0;i<10 && local[i];i++) leaf[i]=local[i]=='.'?'/':local[i]; leaf[i]=0;
    if(!leaf[0]) return 0;
    if(strlen(recent_native(1))+strlen(leaf)+2>=512) return 0;
    sprintf(out,"%s.%s",recent_native(1),leaf); return 1;
}
static int accept_path(const char *native) {
    char path[BS_PATH_MAX+1];return native_to_unix(native,path,sizeof path)?bs_accept_conflict(engine,path):0;
}
static void incoming(void) {
    const BSStatus *s=bs_status(engine); char native[512],label[512],name[256];
    if(!s->awaiting_decision || offer_open) return;
    if((bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)) {
        int index=bs_batch_index(engine),count=bs_batch_count(engine),row=0;char folder[BS_PATH_MAX+1],save_name[256];
        if(prefs.automatic && !prefs.ask) {
            if(!native_to_unix(prefs.folder,folder,sizeof folder) || !bs_accept_batch(engine,folder)){bs_decline(engine);report(bs_last_error(engine));}return;
        }
        show_main();make_window(&offer_view,"Incoming batch - BitSkim",1100,560);offer_open=1;offer_top=index;
        bs_batch_summary(engine,label,sizeof label);make_icon(&offer_view,20,16,1060,36,label,0);
        for(row=0;row<8;row++)offer_rows[row]=make_icon(&offer_view,20,60+row*28,1060,26,"",0);
        for(row=0;row<8 && index+row<count;row++) {
            const BSBatchItem *item=bs_batch_item(engine,index+row);if(!bs_batch_save_name(engine,index+row,save_name,sizeof save_name))strcpy(save_name,"Received");
            if(!bs_to_local(item->filename,name,sizeof name))strcpy(name,"Unicode filename");sprintf(label,"%.42s -> %.10s  %lu B",name,save_name,item->total);icon_set(&offer_view,offer_rows[row],label);
        }
        if(bs_batch_is_tree(engine))tree_offer_rows();
        offer_space=make_icon(&offer_view,20,286,1060,28,"",0);
        offer_previous=make_icon(&offer_view,20,324,200,44,"Previous",BUTTON);offer_next=make_icon(&offer_view,240,324,200,44,"Next",BUTTON);
        make_icon(&offer_view,20,384,1060,30,"Save all in folder (Tools conflict policy applies):",0);
        offer_path=make_icon(&offer_view,20,424,1060,42,recent_native(1),WRITABLE);
        offer_conflict=make_icon(&offer_view,20,490,400,44,bs_conflict_get(engine)==2?"Existing: Replace":bs_conflict_get(engine)==1?"Existing: Skip":"Existing: Rename",BUTTON);
        offer_decline=make_icon(&offer_view,660,490,190,44,"Decline",BUTTON);offer_accept=make_icon(&offer_view,870,490,210,44,"Accept all",BUTTON);
        open_window(&offer_view,1100,560);return;
    }
    if(!destination(native,s->filename)) { bs_decline(engine); report("Rename this file on the sender: this client requires Latin-1 filenames."); return; }
    if(prefs.automatic && !prefs.ask) { if(!accept_path(native)) { report(bs_last_error(engine)); bs_decline(engine); } return; }
    show_main(); make_window(&offer_view,"Incoming file - BitSkim",1100,390); offer_open=1;
    if(!bs_to_local(s->peer_name,name,sizeof(name))) copy(name,"Remote computer",sizeof(name));
    sprintf(label,"%.63s wants to send %lu bytes",name,s->total); make_icon(&offer_view,20,20,1060,36,label,0);
    if(!bs_to_local(s->filename,offer_filename,sizeof(offer_filename))) copy(offer_filename,"File",sizeof(offer_filename));
    make_icon(&offer_view,20,68,1060,36,offer_filename,0);
    make_icon(&offer_view,20,116,1060,36,"Unencrypted; sender names are not verified.",0);
    make_icon(&offer_view,20,164,1060,36,"Save path (old FileCore requires a leaf of at most 10 characters):",0);
    offer_path=make_icon(&offer_view,20,210,1060,42,native,WRITABLE);
    offer_space=make_icon(&offer_view,20,266,1060,28,"",0);
    offer_conflict=make_icon(&offer_view,20,306,400,44,bs_conflict_get(engine)==2?"Existing: Replace":bs_conflict_get(engine)==1?"Existing: Skip":"Existing: Rename",BUTTON);
    offer_decline=make_icon(&offer_view,660,306,190,44,"Decline",BUTTON); offer_accept=make_icon(&offer_view,870,306,210,44,"Accept",BUTTON);
    open_window(&offer_view,1100,390);
}
static void callback(void *unused,BSEvent event) { (void)unused; if(event==BS_EVENT_PEERS) peers_dirty=1; }
static void update(void) {
    const BSStatus *s=bs_status(engine); char text[512],local[256]; int i,n; unsigned long rate,percent;
    if(offer_open && !s->awaiting_decision) { delete_window(&offer_view); offer_open=0; }
    if(offer_open && ticks()/100!=offer_space_clock){char path[BS_PATH_MAX+1],info[256];offer_space_clock=ticks()/100;if(native_to_unix(offer_view.icons[offer_path].text,path,sizeof path)){bs_destination_space_report(engine,path,(bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0),info,sizeof info);icon_set(&offer_view,offer_space,info);}}
    if(hidden) return;
    if(peers_dirty) {
        int count=bs_peer_count(engine); selected=-1;
        if(selected_id[0]) { for(i=0;i<count;i++) if(!strcmp(bs_peer(engine,i)->id,selected_id)) { selected=i; break; } if(selected<0) selected_id[0]=0; }
        if(offset>count-5) offset=count>5?count-5:0;
        for(i=0;i<5;i++) { const BSPeer *p=bs_peer(engine,offset+i); text[0]=0;
            if(p) { if(!bs_to_local(p->name,local,sizeof(local))) copy(local,"Unicode peer",sizeof(local)); sprintf(text,"%c %.28s  [%.12s]  %s",selected==offset+i?'>':' ',local,p->platform,p->address); }
            icon_set(&main_view,peer_icons[i],text);
        } peers_dirty=0;
    }
    if(ticks()-last_ui<20) return; last_ui=ticks();
    {char state[350];sprintf(state,"%.255s | Waiting: %d%s",s->message,(s->active && s->incoming && (bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)?bs_batch_count(engine)-bs_batch_index(engine)-1:bs_queue_waiting(engine)),bs_queue_paused(engine)?(s->active?" - stops after file":" paused"):"");icon_set(&main_view,status_icon,state);}
    if(!bs_to_local(s->filename,local,sizeof(local))) copy(local,"Unicode filename",sizeof(local));
    text[0]=0;if(s->active) {unsigned long position=1,total=1;
        if(!s->incoming && bs_queue_count(engine))bs_queue_progress(engine,&position,&total);
        if(s->incoming && (bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0))sprintf(text,"Receiving file %d/%d: %.230s",bs_batch_index(engine)+1,bs_batch_count(engine),local);else if(s->incoming)sprintf(text,"Receiving: %.240s",local);else sprintf(text,"Sending file %lu/%lu: %.240s",position,total,local);}
    icon_set(&main_view,filename_icon,text);
    text[0]=0;
    if(s->active) { rate=bs_transfer_rate(engine); percent=s->total?((unsigned long)((unsigned long long)s->transferred*100/s->total)):0; if(s->total && s->transferred==s->total) percent=100;
        sprintf(text,"%lu / %lu bytes | %lu%% | %lu left | %lu B/s",s->transferred,s->total,percent,s->total-s->transferred,rate); }
    bs_progress_summary(engine,text,sizeof text);icon_set(&main_view,stats_icon,text);
    text[0]=0; if(s->active) { n=s->total?(int)(s->transferred/(s->total/40+1)):0; if(n>40) n=40; for(i=0;i<40;i++) text[i]=i<n?'=':'-'; text[40]=0; }
    icon_set(&main_view,bar_icon,text);
}
static int clicked(View *v,int icon) { int i; for(i=0;i<v->count;i++) if(v->icons[i].id==icon) return i; return -1; }
static void tools_update(void){int i;char line[81];const char *policy=bs_conflict_get(engine)==2?"Replace":bs_conflict_get(engine)==1?"Skip":"Rename";
    if(tools_mode>=0 && tools_serial){tools_mode=bs_history_find(engine,tools_serial);if(tools_mode<0)tools_mode=-2;}
    for(i=0;i<10;i++){bs_tools_line(engine,tools_mode,tools_top+i,line,sizeof line);icon_set(&tools_view,tools_lines[i],line);}icon_set(&tools_view,tools_buttons[4],policy);
}
static void show_tools(void){int i;if(tools_view.handle>=0){open_window(&tools_view,1160,620);return;}make_window(&tools_view,"BitSkim transfer tools",1160,620);for(i=0;i<10;i++)tools_lines[i]=make_icon(&tools_view,20,28+i*42,1100,36,"",0);
    tools_buttons[0]=make_icon(&tools_view,20,466,170,42,"Diagnostics",BUTTON);tools_buttons[1]=make_icon(&tools_view,210,466,150,42,"History",BUTTON);tools_buttons[2]=make_icon(&tools_view,380,466,150,42,"Older",BUTTON);tools_buttons[3]=make_icon(&tools_view,550,466,150,42,"Retry",BUTTON);tools_buttons[4]=make_icon(&tools_view,720,466,150,42,"Rename",BUTTON);tools_buttons[5]=make_icon(&tools_view,890,466,210,42,"Next page",BUTTON);
    tools_buttons[6]=make_icon(&tools_view,20,570,240,36,"Test connection",BUTTON);
    make_icon(&tools_view,20,530,1100,36,"History is session-only. Conflict policy applies to remaining collection files.",0);tools_update();open_window(&tools_view,1160,620);
}
static void click_tools(int icon){int id=clicked(&tools_view,icon);BSHistoryItem h;
    if(id==tools_buttons[6]){if(!bs_network_test(engine,main_view.icons[ip_icon].text,0))report(bs_last_error(engine));tools_mode=-1;tools_top=0;}
    else if(id==tools_buttons[0]){tools_mode=-1;tools_top=0;}else if(id==tools_buttons[1]){tools_mode=0;tools_top=0;if(bs_history_item(engine,0,&h))tools_serial=h.serial;}
    else if(id==tools_buttons[2]){if(tools_mode+1<bs_history_count(engine))tools_mode++;else tools_mode=0;tools_top=0;if(bs_history_item(engine,tools_mode,&h))tools_serial=h.serial;}
    else if(id==tools_buttons[3]){int i;for(i=0;i<bs_history_count(engine);i++)if(bs_history_item(engine,i,&h) && h.serial==tools_serial){if(!bs_retry_history(engine,i))report(bs_last_error(engine));break;}}
    else if(id==tools_buttons[4])bs_conflict_policy(engine,(bs_conflict_get(engine)+1)%3);else if(id==tools_buttons[5]){char line[81];if(bs_tools_line(engine,tools_mode,tools_top+10,line,sizeof line))tools_top+=10;else tools_top=0;}tools_update();
}
static void click_main(int icon) {
    int i=clicked(&main_view,icon);
    if(i==tools_button)show_tools();
    else if(i>=1 && i<=5 && offset+i-1<bs_peer_count(engine)) { selected=offset+i-1; copy(selected_id,bs_peer(engine,selected)->id,sizeof(selected_id)); icon_set(&main_view,ip_icon,""); peers_dirty=1; }
    else if(i==6) { if(offset>0) offset--; peers_dirty=1; }
    else if(i==7) { if(offset+5<bs_peer_count(engine)) offset++; peers_dirty=1; }
    else if(i==12) send_file(main_view.icons[path_icon].text);
    else if(i==13) bs_refresh(engine);
    else if(i==14) show_preferences();
    else if(i==15) bs_cancel(engine);
    else if(i==16) hide_main();
    else if(i==17) running=0;
}
static void click_preferences(int icon) {
    int i=clicked(&pref_view,icon),j; char wire[64]; Preferences previous;
    for(j=0;j<7;j++) if(i==pref_checks[j]) { int *value=j==0?&draft.visible:j==1?&draft.receiving:j==2?&draft.automatic:j==3?&draft.ask:j==4?&draft.start:j==5?&draft.close_iconify:&draft.bell; *value=!*value; pref_labels(); return; }
    if(i==pref_network){show_network();return;}
    if(i==pref_theme) { draft.theme=(draft.theme+1)%8; pref_labels(); }
    else if(i==pref_cancel) delete_window(&pref_view);
    else if(i==pref_revert) { draft=prefs; icon_set(&pref_view,pref_name,draft.name); icon_set(&pref_view,pref_folder,draft.folder); pref_labels(); }
    else if(i==pref_save) {
        if(strlen(pref_view.icons[pref_name].text)>=64) { report("Client name is too long."); return; }
        copy(draft.name,pref_view.icons[pref_name].text,64); copy(draft.folder,pref_view.icons[pref_folder].text,512);
        if(!draft.name[0] || !bs_to_utf8(draft.name,wire,sizeof(wire))) { report("Use a nonempty client name of at most 63 UTF-8 bytes."); return; }
        previous=prefs; prefs=draft; if(!preferences_write()) { prefs=previous; report("Cannot save Choices inside !BitSkim. Install it in a writable directory."); return; }
        bs_configure(engine,wire,prefs.visible,prefs.receiving);bs_limits_configure(engine,prefs.send_limit,prefs.receive_limit); delete_window(&pref_view);
        copy(manual_ip,main_view.icons[ip_icon].text,sizeof(manual_ip)); copy(send_path,main_view.icons[path_icon].text,sizeof(send_path)); delete_window(&main_view); main_create(); if(!hidden) show_main();
    }
}
static void tree_offer_rows(void){int row;char name[256],label[66];for(row=0;row<8;row++){int index=offer_top+row/4;label[0]=0;if(index<bs_batch_count(engine) && bs_batch_save_name(engine,index,name,sizeof name)){size_t at=(size_t)(row%4)*65,n=strlen(name);if(at<n){n-=at;if(n>65)n=65;memcpy(label,name+at,n);label[n]=0;}}icon_set(&offer_view,offer_rows[row],label);}}
static void page_offer(int direction){int row,index;char label[512],name[256],save_name[256];
    if(direction<0 && offer_top>bs_batch_index(engine))offer_top-=bs_batch_is_tree(engine)?2:8;
    if(direction>0 && offer_top+(bs_batch_is_tree(engine)?2:8)<bs_batch_count(engine))offer_top+=bs_batch_is_tree(engine)?2:8;
    if(bs_batch_is_tree(engine)){tree_offer_rows();return;}
    for(row=0;row<8;row++){index=offer_top+row;label[0]=0;if(index<bs_batch_count(engine)){const BSBatchItem *item=bs_batch_item(engine,index);bs_batch_save_name(engine,index,save_name,sizeof save_name);if(!bs_to_local(item->filename,name,sizeof name))strcpy(name,"Unicode filename");sprintf(label,"%.42s -> %.10s  %lu B",name,save_name,item->total);}icon_set(&offer_view,offer_rows[row],label);}
}
static void message(int *b) {
    _kernel_swi_regs r;int sender=b[1],accepted=0,i;const char *name=(const char *)b+44;
    if(b[4]==0) {running=0;return;}
    /* DataLoad with no your_ref is a durable Filer object, not temporary scrap. */
    if(b[4]!=3 || b[3]!=0 || b[0]<48 || b[0]>256 || !memchr(name,0,(size_t)b[0]-44) || !*name)return;
    if(b[5]==main_view.handle || (b[5]==-2 && iconbar>=0 && b[6]==iconbar)) {
        if(b[5]==-2)show_main();
        else for(i=0;i<5;i++)if(b[6]==main_view.icons[peer_icons[i]].id){const BSPeer *peer=bs_peer(engine,offset+i);if(!peer)return;copy(selected_id,peer->id,sizeof selected_id);icon_set(&main_view,ip_icon,"");peers_dirty=1;break;}
        accepted=send_file(name);
    }else if((pref_view.handle>=0 && b[5]==pref_view.handle && b[6]==pref_view.icons[pref_folder].id) || (offer_open && b[5]==offer_view.handle && b[6]==offer_view.icons[offer_path].id && (bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0))){
        char path[BS_PATH_MAX+1];struct stat st;if(native_to_unix(name,path,sizeof path) && !stat(path,&st) && S_ISDIR(st.st_mode)){if(b[5]==pref_view.handle)icon_set(&pref_view,pref_folder,name);else icon_set(&offer_view,offer_path,name);accepted=1;}else report("Drop a Filer directory into this folder field.");
    }
    if(accepted){b[3]=b[2];b[4]=4;memset(&r,0,sizeof r);r.r[0]=17;r.r[1]=PTR(b);r.r[2]=sender;call(Wimp_SendMessage,&r);}
}
int main(void) {
    _kernel_swi_regs r; int messages[]={3,0},b[64],reason,icon[9],i; char name[64]; BSConfig config;
    main_view.handle=pref_view.handle=offer_view.handle=tools_view.handle=network_view.handle=-1;
    memset(&r,0,sizeof(r)); r.r[0]=310; r.r[1]=0x4b534154; r.r[2]=PTR("BitSkim"); r.r[3]=PTR(messages);
    if(!call(Wimp_Initialise,&r)) return 1; task=r.r[1];
    if(!bs_platform_init()) { report("A full Internet-module TCP/IP stack is required."); return 1; }
    preferences_load(); if(!bs_to_utf8(prefs.name,name,sizeof(name))) copy(name,"RISC OS",sizeof(name));
    memset(&config,0,sizeof(config)); copy(config.name,name,64); copy(config.platform,"RISC OS",32); copy(config.id,prefs.id,33); config.port=config.discovery_port=BS_PORT; config.visible=prefs.visible; config.receiving=prefs.receiving;
    engine=bs_create(&config,callback,NULL);
    if(!engine) { report("Cannot open TCP/UDP port 46440. Check the Internet stack and close other BitSkim instances."); goto done; }
    {char store[BS_PATH_MAX+1];if(native_to_unix("<BitSkim$Dir>.Folders",store,sizeof store))bs_folders_configure(engine,store);}
    bs_limits_configure(engine,prefs.send_limit,prefs.receive_limit);bs_batch_enable(engine,1);
    main_create();
    memset(icon,0,sizeof(icon)); icon[0]=-1; icon[1]=0; icon[2]=0; icon[3]=72; icon[4]=72; icon[5]=2|ICON_HCENTRE|ICON_VCENTRE|ICON_INDIRECT|BUTTON; icon[6]=PTR(icon_name); icon[7]=1; icon[8]=sizeof(icon_name);
    memset(&r,0,sizeof(r)); r.r[1]=PTR(icon); if(call(Wimp_CreateIcon,&r)) iconbar=r.r[0];
    if(prefs.start && iconbar>=0) hide_main(); else show_main();
    while(running) {
        bs_tick(engine);
        if(bs_completion_notice(engine,&bell_serial) && prefs.bell){memset(&r,0,sizeof r);r.r[0]=7;call(OS_WriteC,&r);}
        update(); incoming();
        if(tools_view.handle>=0){static unsigned long checked;unsigned long now=ticks();if(now-checked>=100){checked=now;tools_update();}}
        memset(&r,0,sizeof(r)); r.r[0]=0; r.r[1]=PTR(b); r.r[2]=(int)(ticks()+(bs_status(engine)->active?2:hidden?10:5));
        if(!call(Wimp_PollIdle,&r)) { report("Wimp polling failed."); break; } reason=r.r[0];
        if(reason==1) { memset(&r,0,sizeof(r)); r.r[1]=PTR(b); if(call(Wimp_RedrawWindow,&r)) while(r.r[0]) { r.r[1]=PTR(b); if(!call(Wimp_GetRectangle,&r)) break; } }
        else if(reason==2) { memset(&r,0,sizeof(r)); r.r[1]=PTR(b); call(Wimp_OpenWindow,&r); }
        else if(reason==3) {
            if(b[0]==main_view.handle) { if(prefs.close_iconify && iconbar>=0) hide_main(); else running=0; }
            else if(b[0]==pref_view.handle) delete_window(&pref_view);
            else if(b[0]==network_view.handle)delete_window(&network_view);
            else if(b[0]==tools_view.handle)delete_window(&tools_view);
            else if(b[0]==offer_view.handle) { bs_decline(engine); delete_window(&offer_view); offer_open=0; }
        } else if(reason==6) {
            if(b[3]==-2 && b[4]==iconbar) { if(b[2]&2) show_preferences(); else show_main(); }
            else if(b[3]==main_view.handle) click_main(b[4]);
            else if(b[3]==pref_view.handle) click_preferences(b[4]);
            else if(b[3]==network_view.handle)click_network(b[4]);
            else if(b[3]==tools_view.handle)click_tools(b[4]);
            else if(b[3]==offer_view.handle) { i=clicked(&offer_view,b[4]); if(i==offer_conflict){bs_conflict_policy(engine,(bs_conflict_get(engine)+1)%3);icon_set(&offer_view,offer_conflict,bs_conflict_get(engine)==2?"Existing: Replace":bs_conflict_get(engine)==1?"Existing: Skip":"Existing: Rename");}else if((bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0) && (i==offer_previous || i==offer_next))page_offer(i==offer_previous?-1:1);else if(i==offer_decline) { bs_decline(engine); delete_window(&offer_view); offer_open=0; } else if(i==offer_accept) { char batch_folder[BS_PATH_MAX+1];int accepted=(bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)?(native_to_unix(offer_view.icons[offer_path].text,batch_folder,sizeof batch_folder) && bs_accept_batch(engine,batch_folder)):accept_path(offer_view.icons[offer_path].text);if(!accepted) report(bs_last_error(engine)); else { delete_window(&offer_view); offer_open=0; } } }
        } else if(reason==8) { memset(&r,0,sizeof(r)); r.r[0]=b[6]; call(Wimp_ProcessKey,&r); }
        else if(reason==17 || reason==18) message(b);
    }
done:
    if(engine) bs_destroy(engine); bs_platform_cleanup();
    delete_window(&tools_view);delete_window(&network_view);delete_window(&offer_view); delete_window(&pref_view); delete_window(&main_view);
    memset(&r,0,sizeof(r)); r.r[0]=task; r.r[1]=0x4b534154; call(Wimp_CloseDown,&r); return 0;
}
