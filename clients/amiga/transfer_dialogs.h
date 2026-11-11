extern unsigned long bsp_seconds(void);
/* Modeless transfer dialogs: all core and ARexx work remains in the main task.
   Each dialog owns its screen lock, pens and gadgets independently of the main
   window, so SHOW/HIDE and theme changes cannot invalidate it. */
#include "send_target.h"
#include "receive_name.h"
#define DIALOG_OFFER 1
#define DIALOG_SEND 2
#define DIALOG_NOTICE 3
#define DIALOG_BATCH 4
#define DIALOG_TOOLS 5
typedef struct {
    struct Window *w; struct Screen *screen; APTR visual;
    struct Gadget *gadgets,*path; UIControl *controls;
    UWORD pens[7]; int obtained[7],mode,list_top,history_index;unsigned long space_time;
    unsigned long serial;
    GuiSendTarget target;
    char heading[128],detail[2048],error[256],space[256];
} TransferDialog;
static TransferDialog transfer_dialog;
static void dialog_draw(void) {
    TransferDialog *d=&transfer_dialog; struct RastPort *rp;
    if(!d->w) return;
    rp=d->w->RPort;
    SetFont(rp,control_font?control_font:d->screen->RastPort.Font);
    SetAPen(rp,d->pens[0]); RectFill(rp,d->w->BorderLeft,d->w->BorderTop,d->w->Width-d->w->BorderRight-1,d->w->Height-d->w->BorderBottom-1);
    SetDrMd(rp,JAM1); SetAPen(rp,d->pens[2]);
    Move(rp,16,44); Text(rp,(STRPTR)d->heading,strlen(d->heading)>69?69:strlen(d->heading));
    if(d->mode==DIALOG_TOOLS){int row;char line[70],label[80];
        if(d->history_index>=0 && d->serial){d->history_index=bs_history_find(client,d->serial);if(d->history_index<0)d->history_index=-2;}
        sprintf(label,"Transfer tools / %s / existing files: %s",d->history_index<0?"Diagnostics":"History",bs_conflict_get(client)==2?"Replace":bs_conflict_get(client)==1?"Skip":"Rename");
        SetAPen(rp,d->pens[0]);RectFill(rp,14,24,584,190);SetAPen(rp,d->pens[2]);Move(rp,16,36);Text(rp,(STRPTR)label,strlen(label));
        for(row=0;row<12;row++){bs_tools_line(client,d->history_index,d->list_top+row,line,sizeof line);Move(rp,16,52+row*11);Text(rp,(STRPTR)line,strlen(line));}return;
    }
    if(d->mode==DIALOG_BATCH) {
        int i,row=0;char name[256],original[256],line[100];const BSBatchItem *item;
        SetAPen(rp,d->pens[0]);RectFill(rp,14,24,584,194);SetAPen(rp,d->pens[2]);
        Move(rp,16,32);Text(rp,(STRPTR)d->heading,strlen(d->heading));
        for(i=d->list_top;i<bs_batch_count(client) && row<8;i++) {
            item=bs_batch_item(client,i);if(!item)break;
            if(!bs_batch_save_name(client,i,name,sizeof name))copy(name,"[unsupported name]",sizeof name);
            if(bs_batch_is_tree(client)){size_t at=0,len=strlen(name);int chunk;for(chunk=0;chunk<4;chunk++){size_t count=len-at;if(count>65)count=65;Move(rp,16,48+row*11);if(count)Text(rp,(STRPTR)(name+at),count);at+=count;row++;}continue;}
            if(!bs_to_local(item->filename,original,sizeof original))strcpy(original,"[Unicode name]");
            snprintf(line,sizeof line,"%d %.28s > %.28s",i+1,original,name);
            Move(rp,16,48+row*11);Text(rp,(STRPTR)line,strlen(line));row++;
        }
        SetAPen(rp,d->pens[3]);Move(rp,16,136);
        {char text[80];sprintf(text,"Unencrypted. Existing files: %s (Conflicts changes this).",bs_conflict_get(client)==2?"Replace":bs_conflict_get(client)==1?"Skip":"Rename");Text(rp,(STRPTR)text,strlen(text));}
        Move(rp,16,147);Text(rp,(STRPTR)"Save all in folder:",19);
        SetAPen(rp,d->pens[4]);Move(rp,16,185);{const char *info=d->error[0]?d->error:d->space;Text(rp,(STRPTR)info,strlen(info)>69?69:strlen(info));}return;
    }
    if(d->mode==DIALOG_NOTICE) {
        size_t at=0,length=strlen(d->detail); int row=0;
        while(at<length && row<4) { size_t count=length-at; if(count>69) count=69; Move(rp,16,66+row*22); Text(rp,(STRPTR)(d->detail+at),count); at+=count; row++; }
    } else { Move(rp,16,66); Text(rp,(STRPTR)d->detail,strlen(d->detail)>69?69:strlen(d->detail)); }
    SetAPen(rp,d->pens[3]); Move(rp,16,d->mode==DIALOG_NOTICE?154:88);
    { const char *s=d->mode==DIALOG_OFFER?"Unencrypted. Sender names are not verified.":d->mode==DIALOG_SEND?"Enter a file or folder path, or drop it into the main window.":"BitSkim continues running while this message is open."; Text(rp,(STRPTR)s,strlen(s)); }
    if(d->mode==DIALOG_OFFER){const char *policy=bs_conflict_get(client)==2?"Existing: Replace":bs_conflict_get(client)==1?"Existing: Skip":"Existing: Rename";Move(rp,16,177);Text(rp,(STRPTR)policy,strlen(policy));}
    if(d->mode!=DIALOG_NOTICE) { Move(rp,16,115); Text(rp,(STRPTR)(d->mode==DIALOG_OFFER?"Save as (full path)":"Path (full path)"),d->mode==DIALOG_OFFER?19:16); }
    SetAPen(rp,d->pens[4]); Move(rp,16,166);{const char *info=d->error[0]?d->error:d->space;Text(rp,(STRPTR)info,strlen(info)>69?69:strlen(info));}
}
static void dialog_close(void) {
    TransferDialog *d=&transfer_dialog; struct Screen *saved_screen=screen; UIControl *saved_controls=ui_controls; struct IntuiMessage *m;
    if(d->w) { while((m=GT_GetIMsg(d->w->UserPort))) GT_ReplyIMsg(m); CloseWindow(d->w); d->w=NULL; }
    ui_controls=d->controls; ui_controls_free(&d->gadgets); ui_controls=saved_controls;
    if(d->gadgets) FreeGadgets(d->gadgets);
    if(d->visual) FreeVisualInfo(d->visual);
    screen=d->screen; if(screen) { ui_palette_release(d->pens,d->obtained); UnlockPubScreen(NULL,screen); } screen=saved_screen;
    memset(d,0,sizeof(*d));
}
static int dialog_open(int mode,const char *heading,const char *detail,const char *path) {
    TransferDialog *d=&transfer_dialog; struct Screen *saved_screen=screen; APTR saved_visual=visual;
    UIControl *saved_controls=ui_controls; UWORD saved_pens[7]; struct Gadget *g; int ok=0;
    if(d->w) return 0;
    memset(d,0,sizeof(*d)); d->mode=mode; d->serial=bs_transfer_serial(client);d->list_top=bs_batch_index(client);
    copy(d->heading,heading,sizeof(d->heading)); copy(d->detail,detail,sizeof(d->detail));
    d->screen=LockPubScreen((STRPTR)"Workbench"); if(!d->screen) goto done;
    d->visual=GetVisualInfo(d->screen,TAG_DONE); if(!d->visual) goto done;
    memcpy(saved_pens,control_pens,sizeof(saved_pens)); screen=d->screen; visual=d->visual; ui_controls=NULL;
    ui_palette_get(theme_index,d->pens,d->obtained); memcpy(control_pens,d->pens,sizeof(control_pens));
    g=CreateContext(&d->gadgets);
    if(mode!=DIALOG_NOTICE && mode!=DIALOG_TOOLS) g=d->path=add(g,STRING_KIND,40,16,mode==DIALOG_BATCH?154:125,568,18,NULL,GTST_MaxChars,BS_PATH_MAX);
    if(g && mode!=DIALOG_NOTICE && mode!=DIALOG_TOOLS) g=add(g,BUTTON_KIND,41,348,(mode==DIALOG_BATCH || mode==DIALOG_TOOLS)?199:187,110,24,(mode==DIALOG_OFFER || mode==DIALOG_BATCH)?"Decline":"Cancel",TAG_IGNORE,0);
    if(g) g=add(g,BUTTON_KIND,42,470,(mode==DIALOG_BATCH || mode==DIALOG_TOOLS)?199:187,114,24,mode==DIALOG_BATCH?"Accept all":mode==DIALOG_OFFER?"Accept":mode==DIALOG_SEND?"Send file":"Close",TAG_IGNORE,0);
    if(g && mode==DIALOG_BATCH) g=add(g,BUTTON_KIND,43,16,199,110,24,"Previous",TAG_IGNORE,0);
    if(g && mode==DIALOG_BATCH) g=add(g,BUTTON_KIND,44,138,199,110,24,"Next",TAG_IGNORE,0);
    if(g && (mode==DIALOG_BATCH || mode==DIALOG_OFFER))g=add(g,BUTTON_KIND,45,mode==DIALOG_BATCH?258:16,mode==DIALOG_BATCH?199:187,mode==DIALOG_BATCH?82:110,24,"Conflicts",TAG_IGNORE,0);
    if(g && mode==DIALOG_TOOLS){
        g=add(g,BUTTON_KIND,43,16,199,80,24,"Page",TAG_IGNORE,0);
        if(g)g=add(g,BUTTON_KIND,44,102,199,86,24,"History",TAG_IGNORE,0);
        if(g)g=add(g,BUTTON_KIND,45,194,199,84,24,"Older",TAG_IGNORE,0);
        if(g)g=add(g,BUTTON_KIND,46,284,199,80,24,"Retry",TAG_IGNORE,0);
        if(g)g=add(g,BUTTON_KIND,47,370,199,88,24,"Conflicts",TAG_IGNORE,0);
        if(g)g=add(g,BUTTON_KIND,48,16,226,140,24,"Test network",TAG_IGNORE,0);
    }
    d->controls=ui_controls; ui_controls=saved_controls; memcpy(control_pens,saved_pens,sizeof(control_pens)); screen=saved_screen; visual=saved_visual;
    if(!g) goto done;
    d->w=OpenWindowTags(NULL,WA_PubScreen,d->screen,WA_Left,(d->screen->Width-600)/2,WA_Top,d->screen->BarHeight+(mode==DIALOG_BATCH?2:12),
        WA_Title,(mode==DIALOG_OFFER || mode==DIALOG_BATCH)?"Incoming file - BitSkim":mode==DIALOG_SEND?"Send file - BitSkim":"BitSkim",
        WA_Width,600,WA_Height,mode==DIALOG_TOOLS?258:mode==DIALOG_BATCH?232:224,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_CloseGadget,TRUE,WA_Activate,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW|STRINGIDCMP,TAG_DONE);
    if(!d->w) goto done;
    AddGList(d->w,d->gadgets,~0UL,-1,NULL); dialog_draw(); RefreshGList(d->gadgets,d->w,NULL,-1); GT_RefreshWindow(d->w,NULL);
    if(d->path) { GT_SetGadgetAttrs(d->path,d->w,NULL,GTST_String,path?path:"",TAG_DONE); memcpy(saved_pens,control_pens,sizeof(saved_pens)); memcpy(control_pens,d->pens,sizeof(control_pens)); ui_string_colors(d->path,d->w); memcpy(control_pens,saved_pens,sizeof(control_pens)); ActivateGadget(d->path,d->w,NULL); }
    ScreenToFront(d->screen); ok=1;
done:
    if(!ok) dialog_close();
    return ok;
}
static void dialog_error(const char *s) { copy(transfer_dialog.error,s,sizeof(transfer_dialog.error)); if(transfer_dialog.w) { dialog_draw(); RefreshGList(transfer_dialog.gadgets,transfer_dialog.w,NULL,-1); } }
static void client_notice(const char *s) {
    if(transfer_dialog.w) dialog_error(s);
    else if(!dialog_open(DIALOG_NOTICE,"BitSkim",s,NULL)) { DisplayBeep(NULL); fprintf(stderr,"BitSkim: %s\n",s); }
}
static void dialog_submit(void) {
    TransferDialog *d=&transfer_dialog; char path[BS_PATH_MAX+1],destination[BS_PATH_MAX+1];
    if(d->mode==DIALOG_NOTICE || d->mode==DIALOG_TOOLS) { dialog_close(); return; }
    copy(path,((struct StringInfo *)d->path->SpecialInfo)->Buffer,sizeof(path));
    if(!path[0]) { dialog_error("Enter the full file or directory path."); return; }
    if(d->mode==DIALOG_BATCH) {
        if(!bs_status(client)->awaiting_decision || bs_transfer_serial(client)!=d->serial) {dialog_close();return;}
        if(!bs_accept_batch(client,path)) {dialog_error(bs_last_error(client));return;}
    } else if(d->mode==DIALOG_SEND) {
        if(!prepare_single_send(client)) { dialog_error("A transfer is already in progress."); return; }
        if(!gui_send(client,&d->target,path)) { dialog_error(bs_last_error(client)); return; }
    } else {
        if(!bs_status(client)->awaiting_decision || bs_transfer_serial(client)!=d->serial) { dialog_close(); return; }
        copy(destination,path,sizeof destination);
        if(!bs_accept_conflict(client,destination)) { dialog_error(bs_last_error(client)); return; }
    }
    dialog_close();
}
static void begin_send_file(const char *path) {
    GuiSendTarget target; char recipient[160],local[64]; const BSPeer *peer; int i;
    if(window && ipg) copy(manual_ip,((struct StringInfo *)ipg->SpecialInfo)->Buffer,sizeof(manual_ip));
    if(!gui_target(&target,selected_id,manual_ip)) { client_notice("Select a peer or enter a valid IP address and port."); return; }
    if(path && !prepare_single_send(client)) { client_notice("A transfer is already in progress."); return; }
    if(path) { if(transfer_dialog.mode==DIALOG_SEND) dialog_close(); if(!gui_send(client,&target,path)) client_notice(bs_last_error(client)); return; }
    if(transfer_dialog.w) { WindowToFront(transfer_dialog.w); ActivateWindow(transfer_dialog.w); return; }
    snprintf(recipient,sizeof(recipient),"To: %s:%u",target.address,target.port);
    if(target.id[0]) {
        snprintf(recipient,sizeof(recipient),"To peer ID: %s",target.id);
        for(i=0;i<bs_peer_count(client);i++) {
            peer=bs_peer(client,i); if(strcmp(peer->id,target.id)) continue;
            if(!bs_to_local(peer->name,local,sizeof(local))) copy(local,"Unicode peer",sizeof(local));
            snprintf(recipient,sizeof(recipient),"To: %.40s (%s:%u)",local,peer->address,peer->port); break;
        }
    }
    if(!dialog_open(DIALOG_SEND,"Choose a file or directory to send",recipient,"")) { client_notice("Could not open the send window."); return; }
    transfer_dialog.target=target;
    if(bs_last_folder(client,0)[0])GT_SetGadgetAttrs(transfer_dialog.path,transfer_dialog.w,NULL,GTST_String,bs_last_folder(client,0),TAG_DONE);
}
static void incoming(void) {
    const BSStatus *s=bs_status(client); char local_file[256],local_peer[64],original[BS_PATH_MAX+1],destination[BS_PATH_MAX+1],heading[128],failure[256]; int renamed;
    if(!s->awaiting_decision) return;
    if(transfer_dialog.w && (transfer_dialog.mode==DIALOG_OFFER || transfer_dialog.mode==DIALOG_BATCH) && transfer_dialog.serial==bs_transfer_serial(client)) return;
    if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)) {
        if(auto_accept && !ask_location) {
            if(!bs_accept_batch(client,download)) {copy(failure,bs_last_error(client),sizeof failure);bs_decline(client);client_notice(failure);}return;
        }
        if(iconified && !restore_ui()) {bs_decline(client);return;}
        if(transfer_dialog.w)dialog_close();
        if(!bs_to_local(s->peer_name,local_peer,sizeof local_peer))copy(local_peer,"Unicode peer",sizeof local_peer);
        {char summary[80];bs_batch_summary(client,summary,sizeof summary);snprintf(heading,sizeof heading,"%.24s: %.67s",local_peer,summary);}
        if(!dialog_open(DIALOG_BATCH,heading,"",bs_last_folder(client,1)[0]?bs_last_folder(client,1):download)) {bs_decline(client);client_notice("Could not open batch offer.");return;}
        DisplayBeep(transfer_dialog.screen);return;
    }
    renamed=receive_name(s->filename,local_file);
    copy(original,bs_last_folder(client,1)[0]?bs_last_folder(client,1):download,sizeof(original));
    if(!AddPart((STRPTR)original,(STRPTR)local_file,sizeof(original))) { bs_decline(client); client_notice("Destination path is too long."); return; }
    if(auto_accept && !ask_location && !renamed) {
        if(strlen(original)>=sizeof destination) { bs_decline(client); client_notice("Cannot choose an unused destination name."); }
        else if((copy(destination,original,sizeof destination),!bs_accept_conflict(client,destination))) { copy(failure,bs_last_error(client),sizeof(failure)); bs_decline(client); client_notice(failure); }
        return;
    }
    if(iconified && !restore_ui()) { bs_decline(client); return; }
    if(transfer_dialog.w) dialog_close();
    if(!bs_to_local(s->peer_name,local_peer,sizeof(local_peer))) copy(local_peer,"Unicode peer",sizeof(local_peer));
    snprintf(heading,sizeof(heading),"%.40s offers %lu bytes",local_peer,s->total);
    if(!dialog_open(DIALOG_OFFER,heading,local_file,original)) { bs_decline(client); client_notice("Could not open the incoming file window."); return; }
    if(renamed) dialog_error("Name shortened or replaced for Amiga. Edit Save as, then Accept.");
    DisplayBeep(transfer_dialog.screen);
}
static void dialog_tick(void) {
    TransferDialog *d=&transfer_dialog; struct IntuiMessage *m; ULONG cls; UWORD id; int action=0;
    if(!d->w) return;
    if(d->mode==DIALOG_TOOLS && bsp_seconds()!=d->space_time){static char prior[2048];char report[2048];d->space_time=bsp_seconds();bs_diagnostics(client,report,sizeof report);if(strcmp(prior,report)){copy(prior,report,sizeof prior);dialog_draw();}}
    if((d->mode==DIALOG_OFFER || d->mode==DIALOG_BATCH) && d->path && bsp_seconds()!=d->space_time){d->space_time=bsp_seconds();bs_destination_space_report(client,((struct StringInfo *)d->path->SpecialInfo)->Buffer,d->mode==DIALOG_BATCH,d->space,sizeof d->space);dialog_draw();}
    if((d->mode==DIALOG_OFFER || d->mode==DIALOG_BATCH) && (!bs_status(client)->awaiting_decision || bs_transfer_serial(client)!=d->serial)) { dialog_close(); return; }
    while((m=GT_GetIMsg(d->w->UserPort))) {
        cls=m->Class; id=(cls==IDCMP_GADGETUP && m->IAddress)?((struct Gadget *)m->IAddress)->GadgetID:0;
        GT_ReplyIMsg(m);
        if(cls==IDCMP_CLOSEWINDOW || (cls==IDCMP_GADGETUP && id==41)) action=1;
        else if(!action && cls==IDCMP_GADGETUP && id==42) action=2;
        else if(cls==IDCMP_GADGETUP && d->mode==DIALOG_BATCH && (id==43 || id==44)) {
            if(id==43 && d->list_top>bs_batch_index(client))d->list_top-=bs_batch_is_tree(client)?2:8;
            if(id==44 && d->list_top+(bs_batch_is_tree(client)?2:8)<bs_batch_count(client))d->list_top+=bs_batch_is_tree(client)?2:8;
            dialog_draw();RefreshGList(d->gadgets,d->w,NULL,-1);
        }
        else if(cls==IDCMP_GADGETUP && id==45 && (d->mode==DIALOG_BATCH || d->mode==DIALOG_OFFER)){bs_conflict_policy(client,(bs_conflict_get(client)+1)%3);dialog_draw();RefreshGList(d->gadgets,d->w,NULL,-1);}
        else if(cls==IDCMP_GADGETUP && d->mode==DIALOG_TOOLS){BSHistoryItem h;char line[70];
            if(id==43){if(bs_tools_line(client,d->history_index,d->list_top+12,line,sizeof line))d->list_top+=12;else d->list_top=0;}
            else if(id==44){d->history_index=d->history_index<0?0:-1;d->list_top=0;if(bs_history_item(client,d->history_index,&h))d->serial=h.serial;}
            else if(id==45){d->history_index++;if(d->history_index>=bs_history_count(client))d->history_index=0;d->list_top=0;if(bs_history_item(client,d->history_index,&h))d->serial=h.serial;}
            else if(id==46){int i;for(i=0;i<bs_history_count(client);i++)if(bs_history_item(client,i,&h) && h.serial==d->serial){if(!bs_retry_history(client,i))dialog_error(bs_last_error(client));break;}}
            else if(id==48){if(!bs_network_test(client,manual_ip[0]?manual_ip:NULL,0))dialog_error(bs_last_error(client));d->history_index=-1;d->list_top=0;}
            else if(id==47)bs_conflict_policy(client,(bs_conflict_get(client)+1)%3);
            dialog_draw();RefreshGList(d->gadgets,d->w,NULL,-1);
        }
        else if(cls==IDCMP_REFRESHWINDOW) { GT_BeginRefresh(d->w); dialog_draw(); RefreshGList(d->gadgets,d->w,NULL,-1); GT_EndRefresh(d->w,TRUE); }
    }
    if(action==1) { if((d->mode==DIALOG_OFFER || d->mode==DIALOG_BATCH) && d->serial==bs_transfer_serial(client)) bs_decline(client); dialog_close(); }
    else if(action==2) dialog_submit();
}

static void transfer_tools(void){if(dialog_open(DIALOG_TOOLS,"Transfer tools","",NULL)){transfer_dialog.history_index=-1;transfer_dialog.list_top=0;dialog_draw();}}
