/* 80x25 text UI. BIOS mode/cursor calls and bounded text video buffers only. */
#include <direct.h>
#include <dos.h>
#include <ctype.h>
#define UI_FILES 128
#define UI_ROWS 12
#define UI_PEER_ROWS 4
static unsigned short ui_frame[2000],ui_saved[2000];
static unsigned short __far *ui_video;
static int ui_active,ui_dirty=1,ui_focus,ui_peer_top,ui_file_top,ui_file_selected;
static int ui_original_mode,ui_original_cursor,ui_original_position,ui_saved_screen,ui_mono;
static char ui_selected_id[33],ui_notice[128],ui_cwd[128];
static struct { char name[13]; unsigned attr; unsigned long size; int tagged; } ui_files[UI_FILES];
static int ui_file_count,ui_rescan;
/* 1 send to ID, 2 IP address, 3 send to address, 4 accept, 5 preferences,
   6 help, 7 quit confirmation. Prompts never stop the network event loop. */
/* VGA DAC accents; original palette and register mappings are restored on exit. */
static unsigned char ui_dac_saved[768],ui_ega_saved[17];
static int ui_vga;
static const unsigned char ui_colors[16][3]={
    {3,4,7},{7,9,14},{22,42,30},{26,45,44},
    {45,22,28},{36,29,46},{45,35,22},{47,50,55},
    {26,30,36},{30,37,51},{34,51,40},{35,55,52},
    {58,35,37},{47,39,57},{59,51,34},{58,60,63}
};
static void ui_palette_save(void){union REGS r;struct SREGS seg;
    memset(&r,0,sizeof r);r.x.ax=0x1a00;int86(0x10,&r,&r);ui_vga=r.h.al==0x1a && !ui_mono;if(!ui_vga)return;
    segread(&seg);r.x.ax=0x1017;r.x.bx=0;r.x.cx=256;seg.es=FP_SEG((void __far *)ui_dac_saved);r.x.dx=FP_OFF((void __far *)ui_dac_saved);int86x(0x10,&r,&r,&seg);
    r.x.ax=0x1009;seg.es=FP_SEG((void __far *)ui_ega_saved);r.x.dx=FP_OFF((void __far *)ui_ega_saved);int86x(0x10,&r,&r,&seg);
}
static void ui_palette_apply(void){union REGS r;int i;if(!ui_vga)return;
    for(i=0;i<16;i++){memset(&r,0,sizeof r);r.x.ax=0x1007;r.h.bl=(unsigned char)i;int86(0x10,&r,&r);
        r.h.bl=r.h.bh;r.h.bh=0;r.x.ax=0x1010;r.h.dh=ui_colors[i][0];r.h.ch=ui_colors[i][1];r.h.cl=ui_colors[i][2];int86(0x10,&r,&r);}
}
static void ui_palette_restore(void){union REGS r;struct SREGS seg;if(!ui_vga)return;memset(&r,0,sizeof r);segread(&seg);
    r.x.ax=0x1012;r.x.bx=0;r.x.cx=256;seg.es=FP_SEG((void __far *)ui_dac_saved);r.x.dx=FP_OFF((void __far *)ui_dac_saved);int86x(0x10,&r,&r,&seg);
    r.x.ax=0x1002;seg.es=FP_SEG((void __far *)ui_ega_saved);r.x.dx=FP_OFF((void __far *)ui_ega_saved);int86x(0x10,&r,&r,&seg);
}
static int ui_popup,ui_field,ui_pending_visible,ui_pending_receiving,ui_pending_bell;
static char ui_rates[2][8];
static char ui_edit[128],ui_target_id[33],ui_target_name[64],ui_target_ip[80];
static unsigned short ui_target_port;
static unsigned ui_edit_used;
static int ui_tools_record=-1,ui_tools_top;
static unsigned long ui_tools_serial;
static unsigned long ui_offer_serial;
static int ui_offer_seen,ui_offer_choice,ui_suspended,ui_offer_top;
static struct {int popup,field,visible,receiving,bell;unsigned used;unsigned short port;char edit[128],id[33],name[64],ip[80],rates[2][8];} ui_previous;
static void ui_suspend(void) {
    memcpy(ui_previous.rates,ui_rates,sizeof ui_rates);ui_previous.popup=ui_popup;ui_previous.field=ui_field;ui_previous.visible=ui_pending_visible;ui_previous.receiving=ui_pending_receiving;ui_previous.bell=ui_pending_bell;ui_previous.used=ui_edit_used;ui_previous.port=ui_target_port;
    strcpy(ui_previous.edit,ui_edit);strcpy(ui_previous.id,ui_target_id);strcpy(ui_previous.name,ui_target_name);strcpy(ui_previous.ip,ui_target_ip);ui_suspended=1;
}
static void ui_resume(void) {
    ui_popup=0;
    if(ui_suspended) {memcpy(ui_rates,ui_previous.rates,sizeof ui_rates);ui_popup=ui_previous.popup;ui_field=ui_previous.field;ui_pending_visible=ui_previous.visible;ui_pending_receiving=ui_previous.receiving;ui_pending_bell=ui_previous.bell;ui_edit_used=ui_previous.used;ui_target_port=ui_previous.port;
        strcpy(ui_edit,ui_previous.edit);strcpy(ui_target_id,ui_previous.id);strcpy(ui_target_name,ui_previous.name);strcpy(ui_target_ip,ui_previous.ip);}
    ui_suspended=0;ui_dirty=1;
}
static void ui_offer_update(void) {
    const BSStatus *status=bs_status(client);unsigned long serial=bs_transfer_serial(client);
    if(status->awaiting_decision) {
        if(!ui_offer_seen || ui_offer_serial!=serial) {ui_offer_seen=1;ui_offer_serial=serial;ui_suspend();ui_popup=8;ui_offer_choice=0;ui_offer_top=bs_batch_index(client);ui_dirty=1;}
    } else {ui_offer_seen=0;if(ui_popup==8 || ui_popup==4) ui_resume();}
}

static unsigned char ui_attr(int a) { return (unsigned char)(ui_mono?(a==0x30 || a==0x70?0x70:0x07):a); }
static void ui_put(int x,int y,int width,const char *s,int attr) {
    int i; if(y<0 || y>=25 || x<0 || x>=80) return; if(width>80-x) width=80-x;
    for(i=0;i<width;i++) { unsigned char c=(unsigned char)(s && *s?*s++:' '); if(c<32 || c>126) c='?'; ui_frame[y*80+x+i]=(unsigned short)c|((unsigned short)ui_attr(attr)<<8); }
}
static void ui_cell(int x,int y,unsigned char c,int attr) { if(x>=0 && x<80 && y>=0 && y<25) ui_frame[y*80+x]=c|((unsigned short)ui_attr(attr)<<8); }
static void ui_box(int x,int y,int w,int h,const char *label,int attr) {
    int i; for(i=1;i<w-1;i++) { ui_cell(x+i,y,196,attr); ui_cell(x+i,y+h-1,196,attr); }
    for(i=1;i<h-1;i++) { ui_cell(x,y+i,179,attr);ui_cell(x+w-1,y+i,179,attr); }
    ui_cell(x,y,218,attr);ui_cell(x+w-1,y,191,attr);ui_cell(x,y+h-1,192,attr);ui_cell(x+w-1,y+h-1,217,attr);
    if(label) ui_put(x+2,y,w-4,label,attr);
}
static void ui_cursor(int x,int y,int visible) {
    union REGS r; memset(&r,0,sizeof r);r.h.ah=1;r.x.cx=visible?0x0607:0x2000;int86(0x10,&r,&r);
    if(visible) {r.h.ah=2;r.h.bh=0;r.h.dh=(unsigned char)y;r.h.dl=(unsigned char)x;int86(0x10,&r,&r);}
}
static void ui_message(const char *text) { strncpy(ui_notice,text,sizeof ui_notice-1);ui_notice[sizeof ui_notice-1]=0;ui_dirty=1; }
static int ui_peer_index(void) {
    int i; for(i=0;i<bs_peer_count(client);i++) if(!strcmp(bs_peer(client,i)->id,ui_selected_id)) return i; return -1;
}
static void ui_scan_files(void) {
    struct find_t entry; char old[13]=""; int i,j;
    if(ui_file_selected>=0 && ui_file_selected<ui_file_count) strcpy(old,ui_files[ui_file_selected].name);
    ui_file_count=0;memset(ui_files,0,sizeof ui_files); if(!getcwd(ui_cwd,sizeof ui_cwd)) strcpy(ui_cwd,"[directory unavailable]");
    if(_dos_findfirst("*.*",_A_NORMAL|_A_SUBDIR|_A_RDONLY|_A_ARCH,&entry)==0) do {
        if(!strcmp(entry.name,".") || (entry.attrib&(_A_HIDDEN|_A_SYSTEM|_A_VOLID))) continue;
        if(ui_file_count>=UI_FILES) break;
        strcpy(ui_files[ui_file_count].name,entry.name);ui_files[ui_file_count].attr=entry.attrib;ui_files[ui_file_count].size=entry.size;ui_file_count++;
    } while(_dos_findnext(&entry)==0);
    for(i=0;i<ui_file_count;i++) for(j=i+1;j<ui_file_count;j++) {
        int a=(ui_files[i].attr&_A_SUBDIR)!=0,b=(ui_files[j].attr&_A_SUBDIR)!=0;
        if((b && !a) || (a==b && stricmp(ui_files[i].name,ui_files[j].name)>0)) {
            char n[13]; unsigned attr;unsigned long size;
            strcpy(n,ui_files[i].name);attr=ui_files[i].attr;size=ui_files[i].size;
            ui_files[i]=ui_files[j];strcpy(ui_files[j].name,n);ui_files[j].attr=attr;ui_files[j].size=size;
        }
    }
    ui_file_selected=0;for(i=0;i<ui_file_count;i++) if(!strcmp(old,ui_files[i].name)) ui_file_selected=i;
    ui_file_top=0;ui_rescan=0;ui_dirty=1;
}
static void ui_prompt(int kind,const char *text) {
    ui_popup=kind;ui_field=0;strncpy(ui_edit,text?text:"",sizeof ui_edit-1);ui_edit[sizeof ui_edit-1]=0;ui_edit_used=(unsigned)strlen(ui_edit);ui_dirty=1;
}
static void ui_prefill_file(void) {
    ui_edit[0]=0;
    if(ui_file_selected<ui_file_count && strcmp(ui_files[ui_file_selected].name,"..")) strcpy(ui_edit,ui_files[ui_file_selected].name);
    ui_edit_used=(unsigned)strlen(ui_edit);
}
static void ui_bytes(unsigned long bytes,char *out) {
    if(bytes>=1048576UL)sprintf(out,"%lu.%lu MiB",bytes/1048576UL,(bytes%1048576UL)/104858UL);
    else if(bytes>=1024UL)sprintf(out,"%lu.%lu KiB",bytes/1024UL,(bytes%1024UL)/103UL);
    else sprintf(out,"%lu B",bytes);
}
static void ui_draw(void) {
    char line[160],label[64]; int i,index=ui_peer_index(); const BSStatus *s=bs_status(client);
    unsigned long percent=0;
    if(s->total) {unsigned long low=0,high=100;while(low<high) {unsigned long mid=(low+high+1)/2,threshold=(s->total/100)*mid+((s->total%100)*mid+99)/100;if(s->transferred>=threshold)low=mid;else high=mid-1;}percent=low;}
    if(s->total && s->transferred>=s->total) percent=100; if(percent>100) percent=100;
    if(ui_rescan) ui_scan_files();
    if(!ui_selected_id[0] && bs_peer_count(client)>0) { strcpy(ui_selected_id,bs_peer(client,0)->id);index=0; }
    if(index>=0) {if(index<ui_peer_top)ui_peer_top=index;if(index>=ui_peer_top+UI_PEER_ROWS)ui_peer_top=index-UI_PEER_ROWS+1;}
    if(ui_file_selected<ui_file_top)ui_file_top=ui_file_selected;
    if(ui_file_selected>=ui_file_top+UI_ROWS)ui_file_top=ui_file_selected-UI_ROWS+1;
    for(i=0;i<25;i++)ui_put(0,i,80,"",0x07);
    ui_put(3,1,4,"[>]",0x0b);ui_put(8,1,32,"BitSkim  0.2",0x0f);
    ui_put(56,1,21,"DOS / mTCP   ONLINE",0x0b);
    ui_put(8,2,36,"Different generations. Same network.",0x08);ui_put(49,2,28,config.name,0x08);
    ui_put(3,4,18,"NEARBY COMPUTERS",0x0f);
    sprintf(line,"%d online",bs_peer_count(client));ui_put(23,4,12,line,0x08);
    ui_put(40,4,37,"F5 Send   F3 Connect   F4 Refresh",0x0b);
    for(i=0;i<UI_PEER_ROWS;i++) {
        int p=ui_peer_top+i,y=6+i*3;
        if(p<bs_peer_count(client)){const BSPeer *peer=bs_peer(client,p);int attr=p==index?0x30:0x17;
            if(!bs_to_local(peer->name,label,sizeof label))strcpy(label,"[non-ASCII name]");
            ui_put(3,y,74,"",attr);ui_put(3,y+1,74,"",attr);
            ui_put(5,y,2,p==index?">":" ",attr);ui_put(8,y,62,label,attr);
            sprintf(line,"%.20s   /   %.15s",peer->platform,peer->address);ui_put(8,y+1,64,line,p==index?0x30:0x13);
        }
    }
    if(!bs_peer_count(client)){ui_put(6,8,65,"Looking for your other computers...",0x0f);ui_put(6,10,65,"Open BitSkim on another machine, or connect with F3.",0x08);}
    if(bs_peer_count(client)>UI_PEER_ROWS){sprintf(line,"%d-%d of %d   Arrows to browse",ui_peer_top+1,ui_peer_top+UI_PEER_ROWS<bs_peer_count(client)?ui_peer_top+UI_PEER_ROWS:bs_peer_count(client),bs_peer_count(client));ui_put(42,17,35,line,0x08);}
    for(i=3;i<77;i++)ui_cell(i,18,196,0x08);
    if(s->active){unsigned long position=1,total=1;
        if(!bs_to_local(s->filename,label,sizeof label))strcpy(label,"[choose a DOS filename when accepting]");
        if(!s->incoming && bs_queue_count(client))bs_queue_progress(client,&position,&total);
        if(s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)){position=bs_batch_index(client)+1;total=bs_batch_count(client);}
        sprintf(line,"%s   /   File %lu of %lu",s->awaiting_decision?"AWAITING YOUR APPROVAL":s->incoming?"RECEIVING":"SENDING",position,total);ui_put(3,18,65,line,0x0b);
        ui_put(3,19,74,label,0x0f);
        {char done[24],size[24],rate[24];unsigned long speed=bs_transfer_rate(client);ui_bytes(s->transferred,done);ui_bytes(s->total,size);ui_bytes(speed,rate);sprintf(line,"%s / %s     %s/s",done,size,rate);if(speed && s->total>s->transferred){char eta[24];unsigned long seconds=(s->total-s->transferred)/speed;sprintf(eta,"     %lu:%02lu left",seconds/60,seconds%60);strcat(line,eta);}ui_put(3,21,74,line,0x07);}
    }else{ui_put(3,18,23,"READY TO SEND",0x0b);ui_put(3,19,74,"Choose a computer, then press F5 to browse files and folders.",0x08);}
    if(s->active)for(i=0;i<66;i++)ui_cell(3+i,20,i<(int)(percent*66UL/100UL)?219:196,0x0b);
    if(s->active){sprintf(line,"%3lu%%",percent);ui_put(72,20,5,line,0x0f);}
    bs_progress_summary(client,line,sizeof line);ui_put(3,23,74,line,0x08);
    sprintf(line,"%d waiting | F9 controls%s",bs_queue_waiting(client),bs_queue_paused(client)?" / paused":"");ui_put(3,22,36,line,0x08);
    sprintf(line,"Visible %s / Receive %s / Plaintext",config.visible?"on":"off",config.receiving?"on":"off");ui_put(39,22,38,line,0x08);
    ui_put(3,23,74,ui_notice[0]?ui_notice:s->message,0x07);
    ui_put(0,24,80," F1 Help   F2 Settings   Tab Files   F8 Cancel   F10 Quit",0x17);
    if(ui_focus && !ui_popup){
        for(i=0;i<2000;i++)ui_frame[i]=(ui_frame[i]&255)|0x0800;
        for(i=5;i<23;i++)ui_put(10,i,64,"",0x08);
        for(i=4;i<22;i++)ui_put(8,i,64,"",0x17);
        ui_box(8,4,64,18," LOCAL FILES ",0x13);ui_put(11,5,58,ui_cwd,0x0b);
        ui_put(11,6,58,"NAME                              SIZE / TYPE",0x13);
        for(i=0;i<UI_ROWS;i++){int f=ui_file_top+i;if(f<ui_file_count){char amount[24];int attr=f==ui_file_selected?0x30:0x17;
            if(ui_files[f].attr&_A_SUBDIR)strcpy(amount,"<DIR>");else ui_bytes(ui_files[f].size,amount);
            sprintf(line,"%c %-13s                       %12s",ui_files[f].tagged?'*':' ',ui_files[f].name,amount);ui_put(11,7+i,58,line,attr);
        }}
        if(index>=0){sprintf(line,"To: %.52s",bs_peer(client,index)->name);ui_put(11,19,58,line,0x13);}
        ui_put(11,20,58,"Space tag   Enter open/send   F5 send   Esc back",0x13);
    }
    if(ui_popup) {
        int top=(ui_popup==10?4:(ui_popup==5 || ui_popup==8)?5:7),height=(ui_popup==10?17:(ui_popup==5 || ui_popup==8)?15:10);
        for(i=0;i<2000;i++)ui_frame[i]=(ui_frame[i]&255)|0x0800;
        for(i=top+1;i<top+height+1;i++)ui_put(9,i,66,"",0x00);
        for(i=top;i<top+height;i++)ui_put(7,i,66,"",0x17);
        if(ui_popup==8) {
            if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)) {
                int index,row=7;char save_name[256];
                for(i=6;i<19;i++)ui_put(8,i,64,"",0x17);
                ui_box(7,5,66,15," INCOMING BATCH - ACCEPT ALL ",0x17);
                bs_batch_summary(client,line,sizeof line);ui_put(10,6,60,line,0x17);
                for(index=ui_offer_top;index<bs_batch_count(client) && row<15;index++) {const BSBatchItem *item=bs_batch_item(client,index);bs_batch_save_name(client,index,save_name,sizeof save_name);if(bs_batch_is_tree(client)){int chunk;size_t len=strlen(save_name);for(chunk=0;chunk<4;chunk++){const char *part=(size_t)(chunk*60)<len?save_name+chunk*60:"";ui_put(10,row++,60,part,0x17);}}else{sprintf(line,"%.25s -> %.12s %lu B",item->filename,save_name,item->total);ui_put(10,row++,60,line,0x17);}}
                sprintf(line,"C: existing files = %s (DOS 8.3)",bs_conflict_get(client)==2?"REPLACE":bs_conflict_get(client)==1?"SKIP":"RENAME");ui_put(10,16,60,line,0x17);
                ui_put(13,17,24," [ Accept all / F6 ] ",ui_offer_choice==0?0x30:0x17);
                ui_put(42,17,23," [ Decline / F7 ] ",ui_offer_choice==1?0x30:0x17);
                sprintf(line,"PgUp/PgDn: browse %d files. Choose one folder after Accept.",bs_batch_count(client));ui_put(10,18,60,line,0x17);
            } else {
            const char *filename=s->filename;int length=(int)strlen(filename);
            ui_box(7,7,66,10," INCOMING FILE ",0x17);
            sprintf(line,"From: %.52s",s->peer_name);ui_put(10,8,60,line,0x17);
            ui_put(10,9,60,"File:",0x17);ui_put(16,9,54,filename,0x17);
            if(length>54) ui_put(16,10,54,filename+54,0x17);
            if(length>108) ui_put(67,10,3,"...",0x17);
            sprintf(line,"Size: %lu bytes   |   Sender identity is unverified",s->total);ui_put(10,11,60,line,0x17);
            ui_put(13,13,20," [ Accept / F6 ] ",ui_offer_choice==0?0x30:0x17);
            ui_put(42,13,23," [ Decline / F7 ] ",ui_offer_choice==1?0x30:0x17);
            sprintf(line,"C: existing files = %s",bs_conflict_get(client)==2?"REPLACE":bs_conflict_get(client)==1?"SKIP":"RENAME");ui_put(10,14,60,line,0x17);ui_put(10,15,60,"Tab/arrows: choose   Enter: confirm   Esc: decide later",0x17);

            }        } else if(ui_popup==6) {
            ui_box(7,7,66,10," HELP ",0x17);
            ui_put(10,9,60,"Tab opens the file browser. Arrows select a computer/file.",0x17);
            ui_put(10,10,60,"Space tags entries; F5 sends tagged files or a directory.",0x17);
            ui_put(10,11,60,"Enter opens a folder or sends a file.",0x17);
            ui_put(10,12,60,"Incoming offers wait for you. F7 declines, F8 cancels.",0x17);
            ui_put(10,13,60,"F2 edits your name/visibility. F10 quits. No encryption.",0x17);
            ui_put(10,15,60,"T: Transfer tools (diagnostics, history, conflicts)",0x17);
        } else if(ui_popup==10) {
            int row;char report[61];if(ui_tools_record>=0 && ui_tools_serial){ui_tools_record=bs_history_find(client,ui_tools_serial);if(ui_tools_record<0)ui_tools_record=-2;}ui_box(7,4,66,17," NETWORK / TRANSFER TOOLS (T: test) ",0x17);
            for(row=0;row<10;row++){bs_tools_line(client,ui_tools_record,ui_tools_top+row,report,sizeof report);ui_put(10,6+row,60,report,0x17);}
            sprintf(line,"Conflicts: %s",bs_conflict_get(client)==2?"REPLACE":bs_conflict_get(client)==1?"SKIP":"RENAME");ui_put(10,17,60,line,0x17);
            ui_put(10,18,60,"D diagnostics H history N older B newer R retry C conflicts",0x17);
            ui_put(10,19,60,"PgUp/PgDn scroll; Esc close. History is this session only.",0x17);
        } else if(ui_popup==9) {
            ui_box(7,7,66,10," OUTGOING QUEUE ",0x17);
            sprintf(line,"%d items / %s",bs_queue_count(client),bs_queue_paused(client)?"paused":"running");ui_put(10,9,60,line,0x17);
            ui_put(10,11,60,"R: retry/continue   P: pause after current",0x17);
            ui_put(10,13,60,"S: skip idle head   C: clear waiting   Esc: close",0x17);
            ui_put(10,15,60,"Completed files are kept. Queue is not saved on exit.",0x17);
        } else if(ui_popup==7) {
            ui_box(7,7,66,10," QUIT BITSKIM? ",0x17);ui_put(10,10,60,"An active transfer will be cancelled.",0x17);ui_put(10,13,60,"Enter: quit     Esc: continue",0x17);
        } else if(ui_popup==5) {
            ui_box(7,5,66,15," PREFERENCES / NETWORK ",0x17);ui_put(10,7,9,"Name:",0x17);ui_put(20,7,49,ui_edit_used>48?ui_edit+ui_edit_used-48:ui_edit,ui_field==0?0x30:0x17);
            sprintf(line,"[%c] Visible to nearby computers",ui_pending_visible?'X':' ');ui_put(10,9,59,line,ui_field==1?0x30:0x17);
            sprintf(line,"[%c] Allow incoming transfers",ui_pending_receiving?'X':' ');ui_put(10,10,59,line,ui_field==2?0x30:0x17);
            ui_put(10,12,60,"Network limits (KiB/s; 0 = Unlimited; next file)",0x17);
            ui_put(10,14,22,"Send:",0x17);ui_put(34,14,12,ui_rates[0],ui_field==3?0x30:0x17);
            ui_put(10,15,22,"Receive:",0x17);ui_put(34,15,12,ui_rates[1],ui_field==4?0x30:0x17);
            sprintf(line,"[%c] Completion sound",ui_pending_bell?'X':' ');ui_put(10,16,59,line,ui_field==5?0x30:0x17);
            ui_put(10,17,60,"Tab: next field   Space: toggle   Enter: save",0x17);ui_put(10,18,60,"Esc: cancel. Receive caps need an updated sender.",0x17);
        } else {
            const char *title=ui_popup==2?" CONNECT BY IP ":ui_popup==4?" ACCEPT INCOMING FILE ":" SEND FILE ";
            ui_box(7,7,66,10,title,0x17);
            if(ui_popup==1) {sprintf(line,"To: %.55s",ui_target_name);ui_put(10,9,60,line,0x17);}
            else if(ui_popup==4) ui_put(10,9,60,(bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)?"Save all files in this existing folder.":"Choose a NEW filename. Existing files are protected.",0x17);
            else ui_put(10,9,60,ui_popup==2?"IPv4 address, optionally :port (Mac host: 10.0.2.2)":ui_target_ip,0x17);
            ui_put(10,11,60,ui_popup==2?"Address:":ui_popup==4 && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)?"Destination folder:":"File or folder path:",0x17);ui_put(10,12,60,ui_edit_used>58?ui_edit+ui_edit_used-58:ui_edit,0x30);
            ui_put(10,14,60,"Enter: confirm     Esc: cancel",0x17);if(ui_popup==4){char info[256];bs_destination_space_report(client,ui_edit,(bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0),info,sizeof info);ui_put(10,15,60,info,0x17);}else ui_put(10,15,60,"Networking continues while this prompt is open.",0x17);
        }
    }
    if(s->awaiting_decision && ui_popup && ui_popup!=8 && ui_popup!=4) ui_put(1,21,78,"INCOMING OFFER waiting: Esc closes this dialog; F6 accepts / F7 declines.",0x1e);
    _fmemcpy(ui_video,ui_frame,sizeof ui_frame);
    if(ui_popup && ui_popup<=5 && (ui_popup!=5 || ui_field==0 || (ui_field>=3 && ui_field<=4))) {int x,y; if(ui_popup==5) {if(ui_field>=3){x=34+(int)strlen(ui_rates[ui_field-3]);y=ui_field==3?14:15;}else{x=20+(ui_edit_used<48?ui_edit_used:48);y=7;}}else{x=10+(ui_edit_used<58?ui_edit_used:58);y=12;}ui_cursor(x,y,1);} else ui_cursor(0,0,0);
    ui_dirty=0;
}
static void ui_start(void) {
    union REGS r;unsigned char rows=*(unsigned char __far *)MK_FP(0x40,0x84);
    memset(&r,0,sizeof r);r.h.ah=0x0f;int86(0x10,&r,&r);ui_original_mode=r.h.al&0x7f;
    ui_mono=ui_original_mode==7;ui_video=(unsigned short __far *)MK_FP(ui_mono?0xb000:0xb800,0);
    r.h.ah=3;r.h.bh=0;int86(0x10,&r,&r);ui_original_cursor=r.x.cx;ui_original_position=r.x.dx;
    ui_saved_screen=(ui_original_mode==3 || ui_original_mode==7) && (rows==24 || rows==0);
    if(ui_saved_screen) _fmemcpy(ui_saved,ui_video,sizeof ui_saved);
    ui_palette_save();r.x.ax=ui_mono?7:3;int86(0x10,&r,&r);ui_palette_apply();ui_active=1;ui_scan_files();ui_draw();
}
static void ui_stop(void) {
    union REGS r; if(!ui_active) return; ui_active=0;
    memset(&r,0,sizeof r);r.x.ax=(unsigned)ui_original_mode;int86(0x10,&r,&r);ui_palette_restore();
    if(ui_saved_screen) _fmemcpy(ui_video,ui_saved,sizeof ui_saved);
    r.h.ah=1;r.x.cx=ui_original_cursor;int86(0x10,&r,&r);r.h.ah=2;r.h.bh=0;r.x.dx=ui_original_position;int86(0x10,&r,&r);
}
static void ui_submit(void) {
    int ok=1;
    if(ui_popup==5) {
        unsigned long up,down;
        if(!bs_limit_parse(ui_rates[0],&up) || !bs_limit_parse(ui_rates[1],&down)){ui_message("Limits: use 0..1048575 KiB/s; 0 = Unlimited.");return;}
        if(!ui_edit[0] || strlen(ui_edit)>BS_NAME_MAX) {ui_message("Choose an ASCII name of 1-63 characters.");return;}
        send_limit=up;receive_limit=down;bs_limits_configure(client,up,down);
        strcpy(config.name,ui_edit);config.visible=ui_pending_visible;config.receiving=ui_pending_receiving;completion_bell=ui_pending_bell;
        bs_configure(client,config.name,config.visible,config.receiving);ui_message(save_config()?"Preferences saved.":"Preferences changed, but could not save BITSKIM.CFG.");ui_popup=0;return;
    }
    if(ui_popup==2) {
        char *colon;char address_text[80];unsigned long port=BS_PORT;
        if(strlen(ui_edit)>=sizeof address_text) {ui_message("The IP address is too long.");return;}
        strcpy(address_text,ui_edit);colon=strchr(address_text,':');
        if(colon) {char *end;*colon++=0;port=strtoul(colon,&end,10);if(*end || port<1 || port>65535) {ui_message("Invalid port.");return;}}
        if(inet_addr(address_text)==INADDR_NONE || inet_addr(address_text)==0) {ui_message("Enter a numeric IPv4 address.");return;}
        strcpy(ui_target_ip,address_text);ui_target_port=(unsigned short)port;ui_prompt(3,"");ui_prefill_file();return;
    }
    if(!ui_edit[0]) {ui_message("Enter a DOS 8.3 file path first.");return;}
    if((ui_popup==1 || ui_popup==3) && !prepare_single_send(client)){ui_message("A transfer is already in progress.");return;}
    if(ui_popup==1) {char absolute[128];ok=_fullpath(absolute,ui_edit,sizeof absolute)!=NULL && bs_queue_peer(client,ui_target_id,absolute);}
    else if(ui_popup==3) {char absolute[128];ok=_fullpath(absolute,ui_edit,sizeof absolute)!=NULL && bs_queue_address(client,ui_target_ip,ui_target_port,absolute);}
    else if(ui_popup==4) ok=(bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)?bs_accept_batch(client,ui_edit):bs_accept_conflict(client,ui_edit);
    if(ok) {if(ui_popup==4) ui_resume();else {ui_popup=0;ui_focus=0;}ui_notice[0]=0;}else ui_message(bs_last_error(client));
}
static void ui_accept_prompt(void) {
    char leaf[13];
    if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)){ui_prompt(4,bs_last_folder(client,1)[0]?bs_last_folder(client,1):ui_cwd);return;}
    if(!bs_suggest_name(bs_status(client)->filename,0,leaf,sizeof leaf))strcpy(leaf,"RECEIVED.BIN");
    {char path[128];const char *folder=bs_last_folder(client,1);if(folder[0] && strlen(folder)+strlen(leaf)+2<sizeof path){sprintf(path,"%s%s%s",folder,folder[strlen(folder)-1]==92?"":"\\",leaf);ui_prompt(4,path);}else ui_prompt(4,leaf);}
}
/* Return zero only on a confirmed quit; no blocking input or modal event loop. */
static int ui_key(int key) {
    int index=ui_peer_index(),delta=0;
    if(ui_popup) {
        if(ui_popup==9){
            if(key==27)ui_popup=0;
            else if(key=='r' || key=='R'){if(bs_queue_count(client))bs_queue_continue(client);else if(!bs_retry_failed(client))ui_message(bs_last_error(client));}
            else if(key=='p' || key=='P')bs_queue_pause(client);
            else if(key=='s' || key=='S')bs_queue_skip(client);
            else if(key=='c' || key=='C')bs_queue_clear(client);
            ui_dirty=1;return 1;
        }
        if(ui_popup==10){
            if(key=='t' || key=='T'){const BSPeer *peer=index>=0?bs_peer(client,index):NULL;if(!bs_network_test(client,peer?peer->address:NULL,peer?peer->port:0))ui_message(bs_last_error(client));ui_tools_record=-1;ui_tools_top=0;}
            if(key==27)ui_popup=0;
            else if(key=='d' || key=='D'){ui_tools_record=-1;ui_tools_top=0;}
            else if(key=='h' || key=='H'){BSHistoryItem h;ui_tools_record=0;ui_tools_top=0;if(bs_history_item(client,0,&h))ui_tools_serial=h.serial;}
            else if(key==0x149 && ui_tools_top>=10)ui_tools_top-=10;
            else if(key==0x151)ui_tools_top+=10;
            else if((key=='n' || key=='N') && ui_tools_record+1<bs_history_count(client)){BSHistoryItem h;ui_tools_record++;ui_tools_top=0;if(bs_history_item(client,ui_tools_record,&h))ui_tools_serial=h.serial;}
            else if((key=='b' || key=='B') && ui_tools_record>0){BSHistoryItem h;ui_tools_record--;ui_tools_top=0;if(bs_history_item(client,ui_tools_record,&h))ui_tools_serial=h.serial;}
            else if(key=='c' || key=='C')bs_conflict_policy(client,(bs_conflict_get(client)+1)%3);
            else if(key=='r' || key=='R'){BSHistoryItem h;int i;for(i=0;i<bs_history_count(client);i++)if(bs_history_item(client,i,&h) && h.serial==ui_tools_serial){if(!bs_retry_history(client,i))ui_message(bs_last_error(client));break;}}
            ui_dirty=1;return 1;
        }
        if(ui_popup==8) {
            if(key=='c' || key=='C'){bs_conflict_policy(client,(bs_conflict_get(client)+1)%3);ui_dirty=1;return 1;}
            if(key==0x149 && ui_offer_top>bs_batch_index(client))ui_offer_top-=bs_batch_is_tree(client)?2:8;
            else if(key==0x151 && ui_offer_top+(bs_batch_is_tree(client)?2:8)<bs_batch_count(client))ui_offer_top+=bs_batch_is_tree(client)?2:8;
            else if(key==9 || key==0x14b || key==0x14d) ui_offer_choice=!ui_offer_choice;
            else if(key==27) ui_resume();
            else if(key==0x140 || key=='a' || key=='A' || (key==13 && ui_offer_choice==0)) ui_accept_prompt();
            else if(key==0x141 || key=='d' || key=='D' || (key==13 && ui_offer_choice==1)) {bs_decline(client);ui_resume();ui_message("Incoming offer declined.");}
            ui_dirty=1;return 1;
        }
        if(ui_popup==4 && key==27 && bs_status(client)->awaiting_decision) {ui_popup=8;ui_dirty=1;return 1;}
        if(key==27) {ui_popup=0;ui_dirty=1;return 1;}
        if(ui_popup==7) {if(key==13) return 0;return 1;}
        if(ui_popup==6 && (key=='t' || key=='T')){ui_popup=10;ui_tools_record=-1;ui_tools_top=0;ui_dirty=1;return 1;}
        if(ui_popup==6) {if(key==13 || key==0x13b) ui_popup=0;ui_dirty=1;return 1;}
        if(key==13) {ui_submit();ui_dirty=1;return 1;}
        if(ui_popup==5 && key==9) {ui_field=(ui_field+1)%6;ui_dirty=1;return 1;}
        if(ui_popup==5 && ui_field>0 && ui_field<3) {if(key==' ') {if(ui_field==1) ui_pending_visible=!ui_pending_visible;else ui_pending_receiving=!ui_pending_receiving;}ui_dirty=1;return 1;}
        if(ui_popup==5 && ui_field==5){if(key==' ')ui_pending_bell=!ui_pending_bell;ui_dirty=1;return 1;}
        if(ui_popup==5 && ui_field>=3){char *value=ui_rates[ui_field-3];int used=(int)strlen(value);if(key==8 && used)value[used-1]=0;else if(key>='0' && key<='9' && used<7){value[used]=(char)key;value[used+1]=0;}ui_dirty=1;return 1;}
        if(key==8 && ui_edit_used) ui_edit[--ui_edit_used]=0;
        else if(key>=32 && key<=126 && ui_edit_used<(ui_popup==5?BS_NAME_MAX:127)) {ui_edit[ui_edit_used++]=(char)key;ui_edit[ui_edit_used]=0;}
        ui_dirty=1;return 1;
    }
    if(key==' ' && ui_focus && ui_file_selected<ui_file_count && strcmp(ui_files[ui_file_selected].name,".."))ui_files[ui_file_selected].tagged=!ui_files[ui_file_selected].tagged;
    else if(key==9) ui_focus=!ui_focus;
    else if(key==0x148) delta=-1;else if(key==0x150) delta=1;
    else if(key==0x149) delta=-(ui_focus?UI_ROWS:UI_PEER_ROWS);else if(key==0x151) delta=ui_focus?UI_ROWS:UI_PEER_ROWS;
    else if(key==0x147) delta=-UI_FILES;else if(key==0x14f) delta=UI_FILES;
    else if(key==8 && ui_focus) {if(chdir("..")) ui_message("Cannot open parent directory.");else ui_scan_files();}
    else if(key==0x13b) ui_prompt(6,"");
    else if(key==0x13c) {sprintf(ui_rates[0],"%lu",send_limit/1024UL);sprintf(ui_rates[1],"%lu",receive_limit/1024UL);ui_pending_visible=config.visible;ui_pending_receiving=config.receiving;ui_pending_bell=completion_bell;ui_prompt(5,config.name);}
    else if(key==0x13d) ui_prompt(2,"10.0.2.2");
    else if(key==0x13e) {bs_refresh(client);ui_scan_files();ui_message("Searching for nearby computers...");}
    else if((key==0x13f || key==13) && !ui_focus) {if(index<0)ui_message("Select an available computer, or use F3 to connect by IP.");else {const char *folder=bs_last_folder(client,0);if(folder[0] && !chdir(folder))ui_scan_files();ui_focus=1;}}
    else if(key==27 && ui_focus)ui_focus=0;
    else if(key==0x13f || key==13) {
        int tagged=0,j;for(j=0;j<ui_file_count;j++)if(ui_files[j].tagged)tagged++;
        if(key==0x13f && tagged){
            if(index<0)ui_message("Select an available computer first.");
            else if(!prepare_single_send(client))ui_message("A transfer is already in progress.");
            else {char id[33],absolute[128];int ok=1;strcpy(id,ui_selected_id);
                for(j=0;j<ui_file_count;j++)if(ui_files[j].tagged){
                    if(!_fullpath(absolute,ui_files[j].name,sizeof absolute) || !bs_queue_peer(client,id,absolute)){ok=0;break;}
                    ui_files[j].tagged=0;
                }
                if(!ok){bs_queue_pause(client);ui_message("Queue paused: cannot add every tagged entry. Review F9.");}
                else {ui_focus=0;ui_message("Selected files queued. F9 controls the outgoing queue.");}
            }
        }
        else if(key==13 && ui_focus && ui_file_selected<ui_file_count && (ui_files[ui_file_selected].attr&_A_SUBDIR)) {if(chdir(ui_files[ui_file_selected].name))ui_message("Cannot open that directory.");else ui_scan_files();}
        else if(index<0) ui_message("Select an available computer, or use F3 to connect by IP.");
        else {strcpy(ui_target_id,ui_selected_id);strcpy(ui_target_name,bs_peer(client,index)->name);ui_prompt(1,"");ui_prefill_file();}
    } else if(key==0x140) {if(bs_status(client)->awaiting_decision) ui_accept_prompt();else ui_message("There is no incoming offer waiting for approval.");}
    else if(key==0x141) {bs_decline(client);ui_message("Incoming offer declined.");}

    else if(key==0x142) {bs_cancel(client);ui_message("Transfer cancelled.");}
    else if(key==0x143)ui_prompt(9,"");
    else if(key==0x144 || key==27) {if(bs_status(client)->active) ui_prompt(7,"");else return 0;}
    if(delta) {
        if(ui_focus) {ui_file_selected+=delta;if(ui_file_selected<0)ui_file_selected=0;if(ui_file_selected>=ui_file_count)ui_file_selected=ui_file_count?ui_file_count-1:0;}
        else if(bs_peer_count(client)>0) {index=index<0?0:index+delta;if(index<0)index=0;if(index>=bs_peer_count(client))index=bs_peer_count(client)-1;strcpy(ui_selected_id,bs_peer(client,index)->id);}
    }
    ui_dirty=1;return 1;
}
