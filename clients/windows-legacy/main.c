/* Classic controls, ANSI UI; all core strings and wire fields remain UTF-8. */
#include "bitskim.h"
#include "../shared/single-send.h"
#include "platform.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned long send_limit,receive_limit;
static HWND network_window,network_send,network_receive;
static HWND window,list,address,status,prefs,namebox,visiblebox,receivebox,autobox,themebox,startbox,closebox;
static int completion_sound;static unsigned long completion_serial;
static BSClient *client; static char peer_ids[BS_MAX_PEERS][33],ini[MAX_PATH],name[64],identity[33];
static int visible=1,receiving=1,automatic=0,in_offer=0,peers_dirty=1; static HINSTANCE instance;
#include "appearance.h"
#include "tray.h"
static HWND control(HWND parent,const char *klass,const char *text,DWORD style,int x,int y,int w,int h,int id) {
    int is_button=!lstrcmpiA(klass,"BUTTON"),check=is_button && ((style&15)==BS_AUTOCHECKBOX);
    HWND c;
    if(is_button) style=(style&~15UL)|BS_OWNERDRAW|WS_TABSTOP;
    c=CreateWindowA(klass,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,(HMENU)(INT_PTR)id,instance,NULL);
    if(check) SetWindowLongA(c,GWL_USERDATA,CHECK_KIND);
    SendMessage(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE); return c;
}
static int ui_convert(const char *s,char *out,int cap,UINT from,UINT to) {
    wchar_t wide[BS_PATH_MAX+1],check[BS_PATH_MAX+1]; BOOL used=FALSE; int i;
    if(!MultiByteToWideChar(from,0,s,-1,wide,BS_PATH_MAX+1)) return 0;
    if(!WideCharToMultiByte(to,0,wide,-1,out,cap,NULL,to==CP_ACP?&used:NULL) || used) return 0;
    if(!MultiByteToWideChar(to,0,out,-1,check,BS_PATH_MAX+1)) return 0;
    for(i=0;wide[i] || check[i];i++) if(wide[i]!=check[i]) return 0;
    return 1;
}
static int from_ansi(const char *s,char *out,int cap) { return ui_convert(s,out,cap,CP_ACP,CP_UTF8); }
static int local(const char *s,char *out,int n) { if(ui_convert(s,out,n,CP_UTF8,CP_ACP)) return 1; lstrcpynA(out,"[name unavailable in local code page]",n); return 0; }
static void changed(void *context,BSEvent event) { (void)context; if(event==BS_EVENT_PEERS) peers_dirty=1; }
#include "../shared/tools-win.h"
static void catalog(void) {
    int i,selection=(int)SendMessage(list,LB_GETCURSEL,0,0); char selected[33]="",line[256],label[128];
    if(selection>=0 && selection<BS_MAX_PEERS) strcpy(selected,peer_ids[selection]);
    SendMessage(list,LB_RESETCONTENT,0,0);
    for(i=0;i<bs_peer_count(client);i++) {
        const BSPeer *p=bs_peer(client,i); strcpy(peer_ids[i],p->id); local(p->name,label,sizeof label);
        wsprintfA(line,"%s  [%s]  %s",label,p->platform,p->address); SendMessageA(list,LB_ADDSTRING,0,(LPARAM)line);
        if(!strcmp(selected,p->id)) SendMessage(list,LB_SETCURSEL,i,0);
    }
}
static void error(void) { char text[256]; local(bs_last_error(client),text,sizeof text); MessageBoxA(window,text,"BitSkim",MB_OK|MB_ICONEXCLAMATION); }
static const char *recent_folder(int incoming){static char native[MAX_PATH];local(bs_last_folder(client,incoming),native,sizeof native);return native;}
static void send_file_inner(const char *dropped) {
    char path[MAX_PATH]="",utf8[BS_PATH_MAX+1],target[80]; OPENFILENAMEA ofn; int selected,ok;
    if(dropped) lstrcpynA(path,dropped,sizeof path);
    else { memset(&ofn,0,sizeof ofn); ofn.lStructSize=OPENFILENAME_SIZE_VERSION_400; ofn.hwndOwner=window; ofn.lpstrFile=path; ofn.nMaxFile=sizeof path; ofn.lpstrInitialDir=recent_folder(0); ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR; if(!GetOpenFileNameA(&ofn)) return; }
    if(!from_ansi(path,utf8,sizeof utf8)) return;
    if(!prepare_single_send(client)) {MessageBoxA(window,"A transfer is already in progress.","BitSkim",MB_OK);return;}
    GetWindowTextA(address,target,sizeof target); selected=(int)SendMessage(list,LB_GETCURSEL,0,0);
    if(*target) { char *colon=strchr(target,':'); unsigned long port=BS_PORT; if(colon) { char *end; *colon++=0; port=strtoul(colon,&end,10); if(*end || port<1 || port>65535) { MessageBoxA(window,"Invalid port.","BitSkim",MB_OK); return; } } ok=bs_queue_address(client,target,(unsigned short)port,utf8); }
    else if(selected>=0 && selected<bs_peer_count(client)) ok=bs_queue_peer(client,peer_ids[selected],utf8);
    else { MessageBoxA(window,"Select a computer or enter its IPv4 address.","BitSkim",MB_OK); return; }
    if(!ok) error();
}
static void send_file(const char *path) { if(in_offer) return; in_offer=1; send_file_inner(path); in_offer=0; }
#include "../shared/collection-win.h"
static void send_folder(void){BROWSEINFOA browse;LPITEMIDLIST item;char path[MAX_PATH];memset(&browse,0,sizeof browse);browse.hwndOwner=window;browse.lpszTitle="Choose a folder to send";browse.ulFlags=BIF_RETURNONLYFSDIRS;item=SHBrowseForFolderA(&browse);if(item){if(SHGetPathFromIDListA(item,path))send_file(path);{LPMALLOC allocator;if(SUCCEEDED(SHGetMalloc(&allocator))){allocator->lpVtbl->Free(allocator,item);allocator->lpVtbl->Release(allocator);}}}}
static void batch_offer(void) {
    char path[MAX_PATH],folder[BS_PATH_MAX+1],*slash;OPENFILENAMEA ofn;unsigned long serial=bs_transfer_serial(client);
    if(!automatic && !collection_confirm(window,client)){bs_decline(client);return;}
    if(serial!=bs_transfer_serial(client) || !bs_status(client)->awaiting_decision)return;
    local(bs_batch_item(client,bs_batch_index(client))->filename,path,sizeof path);memset(&ofn,0,sizeof ofn);ofn.lStructSize=OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner=window;ofn.lpstrFile=path;ofn.nMaxFile=sizeof path;ofn.lpstrInitialDir=recent_folder(1);ofn.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;ofn.lpstrTitle="Save all files here (filename field ignored)";
    if(!GetSaveFileNameA(&ofn)){bs_decline(client);return;}
    if(serial!=bs_transfer_serial(client) || !bs_status(client)->awaiting_decision)return;
    slash=strrchr(path,'\\');if(!slash){bs_decline(client);return;}slash[1]=0;
    if(!from_ansi(path,folder,sizeof folder) || !bs_tools_space(window,client,folder,1,!automatic) || !bs_accept_batch(client,folder)){error();bs_decline(client);}
}
static void offer(void) {
    const BSStatus *s=bs_status(client); char text[600],filename[256],sender[128],path[MAX_PATH]="",utf8[BS_PATH_MAX+1]; OPENFILENAMEA ofn;
    if(in_offer || !s->awaiting_decision) return; in_offer=1;
    show_main();
    if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)){batch_offer();in_offer=0;return;}
    if(!bs_suggest_name(s->filename,0,filename,sizeof filename))strcpy(filename,"received-file"); local(s->peer_name,sender,sizeof sender);
    wsprintfA(text,"Accept %s from %s (%lu bytes)?\r\nTransfers are unencrypted; sender names are unverified.",filename,sender,s->total);
    if(!automatic && MessageBoxA(window,text,"Incoming BitSkim file",MB_YESNO|MB_ICONQUESTION)!=IDYES) bs_decline(client);
    else {
        if(!automatic && !bs_tools_pair_offer(window,client)){bs_decline(client);in_offer=0;return;}
        lstrcpynA(path,filename,sizeof path); memset(&ofn,0,sizeof ofn); ofn.lStructSize=OPENFILENAME_SIZE_VERSION_400;
        ofn.hwndOwner=window; ofn.lpstrFile=path; ofn.nMaxFile=sizeof path; ofn.lpstrInitialDir=recent_folder(1); ofn.Flags=OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        ofn.lpstrTitle="Save incoming file (choose a new filename)";
        if(GetSaveFileNameA(&ofn) && from_ansi(path,utf8,sizeof utf8)) { if(!bs_tools_space(window,client,utf8,0,!automatic) || !bs_accept_conflict(client,utf8)) { error(); bs_decline(client); } } else bs_decline(client);
    }
    in_offer=0;
}
static LRESULT CALLBACK network_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    int handled;LRESULT painted=paint_theme(w,msg,wp,lp,&handled);if(handled)return painted;
    if(msg==WM_CREATE){char value[32];
        control(w,"STATIC","Network transfer limits (KiB/s)",0,16,18,350,20,0);
        control(w,"STATIC","Send",0,16,54,140,22,0);sprintf(value,"%lu",send_limit/1024UL);network_send=control(w,"EDIT",value,WS_TABSTOP|WS_BORDER|ES_NUMBER,176,50,170,24,0);SendMessage(network_send,EM_SETLIMITTEXT,7,0);
        control(w,"STATIC","Receive",0,16,88,140,22,0);sprintf(value,"%lu",receive_limit/1024UL);network_receive=control(w,"EDIT",value,WS_TABSTOP|WS_BORDER|ES_NUMBER,176,84,170,24,0);SendMessage(network_receive,EM_SETLIMITTEXT,7,0);
        control(w,"STATIC","0 = Unlimited. Applies to the next file.\r\nReceive caps require an updated sender.",0,16,122,350,40,0);
        control(w,"BUTTON","Save",BS_DEFPUSHBUTTON,176,180,80,28,1);control(w,"BUTTON","Cancel",0,266,180,80,28,2);return 0;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==1){char up[32],down[32];unsigned long tx,rx;
        GetWindowTextA(network_send,up,sizeof up);GetWindowTextA(network_receive,down,sizeof down);
        if(!bs_limit_parse(up,&tx) || !bs_limit_parse(down,&rx)){MessageBoxA(w,"Enter 0..1048575 KiB/s; 0 is Unlimited.","BitSkim",MB_OK);return 0;}
        send_limit=tx;receive_limit=rx;bs_limits_configure(client,tx,rx);
        WritePrivateProfileStringA("BitSkim","SendLimitKiB",up,ini);WritePrivateProfileStringA("BitSkim","ReceiveLimitKiB",down,ini);DestroyWindow(w);return 0;
    }
    if(msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(wp)==2)){DestroyWindow(w);return 0;}
    if(msg==WM_DESTROY){network_window=NULL;EnableWindow(prefs,TRUE);SetForegroundWindow(prefs);return 0;}
    return DefWindowProcA(w,msg,wp,lp);
}
static LRESULT CALLBACK prefproc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    int handled; LRESULT painted=paint_theme(w,msg,wp,lp,&handled); if(handled) return painted;
    if(msg==WM_COMMAND && LOWORD(wp)==107){if(!network_window){network_window=CreateWindowA("BitSkimLegacyNetwork","BitSkim Network",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,380,260,w,NULL,instance,NULL);if(network_window){EnableWindow(w,FALSE);ShowWindow(network_window,SW_SHOW);}}return 0;}
    if(msg==WM_COMMAND && LOWORD(wp)>=101 && LOWORD(wp)<=105 && HIWORD(wp)==BN_CLICKED) { HWND c=(HWND)lp; set_checked(c,!checked(c)); return 0; }
    if(msg==WM_COMMAND && LOWORD(wp)==106 && HIWORD(wp)==CBN_SELCHANGE) { int selected=(int)SendMessage(themebox,CB_GETCURSEL,0,0); if(selected>=0 && selected<WIN_THEME_COUNT) { theme_index=selected; apply_theme(); } return 0; }
    if(msg==WM_CREATE) {
        int i; char label[128]; saved_theme=theme_index; local(name,label,sizeof label);
        control(w,"STATIC","Client name",0,16,18,290,20,0); namebox=control(w,"EDIT",label,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,16,42,320,24,0); SendMessage(namebox,EM_SETLIMITTEXT,BS_NAME_MAX,0);
        visiblebox=control(w,"BUTTON","Visible on network",BS_AUTOCHECKBOX|WS_TABSTOP,16,84,320,24,101);
        receivebox=control(w,"BUTTON","Allow incoming transfers",BS_AUTOCHECKBOX|WS_TABSTOP,16,116,320,24,102);
        autobox=control(w,"BUTTON","Automatically accept offers",BS_AUTOCHECKBOX|WS_TABSTOP,16,148,320,24,103);
        set_checked(visiblebox,visible); set_checked(receivebox,receiving); set_checked(autobox,automatic);
        control(w,"STATIC","Theme (live preview)",0,16,184,320,20,0);
        themebox=control(w,"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,16,208,320,180,106);
        for(i=0;i<WIN_THEME_COUNT;i++) { WideCharToMultiByte(CP_ACP,0,win_themes[i].name,-1,label,sizeof label,NULL,NULL); SendMessageA(themebox,CB_ADDSTRING,0,(LPARAM)label); }
        SendMessage(themebox,CB_SETCURSEL,theme_index,0);
        startbox=control(w,"BUTTON","Start hidden in the system tray",BS_AUTOCHECKBOX,16,246,320,26,104);
        closebox=control(w,"BUTTON","Close window to the system tray",BS_AUTOCHECKBOX,16,278,320,26,105);
        set_checked(startbox,start_tray); set_checked(closebox,close_tray);
        control(w,"BUTTON","Network transfer limits...",0,16,318,320,28,107);
        control(w,"BUTTON","Save",BS_DEFPUSHBUTTON,166,374,80,28,1); control(w,"BUTTON","Cancel",0,256,374,80,28,2); return 0;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==1) {
        char label[128]; GetWindowTextA(namebox,label,sizeof label); if(!*label || !from_ansi(label,name,sizeof name)) { MessageBoxA(w,"Choose a shorter client name.","BitSkim",MB_OK); return 0; }
        visible=checked(visiblebox); receiving=checked(receivebox); automatic=checked(autobox);
        WritePrivateProfileStringA("BitSkim","Name",label,ini); WritePrivateProfileStringA("BitSkim","Visible",visible?"1":"0",ini); WritePrivateProfileStringA("BitSkim","Receiving",receiving?"1":"0",ini); WritePrivateProfileStringA("BitSkim","AutoAccept",automatic?"1":"0",ini);
        start_tray=checked(startbox); close_tray=checked(closebox);
        WritePrivateProfileStringA("BitSkim","Theme",theme()->id,ini);
        WritePrivateProfileStringA("BitSkim","StartTray",start_tray?"1":"0",ini);
        WritePrivateProfileStringA("BitSkim","CloseTray",close_tray?"1":"0",ini);
        bs_configure(client,name,visible,receiving); DestroyWindow(w); return 0;
    }
    if((msg==WM_COMMAND && LOWORD(wp)==2) || msg==WM_CLOSE) { theme_index=saved_theme; apply_theme(); DestroyWindow(w); return 0; }
    if(msg==WM_DESTROY) { prefs=NULL; EnableWindow(window,TRUE); SetForegroundWindow(window); return 0; }
    return DefWindowProcA(w,msg,wp,lp);
}
static LRESULT CALLBACK procedure(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    int handled; LRESULT painted=paint_theme(w,msg,wp,lp,&handled); if(handled) return painted;
    if(taskbar_created && msg==taskbar_created) { tray_present=0; if(!add_tray()) show_main(); return 0; }
    if(msg==TRAY_MESSAGE) { if(lp==WM_LBUTTONDBLCLK) show_main(); else if(lp==WM_RBUTTONUP) tray_menu(); return 0; }
    if(msg==WM_MEASUREITEM) { ((MEASUREITEMSTRUCT *)lp)->itemHeight=26; return TRUE; }
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps;HDC dc=BeginPaint(w,&ps);RECT r={16,424,564,431};const BSStatus *s=bs_status(client);unsigned long pct=0;HBRUSH brush;
        FillRect(dc,&r,surface_brush);
        if(s->active && s->total)pct=s->total<=42949672UL?s->transferred*100/s->total:s->transferred/(s->total/100);
        if(pct>100)pct=100;r.right=r.left+(int)(548*pct/100);brush=CreateSolidBrush(theme()->accent);FillRect(dc,&r,brush);DeleteObject(brush);EndPaint(w,&ps);return 0;
    }
    if(msg==WM_CREATE) {
        {HMENU bar=CreateMenu();AppendMenuA(bar,MF_STRING,9010,"Transfer tools");AppendMenuA(bar,MF_STRING,9011,"Send folder...");AppendMenuA(bar,MF_STRING|(completion_sound?MF_CHECKED:0),9012,"Completion sound");SetMenu(w,bar);}
        control(w,"STATIC","Nearby computers - select one, then Send file or drop files",0,16,16,550,22,0);
        list=control(w,"LISTBOX","",WS_TABSTOP|WS_BORDER|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,16,44,548,190,10);
        control(w,"STATIC","Or connect by IPv4 address (optional :port)",0,16,246,540,20,0);
        address=control(w,"EDIT","",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,16,270,548,24,11); SendMessage(address,EM_SETLIMITTEXT,79,0);
        control(w,"BUTTON","Send file...",0,16,310,104,30,1); control(w,"BUTTON","Refresh",0,128,310,96,30,2); control(w,"BUTTON","Preferences",0,232,310,112,30,3); control(w,"BUTTON","Cancel transfer",0,352,310,116,30,4);
        control(w,"BUTTON","To tray",0,476,310,88,30,5);
        status=control(w,"STATIC","Ready - local network / unencrypted",0,16,358,548,64,0);
        DragAcceptFiles(w,TRUE); SetTimer(w,1,50,NULL); return 0;
    }
    if(msg==WM_TIMER && !in_offer) {
        const BSStatus *s; char text[600],label[256]; bs_tick(client);if(bs_completion_notice(client,&completion_serial) && completion_sound)MessageBeep(MB_OK); if(peers_dirty) { catalog(); peers_dirty=0; } s=bs_status(client); local(s->message,label,sizeof label);
        if(s->active) {unsigned long position=1,total=1,done=s->transferred>s->total?s->total:s->transferred,pct=0;char filename[256],heading[320];
         local(s->filename,filename,sizeof filename);if(!s->incoming && bs_queue_count(client))bs_queue_progress(client,&position,&total);
         if(s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0))wsprintfA(heading,"Receiving file %d/%d: %.190s",bs_batch_index(client)+1,bs_batch_count(client),filename);else if(s->incoming)wsprintfA(heading,"Receiving: %.190s",filename);else wsprintfA(heading,"Sending file %lu/%lu: %.190s",position,total,filename);
         if(s->total)pct=s->total<=42949672UL?done*100/s->total:done/(s->total/100);if(pct>100)pct=100;
         wsprintfA(text,"%s\r\n%lu / %lu bytes - %lu%% - %lu bytes/s\r\n%d waiting%s",heading,done,s->total,pct,bs_transfer_rate(client),(s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)?bs_batch_count(client)-bs_batch_index(client)-1:bs_queue_waiting(client)),bs_queue_paused(client)?" - stops after this file":"");
        } else wsprintfA(text,"%s\r\n%d waiting%s",label,bs_queue_waiting(client),bs_queue_paused(client)?" - paused":"");
        {RECT r={16,424,564,431};InvalidateRect(w,&r,FALSE);}
        {char summary[256];bs_progress_summary(client,summary,sizeof summary);if(strlen(text)+strlen(summary)+3<sizeof text){strcat(text,"\r\n");strcat(text,summary);}SetWindowTextA(status,text);} offer(); return 0;
    }
    if(msg==WM_DROPFILES) {
        HDROP drop=(HDROP)wp; char path[MAX_PATH];
        UINT i,count=DragQueryFileA(drop,0xffffffff,NULL,0);
        if(count!=1) MessageBoxA(w,"Drop one file or directory.","BitSkim",MB_OK);
        else for(i=0;i<count;i++) {if(DragQueryFileA(drop,i,NULL,0)>=MAX_PATH)break;DragQueryFileA(drop,i,path,sizeof path);send_file(path);} DragFinish(drop); return 0;
    }
    if(msg==WM_COMMAND) switch(LOWORD(wp)) {
        case 9011:send_folder();break;
        case 9010:bs_tools_open(window,instance,client);break;
        case 9012:completion_sound=!completion_sound;WritePrivateProfileStringA("BitSkim","CompletionSound",completion_sound?"1":"0",ini);CheckMenuItem(GetMenu(w),9012,MF_BYCOMMAND|(completion_sound?MF_CHECKED:MF_UNCHECKED));break;
        case 1: send_file(NULL); break; case 2: case TRAY_REFRESH: bs_refresh(client); break;
        case TRAY_SHOW: show_main(); break;
        case TRAY_PREFS: show_main();
        /* fall through */
        case 3: if(!prefs) { prefs=CreateWindowA("BitSkimLegacyPrefs","BitSkim Preferences",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,370,450,w,NULL,instance,NULL); EnableWindow(w,FALSE); ShowWindow(prefs,SW_SHOW); } break;
        case 4: case TRAY_CANCEL: bs_cancel(client); break;
        case 5: hide_main(); break;
        case TRAY_QUIT: if(!prefs && (!bs_status(client)->active || MessageBoxA(w,"Cancel the transfer and quit?","BitSkim",MB_YESNO)==IDYES)) DestroyWindow(w); break;
    }
    if(msg==WM_CLOSE) { if(close_tray) { hide_main(); return 0; } if(bs_status(client)->active && MessageBoxA(w,"Cancel the transfer and quit?","BitSkim",MB_YESNO)!=IDYES) return 0; DestroyWindow(w); return 0; }
    if(msg==WM_DESTROY) { KillTimer(w,1); if(tray_present) Shell_NotifyIconA(NIM_DELETE,&tray_data); tray_present=0; PostQuitMessage(0); return 0; }
    return DefWindowProcA(w,msg,wp,lp);
}
/* Per-user configuration is separate from the read-only program directory. */
static int settings_location(void) {
    char folder[MAX_PATH],old_ini[MAX_PATH],*slash;LPITEMIDLIST pidl=NULL;LPMALLOC allocator=NULL;DWORD length;
    folder[0]=0;
    if(SUCCEEDED(SHGetSpecialFolderLocation(NULL,CSIDL_APPDATA,&pidl))) {
        SHGetPathFromIDListA(pidl,folder);
        if(SUCCEEDED(SHGetMalloc(&allocator))){allocator->lpVtbl->Free(allocator,pidl);allocator->lpVtbl->Release(allocator);}
    }
    if(!folder[0]) {length=GetEnvironmentVariableA("APPDATA",folder,sizeof folder);if(!length || length>=sizeof folder)folder[0]=0;}
    /* Unprofiled Windows 95 has no user AppData folder. */
    if(!folder[0]) {length=GetWindowsDirectoryA(folder,sizeof folder);if(!length || length>=sizeof folder)return 0;}
    if(strlen(folder)>MAX_PATH-36)return 0;
    strcat(folder,"\\BitSkim Classic");if(!CreateDirectoryA(folder,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS)return 0;
    strcpy(ini,folder);strcat(ini,"\\preferences.ini");
    if(GetFileAttributesA(ini)==INVALID_FILE_ATTRIBUTES){strcpy(old_ini,folder);strcat(old_ini,"\\BitSkim.ini");if(GetFileAttributesA(old_ini)!=INVALID_FILE_ATTRIBUTES && !CopyFileA(old_ini,ini,TRUE))return 0;}
    if(GetFileAttributesA(ini)==INVALID_FILE_ATTRIBUTES && GetModuleFileNameA(NULL,old_ini,sizeof old_ini)) {
        slash=strrchr(old_ini,'\\');if(slash && slash-old_ini<MAX_PATH-15){strcpy(slash+1,"BitSkim.ini");if(GetFileAttributesA(old_ini)!=INVALID_FILE_ATTRIBUTES && !CopyFileA(old_ini,ini,TRUE))return 0;}
    }
    return 1;
}
int WINAPI WinMain(HINSTANCE h,HINSTANCE old,LPSTR cmd,int show) {
    WNDCLASSA wc; MSG msg; BSConfig config; char label[128]; int i; (void)old;(void)cmd;
    instance=h;if(!settings_location()){MessageBoxA(NULL,"Cannot create the per-user AppData configuration folder.","BitSkim",MB_OK|MB_ICONERROR);return 1;}
    if(!bs_platform_init()) { MessageBoxA(NULL,"TCP/IP Winsock is unavailable.","BitSkim",MB_OK); return 1; }
    bs_hostname(name,sizeof name); local(name,label,sizeof label); GetPrivateProfileStringA("BitSkim","Name",label,label,sizeof label,ini); if(!from_ansi(label,name,sizeof name)) strcpy(name,"BitSkim");
    GetPrivateProfileStringA("BitSkim","ID","",identity,sizeof identity,ini);
    if(strlen(identity)!=32) { DWORD n=GetTickCount(); wsprintfA(identity,"%08lx%08lx%08lx%08lx",n,GetCurrentProcessId(),(DWORD)time(NULL),n^0x42534b31UL); WritePrivateProfileStringA("BitSkim","ID",identity,ini); }
    GetPrivateProfileStringA("BitSkim","SendLimitKiB","0",label,sizeof label,ini);if(!bs_limit_parse(label,&send_limit))send_limit=0;
    GetPrivateProfileStringA("BitSkim","ReceiveLimitKiB","0",label,sizeof label,ini);if(!bs_limit_parse(label,&receive_limit))receive_limit=0;
    visible=GetPrivateProfileIntA("BitSkim","Visible",1,ini); receiving=GetPrivateProfileIntA("BitSkim","Receiving",1,ini); automatic=GetPrivateProfileIntA("BitSkim","AutoAccept",0,ini);
    completion_sound=GetPrivateProfileIntA("BitSkim","CompletionSound",0,ini)!=0;
    start_tray=GetPrivateProfileIntA("BitSkim","StartTray",0,ini); close_tray=GetPrivateProfileIntA("BitSkim","CloseTray",1,ini);
    GetPrivateProfileStringA("BitSkim","Theme","mocha",label,sizeof label,ini);
    for(i=0;i<WIN_THEME_COUNT;i++) if(!strcmp(label,win_themes[i].id)) theme_index=i;
    apply_theme(); taskbar_created=RegisterWindowMessageA("TaskbarCreated");
    memset(&config,0,sizeof config); strcpy(config.name,name); strcpy(config.id,identity); strcpy(config.platform,"Windows Classic"); config.port=config.discovery_port=BS_PORT; config.visible=visible; config.receiving=receiving;
    client=bs_create(&config,changed,NULL); if(!client) { MessageBoxA(NULL,"Cannot listen on port 46440. Close any other BitSkim instance.","BitSkim",MB_OK); bs_platform_cleanup(); return 1; }
    {char store[BS_PATH_MAX+1],*slash;
        if(from_ansi(ini,store,sizeof store)) {slash=strrchr(store,92);if(slash)strcpy(slash+1,"BSFOLD.DAT");bs_folders_configure(client,store);}
    }
    bs_limits_configure(client,send_limit,receive_limit);bs_batch_enable(client,1);
    memset(&wc,0,sizeof wc); wc.hInstance=h; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hIcon=LoadIconA(h,MAKEINTRESOURCEA(1)); wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.lpfnWndProc=procedure; wc.lpszClassName="BitSkimLegacy"; RegisterClassA(&wc); wc.lpfnWndProc=prefproc; wc.lpszClassName="BitSkimLegacyPrefs"; RegisterClassA(&wc);wc.lpfnWndProc=network_proc;wc.lpszClassName="BitSkimLegacyNetwork";RegisterClassA(&wc);
    window=CreateWindowA("BitSkimLegacy","BitSkim 0.2 - Windows Classic",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,600,500,NULL,NULL,h,NULL); if(!window) { bs_destroy(client); bs_platform_cleanup(); return 1; }
    add_tray(); if(start_tray && tray_present) ShowWindow(window,SW_HIDE); else ShowWindow(window,show);
    while(GetMessageA(&msg,NULL,0,0)>0) { if(bs_tools_window && IsDialogMessageA(bs_tools_window,&msg))continue; if(!IsDialogMessageA(network_window?network_window:prefs?prefs:window,&msg)) { TranslateMessage(&msg); DispatchMessageA(&msg); } }
    bs_destroy(client); bs_platform_cleanup(); DeleteObject(base_brush); DeleteObject(surface_brush); return 0;
}
