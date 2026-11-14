/* BitSkim 0.2: native Windows 10 x64 desktop client. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "bitskim.h"
#include "../shared/single-send.h"
#include "platform.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <bcrypt.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <windowsx.h>
#include "themes.h"
#include "windows-names.h"
#include "window-fit.h"
#define ID_ICON 1
#define WM_TRAY (WM_APP+1)
#define WM_ENGINE (WM_APP+2)
#define WM_OFFER (WM_APP+3)
#define ID_PEERS 100
#define ID_IP 101
#define ID_SEND 102
#define ID_REFRESH 103
#define ID_PREFS 104
#define ID_CANCEL 105
#define ID_HIDE 106
#define ID_QUIT 107
#define ID_RECEIVE 108
#define ID_QUEUE 109
#define PREF_SECURITY 217
#define PREF_KEY 218
#define PREF_GENERATE 219
#define PREF_RESUME 220
#define PREF_NAV 200
#define PREF_NAME 201
#define PREF_VISIBLE 202
#define PREF_THEME 203
#define PREF_RECEIVE 204
#define PREF_AUTO 205
#define PREF_TRUSTED_AUTO 232
#define PREF_ASK 206
#define PREF_FOLDER 207
#define PREF_BROWSE 208
#define PREF_TRAY 209
#define PREF_START 210
#define PREF_SAVE 211
#define PREF_CANCEL 212
#define PREF_REVERT 213
#define PREF_PREVIEW 214
#define PREF_GITHUB 215
#define PREF_WEBSITE 216
#define PREF_SEND_LIMIT 230
#define PREF_RECEIVE_LIMIT 231
#define PATH_CAP (BS_PATH_MAX+1)
typedef struct {
    wchar_t name[64],folder[PATH_CAP],key[65]; char id[33];
    int visible,receiving,automatic,trusted_auto,ask,tray,start,theme,security,resume;
    unsigned long send_limit,receive_limit;
} Preferences;
static HINSTANCE instance;
static HWND main_window,prefs_window,peer_list,ip_edit,status_text,progress_bar,amount_text,speed_text;
static HWND pref_nav,pref_controls[32]; static int pref_control_count,pref_section,pref_bad_limits;
static Preferences prefs,draft;
static BSClient *client;
static HICON app_icon;
static HFONT font,title_font;
static HBRUSH base_brush[WIN_THEME_COUNT],surface_brush[WIN_THEME_COUNT];
static wchar_t ini_path[PATH_CAP];
static int completion_alerts;static unsigned long completion_serial;
static NOTIFYICONDATAW tray_data;
static int tray_added,peer_dirty=1,offer_pending,handling_offer,dpi=96;
static UINT taskbar_created;
static BSPeer displayed_peers[BS_MAX_PEERS];
static int displayed_count,drop_hover=-1,drop_active;
static HIMAGELIST peer_images;
static void send_file(const wchar_t *dropped);
static const wchar_t *sections[]={L"General",L"Appearance",L"Receiving",L"Startup",L"Security",L"Network",L"About"};
static int S(int n) { return MulDiv(n,dpi,96); }
static void text(HWND w,const wchar_t *s) { SetWindowTextW(w,s); }
static int utf8(const wchar_t *s,char *out,int cap) { return WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,s,-1,out,cap,NULL,NULL)!=0; }
static void utf16(const char *s,wchar_t *out,int cap) { if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,out,cap)) lstrcpynW(out,L"Unrepresentable text",cap); }
static void error_box(HWND owner,const wchar_t *message) { MessageBoxW(owner,message,L"BitSkim 0.2",MB_OK|MB_ICONEXCLAMATION); }
static void core_error(void) { wchar_t message[512]; utf16(bs_last_error(client),message,512); error_box(main_window,message); }
static int theme_for(HWND owner) { return (prefs_window && (owner==prefs_window || GetParent(owner)==prefs_window))?draft.theme:prefs.theme; }
static HWND control(HWND parent,const wchar_t *cls,const wchar_t *label,DWORD style,int id) {
    HWND w=CreateWindowExW(0,cls,label,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,instance,NULL);
    SendMessageW(w,WM_SETFONT,(WPARAM)font,TRUE); return w;
}
static HWND button(HWND parent,const wchar_t *label,int id) { return control(parent,L"BUTTON",label,WS_TABSTOP|BS_OWNERDRAW,id); }
static void place(HWND w,int x,int y,int width,int height) { MoveWindow(w,S(x),S(y),S(width),S(height),TRUE); }
static void checked(HWND w,int value) { SetPropW(w,L"BitSkimChecked",(HANDLE)(INT_PTR)(value+1)); InvalidateRect(w,NULL,TRUE); }
static int is_checked(HWND w) { return (int)(INT_PTR)GetPropW(w,L"BitSkimChecked")-1; }
static void draw_text(HDC dc,const wchar_t *s,RECT r,COLORREF color,UINT flags) {
    SetTextColor(dc,color); SetBkMode(dc,TRANSPARENT); DrawTextW(dc,s,-1,&r,flags);
}
static void fill(HDC dc,RECT r,COLORREF color) { HBRUSH b=CreateSolidBrush(color); FillRect(dc,&r,b); DeleteObject(b); }
static void draw_control(DRAWITEMSTRUCT *d,HWND owner) {
    const WinTheme *t=&win_themes[theme_for(owner)]; RECT r=d->rcItem; wchar_t label[512]; int chosen=d->itemState&ODS_SELECTED;
    HFONT old=(HFONT)SelectObject(d->hDC,font);
    if(d->CtlID==ID_PEERS || d->CtlID==PREF_NAV || d->CtlID==PREF_THEME) {
        if(d->itemID==(UINT)-1) { FillRect(d->hDC,&r,surface_brush[theme_for(owner)]); SelectObject(d->hDC,old); return; }
        if(d->CtlID==PREF_THEME) lstrcpynW(label,win_themes[d->itemID<WIN_THEME_COUNT?d->itemID:0].name,512);
        else SendMessageW(d->hwndItem,LB_GETTEXT,d->itemID,(LPARAM)label);
        if(d->CtlID==ID_PEERS) {
            wchar_t *detail=wcsstr(label,L"  |  "); RECT line=r, stripe=r;
            fill(d->hDC,r,t->surface);
            if(chosen) { stripe.right=stripe.left+S(3); fill(d->hDC,stripe,t->accent); }
            line.left+=S(14); line.right-=S(14); line.top+=S(5); line.bottom=line.top+S(20);
            if(detail) { *detail=0; detail+=5; }
            draw_text(d->hDC,label,line,chosen?t->accent:t->text,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            if(detail) { line.top+=S(21); line.bottom=line.top+S(18); draw_text(d->hDC,detail,line,t->muted,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS); }
        } else if(d->CtlID==PREF_NAV) {
            RECT stripe=r;
            fill(d->hDC,r,chosen?t->surface:t->base);
            if(chosen) { stripe.right=stripe.left+S(3); fill(d->hDC,stripe,t->accent); }
            r.left+=S(12); r.right-=S(8);
            draw_text(d->hDC,label,r,chosen?t->accent:t->text,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        } else {
            fill(d->hDC,r,chosen?t->accent:t->surface); r.left+=S(12); r.right-=S(8);
            draw_text(d->hDC,label,r,chosen?t->base:t->text,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        }
    } else {
        GetWindowTextW(d->hwndItem,label,512);
        if(GetPropW(d->hwndItem,L"BitSkimChecked")) {
            RECT box=r; box.left+=S(2); box.top+=(r.bottom-r.top-S(18))/2; box.right=box.left+S(18); box.bottom=box.top+S(18);
            fill(d->hDC,r,t->base); fill(d->hDC,box,is_checked(d->hwndItem)?t->accent:t->surface);
            if(is_checked(d->hwndItem)) {
                HPEN pen=CreatePen(PS_SOLID,S(2),t->base),oldpen=(HPEN)SelectObject(d->hDC,pen);
                MoveToEx(d->hDC,box.left+S(4),box.top+S(9),NULL); LineTo(d->hDC,box.left+S(8),box.top+S(13)); LineTo(d->hDC,box.left+S(15),box.top+S(5));
                SelectObject(d->hDC,oldpen); DeleteObject(pen);
            }
            r.left=box.right+S(9); draw_text(d->hDC,label,r,(d->itemState&ODS_DISABLED)?t->muted:t->text,DT_LEFT|DT_SINGLELINE|DT_VCENTER);
        } else {
            int primary=d->CtlID==ID_SEND || d->CtlID==PREF_SAVE;
            fill(d->hDC,r,(chosen||primary)?t->accent:t->surface);
            draw_text(d->hDC,label,r,(d->itemState&ODS_DISABLED)?t->muted:((chosen||primary)?t->base:t->text),DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        }
    }
    if((d->itemState&ODS_FOCUS) && !(d->itemState&ODS_NOFOCUSRECT)) { HBRUSH outline=CreateSolidBrush(t->accent); r=d->rcItem; InflateRect(&r,-S(2),-S(2)); FrameRect(d->hDC,&r,outline); DeleteObject(outline); }
    SelectObject(d->hDC,old);
}
static void notify(const wchar_t *title,const wchar_t *message) {
    if(!tray_added) return;
    tray_data.uFlags=NIF_INFO; lstrcpynW(tray_data.szInfoTitle,title,64); lstrcpynW(tray_data.szInfo,message,256); tray_data.dwInfoFlags=NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY,&tray_data);
}
static void update_tray(void) {
    if(prefs.tray && !tray_added) {
        ZeroMemory(&tray_data,sizeof tray_data); tray_data.cbSize=sizeof tray_data; tray_data.hWnd=main_window; tray_data.uID=1;
        tray_data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP; tray_data.uCallbackMessage=WM_TRAY; tray_data.hIcon=app_icon;
        lstrcpynW(tray_data.szTip,L"BitSkim 0.2 - local file sharing",128); tray_added=Shell_NotifyIconW(NIM_ADD,&tray_data)!=FALSE;
    } else if(!prefs.tray && tray_added) { Shell_NotifyIconW(NIM_DELETE,&tray_data); tray_added=0; }
}
static void show_main(void) { ShowWindow(main_window,SW_RESTORE); SetForegroundWindow(main_window); }
static void quit(void) {
    if(bs_status(client)->active && MessageBoxW(main_window,L"Cancel the current transfer and quit?",L"BitSkim",MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
    DestroyWindow(main_window);
}
static void load_preferences(void) {
    wchar_t appdata[PATH_CAP],value[128]; PWSTR downloads=NULL; unsigned char bytes[16]; char name[64]; int i;
    ZeroMemory(&prefs,sizeof prefs); prefs.security=prefs.resume=1; prefs.visible=prefs.receiving=prefs.ask=prefs.tray=1; prefs.theme=WIN_THEME_DEFAULT;
    bs_hostname(name,sizeof name); utf16(name,prefs.name,64);
    if(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_Downloads,0,NULL,&downloads))) { lstrcpynW(prefs.folder,downloads,PATH_CAP); CoTaskMemFree(downloads); }
    else SHGetFolderPathW(NULL,CSIDL_PERSONAL,NULL,SHGFP_TYPE_CURRENT,prefs.folder);
    if(FAILED(SHGetFolderPathW(NULL,CSIDL_APPDATA|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,appdata)) || wcslen(appdata)+32>=PATH_CAP) ini_path[0]=0;
    else { swprintf(ini_path,PATH_CAP,L"%ls\\BitSkim",appdata); CreateDirectoryW(ini_path,NULL); wcscat(ini_path,L"\\preferences.ini"); }
    if(BCryptGenRandom(NULL,bytes,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) { error_box(NULL,L"Could not create a client identity."); ExitProcess(1); }
    for(i=0;i<16;i++) sprintf(prefs.id+i*2,"%02x",bytes[i]);
    if(!ini_path[0]) return;
    GetPrivateProfileStringW(L"BitSkim",L"Name",prefs.name,value,128,ini_path); lstrcpynW(prefs.name,value,64);
    GetPrivateProfileStringW(L"BitSkim",L"Folder",prefs.folder,appdata,PATH_CAP,ini_path); lstrcpynW(prefs.folder,appdata,PATH_CAP);
    GetPrivateProfileStringW(L"BitSkim",L"ID",L"",value,128,ini_path);
    if(wcslen(value)==32) { int valid=1; for(i=0;i<32;i++) if(!((value[i]>=L'0'&&value[i]<=L'9')||(value[i]>=L'a'&&value[i]<=L'f'))) valid=0; if(valid) for(i=0;i<33;i++) prefs.id[i]=(char)value[i]; }
#define READ_BOOL(field,key,def) prefs.field=GetPrivateProfileIntW(L"BitSkim",key,def,ini_path)!=0
    READ_BOOL(visible,L"Visible",1); READ_BOOL(receiving,L"Receiving",1); READ_BOOL(automatic,L"AutoAccept",0); READ_BOOL(trusted_auto,L"TrustedAutoAcceptOnly",1); READ_BOOL(ask,L"AskLocation",1);
    READ_BOOL(resume,L"Resume",1);
    READ_BOOL(tray,L"KeepTray",1); READ_BOOL(start,L"StartInTray",0);
#undef READ_BOOL
    {char number[32];
     GetPrivateProfileStringW(L"BitSkim",L"SendLimitKiB",L"0",value,128,ini_path);if(!utf8(value,number,sizeof number) || !bs_limit_parse(number,&prefs.send_limit))prefs.send_limit=0;
     GetPrivateProfileStringW(L"BitSkim",L"ReceiveLimitKiB",L"0",value,128,ini_path);if(!utf8(value,number,sizeof number) || !bs_limit_parse(number,&prefs.receive_limit))prefs.receive_limit=0;}
    completion_alerts=GetPrivateProfileIntW(L"BitSkim",L"CompletionAlerts",1,ini_path)!=0;
    prefs.security=(int)GetPrivateProfileIntW(L"BitSkim",L"Security",1,ini_path);
    GetPrivateProfileStringW(L"BitSkim",L"PairingKey",L"",prefs.key,65,ini_path);
    prefs.theme=(int)GetPrivateProfileIntW(L"BitSkim",L"Theme",WIN_THEME_DEFAULT,ini_path); if(prefs.theme<0 || prefs.theme>=WIN_THEME_COUNT) prefs.theme=WIN_THEME_DEFAULT;
    if(!prefs.tray) prefs.start=0;
}
static int save_preferences(void) {
    wchar_t value[64]; int ok=1; HANDLE file;
    if(!ini_path[0]) return 0;
    if(GetFileAttributesW(ini_path)==INVALID_FILE_ATTRIBUTES) {
        WORD bom=0xfeff; DWORD written;
        file=CreateFileW(ini_path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) { WriteFile(file,&bom,2,&written,NULL); CloseHandle(file); }
    }
#define WRITE_STR(key,data) do { if(!WritePrivateProfileStringW(L"BitSkim",key,data,ini_path)) ok=0; } while(0)
#define WRITE_NUM(key,data) do { swprintf(value,64,L"%d",data); WRITE_STR(key,value); } while(0)
    WRITE_STR(L"Name",prefs.name); WRITE_STR(L"Folder",prefs.folder); utf16(prefs.id,value,64); WRITE_STR(L"ID",value);
    WRITE_NUM(L"Visible",prefs.visible); WRITE_NUM(L"Receiving",prefs.receiving); WRITE_NUM(L"AutoAccept",prefs.automatic); WRITE_NUM(L"TrustedAutoAcceptOnly",prefs.trusted_auto); WRITE_NUM(L"AskLocation",prefs.ask);
    WRITE_NUM(L"SendLimitKiB",prefs.send_limit/1024UL);WRITE_NUM(L"ReceiveLimitKiB",prefs.receive_limit/1024UL);
    WRITE_NUM(L"Security",prefs.security); WRITE_NUM(L"Resume",prefs.resume); WRITE_STR(L"PairingKey",prefs.key);
    WRITE_NUM(L"KeepTray",prefs.tray); WRITE_NUM(L"StartInTray",prefs.start); WRITE_NUM(L"Theme",prefs.theme);
#undef WRITE_STR
#undef WRITE_NUM
    return ok;
}
static int file_dialog(int save,wchar_t *path,const wchar_t *filename) {
    OPENFILENAMEW ofn; wchar_t recent[PATH_CAP];utf16(bs_last_folder(client,save),recent,PATH_CAP); ZeroMemory(&ofn,sizeof ofn); path[0]=0; if(filename) lstrcpynW(path,filename,PATH_CAP);
    ofn.lStructSize=sizeof ofn; ofn.hwndOwner=main_window; ofn.lpstrFile=path; ofn.nMaxFile=PATH_CAP;
    ofn.lpstrFilter=L"All files\0*.*\0\0"; ofn.lpstrInitialDir=recent[0]?recent:prefs.folder;
    ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_HIDEREADONLY|(save?OFN_NOREADONLYRETURN:OFN_FILEMUSTEXIST);
    return save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn);
}
static void send_file(const wchar_t *dropped) {
    wchar_t target[80]; char source[PATH_CAP],address[80],peer_id[33]="",*colon; unsigned short port=BS_PORT; int index;
    const BSPeer *peer=NULL;
    
    GetWindowTextW(ip_edit,target,80); index=ListView_GetNextItem(peer_list,-1,LVNI_SELECTED);
    if(target[0]) {
        if(!utf8(target,address,sizeof address)) return;
        colon=strchr(address,':'); if(colon) { char *end; long p; *colon++=0; p=strtol(colon,&end,10); if(*end || p<1 || p>65535) { error_box(main_window,L"Enter a valid port from 1 to 65535."); return; } port=(unsigned short)p; }
    } else {
        if(index>=0 && index<displayed_count) peer=&displayed_peers[index];
        if(!peer) { error_box(main_window,L"Choose a nearby computer or enter an IPv4 address first."); return; }
        lstrcpynA(address,peer->address,sizeof address); port=peer->port;
    }
    if(peer) strcpy(peer_id,peer->id); /* picker can service discovery events */
    if(dropped) {
        if(!prepare_single_send(client)) {error_box(main_window,L"A transfer is already in progress.");return;}
        if(!utf8(dropped,source,sizeof source)) { error_box(main_window,L"This path exceeds the 1023-byte UTF-8 path limit."); return; }
        if(!(peer_id[0]?bs_queue_peer(client,peer_id,source):bs_queue_address(client,address,port,source))) core_error();
    } else {
        wchar_t selection[PATH_CAP]={0},recent[PATH_CAP]; OPENFILENAMEW ofn;utf16(bs_last_folder(client,0),recent,PATH_CAP);
        ZeroMemory(&ofn,sizeof ofn);
        ofn.lpstrTitle=L"Choose a file to send";ofn.lpstrInitialDir=recent;
        ofn.lStructSize=sizeof ofn; ofn.hwndOwner=main_window; ofn.lpstrFile=selection; ofn.nMaxFile=PATH_CAP;
        ofn.lpstrFilter=L"All files\0*.*\0\0"; ofn.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;
        if(!GetOpenFileNameW(&ofn)) return;
        if(!prepare_single_send(client)) {error_box(main_window,L"A transfer is already in progress.");return;}
        if((!utf8(selection,source,sizeof source) ||
           !(peer_id[0]?bs_queue_peer(client,peer_id,source):bs_queue_address(client,address,port,source)))) core_error();
    }
}

static int peer_tile_rect(int index,RECT *r) {
    RECT icon; int center;
    if(!ListView_GetItemRect(peer_list,index,&icon,LVIR_ICON)) return 0;
    center=(icon.left+icon.right)/2;
    *r=(RECT){center-S(88),icon.top-S(2),center+S(88),icon.top+S(138)};
    return 1;
}
static int peer_at(POINT point) {
    int i; RECT r;
    for(i=0;i<displayed_count;i++) if(peer_tile_rect(i,&r)) {
        if(PtInRect(&r,point)) return i;
    }
    return -1;
}
static void hover_peer(int index) {
    if(drop_hover!=index) { drop_hover=index; InvalidateRect(peer_list,NULL,FALSE); }
}
static void drop_file(HDROP drop,int index) {
    wchar_t path[PATH_CAP]; char id[33],source[PATH_CAP];
    if(index<0 || index>=displayed_count) return;
    strcpy(id,displayed_peers[index].id);
    UINT i,count=DragQueryFileW(drop,0xffffffff,NULL,0);
    if(count!=1) {error_box(main_window,L"Drop one file or directory.");return;}
    for(i=0;i<count;i++) if(DragQueryFileW(drop,i,NULL,0)>=PATH_CAP) {error_box(main_window,L"A dropped path is too long.");return;}
    if(!prepare_single_send(client)) {error_box(main_window,L"A transfer is already in progress.");return;}
    text(ip_edit,L""); ListView_SetItemState(peer_list,index,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    for(i=0;i<count;i++) {DragQueryFileW(drop,i,path,PATH_CAP);if(!utf8(path,source,sizeof source) || !bs_queue_peer(client,id,source)){core_error();break;}}
}
static LRESULT CALLBACK peer_subclass(HWND w,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    (void)id; (void)data;
    if(msg==WM_DROPFILES) {
        HDROP drop=(HDROP)wp; POINT point;
        DragQueryPoint(drop,&point); drop_file(drop,peer_at(point)); DragFinish(drop); return 0;
    }
    if(msg==WM_NCDESTROY) { RevokeDragDrop(w); RemoveWindowSubclass(w,peer_subclass,1); }
    return DefSubclassProc(w,msg,wp,lp);
}
static LRESULT draw_peer_tile(NMLVCUSTOMDRAW *draw) {
    const WinTheme *t=&win_themes[prefs.theme]; int index=(int)draw->nmcd.dwItemSpec;
    RECT r,line,bubble,monitor; HDC dc=draw->nmcd.hdc; HBRUSH brush; HPEN pen; HGDIOBJ oldbrush,oldpen,oldfont;
    wchar_t name[128],platform[64],address[64]; int chosen,hover,cx,cy;
    if(draw->nmcd.dwDrawStage==CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if(draw->nmcd.dwDrawStage!=CDDS_ITEMPREPAINT || index<0 || index>=displayed_count) return CDRF_DODEFAULT;
    if(!peer_tile_rect(index,&r)) return CDRF_DODEFAULT;
    chosen=(ListView_GetItemState(peer_list,index,LVIS_SELECTED)&LVIS_SELECTED)!=0; hover=drop_hover==index;
    InflateRect(&r,-S(4),-S(4));
    brush=CreateSolidBrush(t->surface); pen=CreatePen(PS_SOLID,S(hover?2:1),(chosen||hover)?t->accent:t->surface);
    oldbrush=SelectObject(dc,brush); oldpen=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,S(16),S(16));
    cx=(r.left+r.right)/2; cy=r.top+S(34);
    bubble=(RECT){cx-S(24),cy-S(24),cx+S(24),cy+S(24)};
    { HBRUSH circle=CreateSolidBrush(t->base); SelectObject(dc,circle); SelectObject(dc,GetStockObject(NULL_PEN)); Ellipse(dc,bubble.left,bubble.top,bubble.right,bubble.bottom); SelectObject(dc,oldbrush); DeleteObject(circle); }
    { HPEN iconpen=CreatePen(PS_SOLID,S(2),t->accent); SelectObject(dc,iconpen); SelectObject(dc,GetStockObject(HOLLOW_BRUSH));
      monitor=(RECT){cx-S(13),cy-S(11),cx+S(13),cy+S(6)}; RoundRect(dc,monitor.left,monitor.top,monitor.right,monitor.bottom,S(3),S(3));
      MoveToEx(dc,cx,cy+S(6),NULL); LineTo(dc,cx,cy+S(12)); MoveToEx(dc,cx-S(8),cy+S(12),NULL); LineTo(dc,cx+S(8),cy+S(12)); SelectObject(dc,oldpen); DeleteObject(iconpen); }
    SelectObject(dc,oldbrush); SelectObject(dc,oldpen); DeleteObject(brush); DeleteObject(pen);
    oldfont=SelectObject(dc,font); utf16(displayed_peers[index].name,name,128); utf16(displayed_peers[index].platform,platform,64); utf16(displayed_peers[index].address,address,64);
    line=(RECT){r.left+S(8),r.top+S(64),r.right-S(8),r.top+S(84)};
    draw_text(dc,name,line,(chosen||hover)?t->accent:t->text,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    line.top+=S(21); line.bottom+=S(21); draw_text(dc,platform,line,t->muted,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    line.top+=S(18); line.bottom+=S(18); draw_text(dc,address,line,t->muted,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
    if((draw->nmcd.uItemState&CDIS_FOCUS) && GetFocus()==peer_list) { RECT focus=r; InflateRect(&focus,-S(3),-S(3)); DrawFocusRect(dc,&focus); }
    SelectObject(dc,oldfont); return CDRF_SKIPDEFAULT;
}
static HRESULT STDMETHODCALLTYPE drop_query(IDropTarget *self,REFIID iid,void **out) {
    if(!out) return E_POINTER;
    if(IsEqualIID(iid,&IID_IUnknown) || IsEqualIID(iid,&IID_IDropTarget)) { *out=self; return S_OK; }
    *out=NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE drop_ref(IDropTarget *self) { (void)self; return 1; }
static int drop_supported(IDataObject *object) {
    FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; return object && SUCCEEDED(object->lpVtbl->QueryGetData(object,&format));
}
static DWORD drop_effect(POINTL position,DWORD allowed) {
    POINT point={position.x,position.y}; int index; ScreenToClient(peer_list,&point); index=peer_at(point);
    if(bs_queue_count(client)>=BS_QUEUE_MAX || !(allowed&DROPEFFECT_COPY)) index=-1;
    hover_peer(index); return index>=0?DROPEFFECT_COPY:DROPEFFECT_NONE;
}
static HRESULT STDMETHODCALLTYPE drop_enter(IDropTarget *self,IDataObject *object,DWORD keys,POINTL point,DWORD *effect) {
    (void)self; (void)keys; drop_active=drop_supported(object); *effect=drop_active?drop_effect(point,*effect):DROPEFFECT_NONE; return S_OK;
}
static HRESULT STDMETHODCALLTYPE drop_over(IDropTarget *self,DWORD keys,POINTL point,DWORD *effect) {
    (void)self; (void)keys; *effect=drop_active?drop_effect(point,*effect):DROPEFFECT_NONE; return S_OK;
}
static HRESULT STDMETHODCALLTYPE drop_leave(IDropTarget *self) { (void)self; drop_active=0; hover_peer(-1); return S_OK; }
static HRESULT STDMETHODCALLTYPE drop_receive(IDropTarget *self,IDataObject *object,DWORD keys,POINTL point,DWORD *effect) {
    FORMATETC format={CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL}; STGMEDIUM medium; int index;
    (void)self; (void)keys; *effect=drop_active?drop_effect(point,*effect):DROPEFFECT_NONE; index=drop_hover;
    if(*effect==DROPEFFECT_COPY && SUCCEEDED(object->lpVtbl->GetData(object,&format,&medium))) {
        drop_file((HDROP)medium.hGlobal,index); ReleaseStgMedium(&medium);
    } else *effect=DROPEFFECT_NONE;
    drop_active=0; hover_peer(-1); return S_OK;
}
static IDropTargetVtbl peer_drop_vtable={drop_query,drop_ref,drop_ref,drop_enter,drop_over,drop_leave,drop_receive};
static IDropTarget peer_drop_target={&peer_drop_vtable};
static void create_peer_view(HWND w) {
        peer_list=control(w,WC_LISTVIEWW,L"Nearby computers",WS_TABSTOP|LVS_ICON|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_AUTOARRANGE,ID_PEERS);
        ListView_SetExtendedListViewStyle(peer_list,LVS_EX_DOUBLEBUFFER|LVS_EX_INFOTIP);
        SetWindowTheme(peer_list,L"",L"");
        peer_images=ImageList_Create(S(152),S(112),ILC_COLOR32|ILC_MASK,1,1);
        if(peer_images) {
            HDC dc=GetDC(w),memory=CreateCompatibleDC(dc); HBITMAP bitmap=CreateCompatibleBitmap(dc,S(152),S(112));
            HGDIOBJ old=SelectObject(memory,bitmap); RECT area={0,0,S(152),S(112)}; fill(memory,area,RGB(255,0,255));
            SelectObject(memory,old); ImageList_AddMasked(peer_images,bitmap,RGB(255,0,255)); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(w,dc);
            ListView_SetImageList(peer_list,peer_images,LVSIL_NORMAL);
        }
        ListView_SetIconSpacing(peer_list,S(184),S(152));
        if(FAILED(RegisterDragDrop(peer_list,&peer_drop_target))) DragAcceptFiles(peer_list,TRUE);
        SetWindowSubclass(peer_list,peer_subclass,1,0);
}
/* Hidden native-control check: validates full tile hit regions and OLE registration. */
static int check_peer_view(void) {
    HWND parent; RECT first={0},second={0}; POINT point; LVITEMW item; int ok=0;
    if(FAILED(OleInitialize(NULL))) return 0;
    parent=CreateWindowExW(0,L"STATIC",L"BitSkim control check",WS_POPUP,0,0,640,360,NULL,NULL,instance,NULL);
    if(parent) {
        create_peer_view(parent); MoveWindow(peer_list,0,0,640,320,FALSE);
        ZeroMemory(&item,sizeof item); item.mask=LVIF_TEXT|LVIF_IMAGE; item.pszText=L"Test computer"; item.iImage=0;
        ListView_InsertItem(peer_list,&item); item.iItem=1; ListView_InsertItem(peer_list,&item);
        displayed_count=2; ListView_Arrange(peer_list,LVA_DEFAULT);
        ok=peer_tile_rect(0,&first) && peer_tile_rect(1,&second);
        point=(POINT){(first.left+first.right)/2,(first.top+first.bottom)/2};
        ok=ok && peer_at(point)==0;
        point=(POINT){second.left+8,second.bottom-8}; ok=ok && peer_at(point)==1;
        point=(POINT){630,310}; ok=ok && peer_at(point)==-1;
        ok=ok && SUCCEEDED(RevokeDragDrop(peer_list));
        DestroyWindow(parent); if(peer_images) { ImageList_Destroy(peer_images); peer_images=NULL; }
        peer_list=NULL; displayed_count=0;
    }
    OleUninitialize(); return ok;
}
#include "../shared/collection-win.h"
#include "../shared/tools-win.h"
static int CALLBACK recent_browse(HWND w,UINT msg,LPARAM arg,LPARAM context){(void)arg;if(msg==BFFM_INITIALIZED && context)SendMessageW(w,BFFM_SETSELECTIONW,TRUE,context);return 0;}
static void send_folder(void){BROWSEINFOW browse;PIDLIST_ABSOLUTE item;wchar_t path[PATH_CAP],initial[PATH_CAP];utf16(bs_last_folder(client,0),initial,PATH_CAP);ZeroMemory(&browse,sizeof browse);browse.hwndOwner=main_window;browse.lpszTitle=L"Choose a folder to send (links are not followed)";browse.lpfn=recent_browse;browse.lParam=(LPARAM)initial;browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;item=SHBrowseForFolderW(&browse);if(item){if(SHGetPathFromIDListW(item,path))send_file(path);CoTaskMemFree(item);}}
static void incoming_batch(void) {
    wchar_t folder[PATH_CAP];char destination[PATH_CAP];int accept=1,automatic=prefs.automatic && (!prefs.trusted_auto || bs_trust_status(client)==1);
    unsigned long serial=bs_transfer_serial(client);BROWSEINFOW browse;PIDLIST_ABSOLUTE item;
    if(!automatic) {show_main();accept=collection_confirm(main_window,client);}
    if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision)return;
    if(accept && !automatic)accept=bs_tools_pair_offer(main_window,client);
    if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
    if(accept) {
        utf16(bs_last_folder(client,1),folder,PATH_CAP);if(!folder[0])wcscpy(folder,prefs.folder);
        if(prefs.ask) {
            ZeroMemory(&browse,sizeof browse);browse.hwndOwner=main_window;browse.lpszTitle=L"Save all files in this folder";browse.lpfn=recent_browse;browse.lParam=(LPARAM)folder;browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            item=SHBrowseForFolderW(&browse);accept=item && SHGetPathFromIDListW(item,folder);if(item)CoTaskMemFree(item);
        }
        if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision)return;
        if(accept && (!utf8(folder,destination,sizeof destination) || !bs_tools_space(main_window,client,destination,1,!automatic)))accept=0;
        if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision)return;
        if(accept && !bs_accept_batch(client,destination)){core_error();accept=0;}
    }
    if(!accept && bs_transfer_serial(client)==serial && bs_status(client)->awaiting_decision)bs_decline(client);
}
static void incoming(void) {
    BSStatus offer=*bs_status(client); wchar_t question[1000],name[256],sender[128],path[PATH_CAP]; char destination[PATH_CAP]; int automatic=prefs.automatic && (!prefs.trusted_auto || bs_trust_status(client)==1),accept=automatic;
    unsigned long serial=bs_transfer_serial(client);
    offer_pending=0; if(!offer.awaiting_decision || handling_offer) return; handling_offer=1;
    if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)){incoming_batch();handling_offer=0;return;}
    {char mapped[256];if(!bs_suggest_name(offer.filename,0,mapped,sizeof mapped))strcpy(mapped,"received-file");utf16(mapped,name,256);} utf16(offer.peer_name,sender,128);
    if(!windows_name_supported(name)) {
        bs_decline(client); show_main();
        error_box(main_window,L"Windows cannot save this filename. Rename the file on the sender and try again.");
        handling_offer=0; return;
    }
    notify(L"Incoming file",name);
    if(!automatic || prefs.ask) show_main();
    if(!automatic) { swprintf(question,1000,L"%ls wants to send:\n%ls\n%lu bytes\n\n%ls. Sender names are not verified.",sender,name,offer.total,bs_transfer_encrypted(client)?L"TLS encrypted":L"Unencrypted"); accept=MessageBoxW(main_window,question,L"Incoming file - BitSkim",MB_YESNO|MB_ICONQUESTION)==IDYES; }
    if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
    if(accept && !automatic)accept=bs_tools_pair_offer(main_window,client);
    if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
    if(accept) {
        if(prefs.ask) accept=file_dialog(1,path,name);
        else if(wcslen(prefs.folder)+wcslen(name)+2>=PATH_CAP) accept=0;
        else swprintf(path,PATH_CAP,L"%ls\\%ls",prefs.folder,name);
        if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
        if(accept) {
            int restart=0;
            if(!utf8(path,destination,sizeof destination)) accept=0;
            else if(bs_resume_available(client,destination)==1) {
                int answer=MessageBoxW(main_window,L"Resume the retained partial file?\n\nYes: validate and resume\nNo: discard this checkpoint and restart\nCancel: decline this offer",L"Resume interrupted transfer",MB_YESNOCANCEL|MB_ICONQUESTION);
                if(answer==IDCANCEL) accept=0; else restart=answer==IDNO;
            }
            if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
            if(accept && bs_resume_available(client,destination)!=1 && !bs_tools_space(main_window,client,destination,0,!automatic))accept=0;
            if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){handling_offer=0;return;}
            if(accept && !(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES?bs_accept_conflict(client,destination):bs_accept_resumable(client,destination,restart))) {core_error();accept=0;}
        }
    }
    if(!accept && bs_transfer_serial(client)==serial && bs_status(client)->awaiting_decision) bs_decline(client);
    handling_offer=0;
}
static void engine_event(void *unused,BSEvent event) { (void)unused; if(event==BS_EVENT_PEERS) peer_dirty=1; if(main_window) PostMessageW(main_window,WM_ENGINE,event,0); }
static void byte_size(unsigned long bytes,wchar_t *out) {
    if(bytes<1024) swprintf(out,32,L"%lu B",bytes);
    else if(bytes<1048576UL) swprintf(out,32,L"%.1f KiB",bytes/1024.0);
    else if(bytes<1073741824UL) swprintf(out,32,L"%.1f MiB",bytes/1048576.0);
    else swprintf(out,32,L"%.2f GiB",bytes/1073741824.0);
}
static void refresh(void) {
    const BSStatus *s=bs_status(client); wchar_t message[512]; int i;
    static int last_transport=-1;
    int transport=s->active?(bs_transfer_encrypted(client)?1:2):prefs.security==2?3:prefs.security==1?4:0;
    if(transport!=last_transport) {RECT r;GetClientRect(main_window,&r);r.top=r.bottom-S(30);InvalidateRect(main_window,&r,FALSE);last_transport=transport;}
    if(peer_dirty && !drop_active) {
        int selected=ListView_GetNextItem(peer_list,-1,LVNI_SELECTED); char identity[33]="";
        if(selected>=0 && selected<displayed_count) strcpy(identity,displayed_peers[selected].id);
        SendMessageW(peer_list,WM_SETREDRAW,FALSE,0); ListView_DeleteAllItems(peer_list);
        displayed_count=bs_peer_count(client); if(displayed_count>BS_MAX_PEERS) displayed_count=BS_MAX_PEERS;
        for(i=0;i<displayed_count;i++) {
            LVITEMW item; wchar_t name[128],platform[64],ip[64]; const BSPeer *p=bs_peer(client,i);
            displayed_peers[i]=*p;
            utf16(p->name,name,128); utf16(p->platform,platform,64); utf16(p->address,ip,64);
            swprintf(message,512,L"%ls\r\n%ls - %ls",name,platform,ip);
            ZeroMemory(&item,sizeof item); item.mask=LVIF_TEXT|LVIF_IMAGE; item.iItem=i; item.pszText=message; item.iImage=0;
            ListView_InsertItem(peer_list,&item);
            if(!strcmp(identity,p->id)) ListView_SetItemState(peer_list,i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        }
        SendMessageW(peer_list,WM_SETREDRAW,TRUE,0); InvalidateRect(peer_list,NULL,TRUE); peer_dirty=0;
    }
    utf16(s->message,message,512);{char code[20];if(s->active && bs_pairing_code(client,code)){wchar_t wide[24];utf16(code,wide,24);wcscat(message,L"  |  Verify: ");wcscat(message,wide);}}text(status_text,message);
    if(s->active) {
        wchar_t done[32],total[32],left[32],rate[32],name[256];
        unsigned long transferred=s->transferred>s->total?s->total:s->transferred;
        byte_size(transferred,done); byte_size(s->total,total); byte_size(s->total-transferred,left); byte_size(bs_transfer_rate(client),rate);
        swprintf(message,512,L"%ls / %ls  |  %lu%%  |  %ls left",done,total,s->total?(unsigned long)((uint64_t)transferred*100/s->total):0,left); text(amount_text,message);
        swprintf(message,512,L"%ls/s",rate); {char summary[256];bs_progress_summary(client,summary,sizeof summary);utf16(summary,message,512);}text(speed_text,message);
        utf16(s->filename,name,256); utf16(s->message,message,512);
        { wchar_t title[800];unsigned long position=1,count=1;
          if(!s->incoming && bs_queue_count(client))bs_queue_progress(client,&position,&count);
          if(s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0))swprintf(title,800,L"Receiving file %d/%d: %ls",bs_batch_index(client)+1,bs_batch_count(client),name);
          else if(s->incoming)swprintf(title,800,L"Receiving: %ls",name);
          else swprintf(title,800,L"Sending file %lu/%lu: %ls",position,count,name);
          text(status_text,title); }
    }
    ShowWindow(amount_text,s->active?SW_SHOW:SW_HIDE);
    ShowWindow(progress_bar,s->active?SW_SHOW:SW_HIDE);
    SendMessageW(progress_bar,PBM_SETPOS,s->total?(WPARAM)((uint64_t)s->transferred*100/s->total):0,0);
    EnableWindow(GetDlgItem(main_window,ID_CANCEL),s->active || bs_queue_scanning(client)); EnableWindow(GetDlgItem(main_window,ID_SEND),bs_queue_count(client)<BS_QUEUE_MAX);
    {wchar_t rate[32];byte_size(bs_transfer_rate(client),rate);
     swprintf(message,512,L"%ls%ls%d waiting%ls",s->active?rate:L"",s->active?L"/s  |  ":L"",(s->active && s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)?bs_batch_count(client)-bs_batch_index(client)-1:bs_queue_waiting(client)),bs_queue_paused(client)?(s->active?L"  |  Stops after this file":L"  |  Queue paused"):L"");{char summary[256];bs_progress_summary(client,summary,sizeof summary);utf16(summary,message,512);}text(speed_text,message);}
    ShowWindow(speed_text,SW_SHOW);
    if(s->awaiting_decision && !offer_pending && !handling_offer) { offer_pending=1; PostMessageW(main_window,WM_OFFER,0,0); }
}
static BOOL CALLBACK repaint_child(HWND w,LPARAM unused) { (void)unused; InvalidateRect(w,NULL,TRUE); return TRUE; }
static void repaint(HWND w) {
    BOOL dark=win_themes[theme_for(w)].dark;
    if(w==main_window && peer_list) {
        ListView_SetBkColor(peer_list,win_themes[prefs.theme].base);
        ListView_SetTextBkColor(peer_list,win_themes[prefs.theme].base);
        ListView_SetTextColor(peer_list,win_themes[prefs.theme].text);
    }
    if(w==main_window && progress_bar) {
        SendMessageW(progress_bar,PBM_SETBARCOLOR,0,win_themes[prefs.theme].accent);
        SendMessageW(progress_bar,PBM_SETBKCOLOR,0,win_themes[prefs.theme].surface);
    }
    DwmSetWindowAttribute(w,20,&dark,sizeof dark); InvalidateRect(w,NULL,TRUE); EnumChildWindows(w,repaint_child,0);
}
static void layout_main(void) {
    RECT r; int width,height; GetClientRect(main_window,&r); width=MulDiv(r.right,96,dpi); height=MulDiv(r.bottom,96,dpi);
    place(GetDlgItem(main_window,ID_REFRESH),width-104,20,84,30);
    place(peer_list,20,96,width-40,height-330);
    place(ip_edit,20,height-196,width-40,30);
    place(GetDlgItem(main_window,ID_SEND),20,height-152,112,32); place(GetDlgItem(main_window,ID_PREFS),140,height-152,104,32);
    place(GetDlgItem(main_window,ID_CANCEL),252,height-152,80,32); place(GetDlgItem(main_window,ID_HIDE),340,height-152,100,32);
    place(status_text,20,height-110,width-40,22);
    place(amount_text,20,height-84,width-40,20); place(speed_text,20,height-62,width-40,18); place(progress_bar,20,height-42,width-40,5);
    InvalidateRect(main_window,NULL,TRUE);
}
static void capture_page(void) {
    HWND w;
    if(pref_section==0) { GetWindowTextW(GetDlgItem(prefs_window,PREF_NAME),draft.name,64); draft.visible=is_checked(GetDlgItem(prefs_window,PREF_VISIBLE)); }
    else if(pref_section==1) { int value=(int)SendDlgItemMessageW(prefs_window,PREF_THEME,CB_GETCURSEL,0,0); if(value>=0 && value<WIN_THEME_COUNT) draft.theme=value; }
    else if(pref_section==2) {
        draft.receiving=is_checked(GetDlgItem(prefs_window,PREF_RECEIVE)); draft.automatic=is_checked(GetDlgItem(prefs_window,PREF_AUTO)); draft.trusted_auto=is_checked(GetDlgItem(prefs_window,PREF_TRUSTED_AUTO)); draft.ask=is_checked(GetDlgItem(prefs_window,PREF_ASK));
        w=GetDlgItem(prefs_window,PREF_FOLDER); GetWindowTextW(w,draft.folder,PATH_CAP);
    } else if(pref_section==4) { draft.security=(int)SendDlgItemMessageW(prefs_window,PREF_SECURITY,CB_GETCURSEL,0,0); GetWindowTextW(GetDlgItem(prefs_window,PREF_KEY),draft.key,65); draft.resume=is_checked(GetDlgItem(prefs_window,PREF_RESUME));
    } else if(pref_section==5) {wchar_t value[32];char number[32];
        GetWindowTextW(GetDlgItem(prefs_window,PREF_SEND_LIMIT),value,32);pref_bad_limits=!utf8(value,number,sizeof number) || !bs_limit_parse(number,&draft.send_limit);
        GetWindowTextW(GetDlgItem(prefs_window,PREF_RECEIVE_LIMIT),value,32);pref_bad_limits=pref_bad_limits || !utf8(value,number,sizeof number) || !bs_limit_parse(number,&draft.receive_limit);
    } else if(pref_section==3) { draft.tray=is_checked(GetDlgItem(prefs_window,PREF_TRAY)); draft.start=is_checked(GetDlgItem(prefs_window,PREF_START)); if(!draft.tray) draft.start=0; }
}
static HWND page_control(const wchar_t *cls,const wchar_t *label,DWORD style,int id,int x,int y,int w,int h) {
    HWND control_window=control(prefs_window,cls,label,style,id); place(control_window,x,y,w,h); pref_controls[pref_control_count++]=control_window; return control_window;
}
static HWND page_check(const wchar_t *label,int id,int value,int y) {
    HWND w=page_control(L"BUTTON",label,WS_TABSTOP|BS_OWNERDRAW,id,170,y,370,28); checked(w,value); return w;
}
static void build_page(void) {
    int i; HWND w;
    for(i=0;i<pref_control_count;i++) DestroyWindow(pref_controls[i]);
    pref_control_count=0;
    if(pref_section==0) {
        page_control(L"STATIC",L"Client name",0,0,170,68,370,22);
        w=page_control(L"EDIT",draft.name,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,PREF_NAME,170,94,370,28); SendMessageW(w,EM_SETLIMITTEXT,63,0);
        page_check(L"Visible to other computers",PREF_VISIBLE,draft.visible,140);
        page_control(L"STATIC",L"Choose how this computer appears on your network. Sender names are not verified identities.",0,0,170,184,370,58);
    } else if(pref_section==1) {
        page_control(L"STATIC",L"Color theme",0,0,170,68,370,22);
        w=page_control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS,PREF_THEME,170,94,370,260);
        for(i=0;i<WIN_THEME_COUNT;i++) SendMessageW(w,CB_ADDSTRING,0,(LPARAM)win_themes[i].name);
        SendMessageW(w,CB_SETCURSEL,draft.theme,0);
        page_control(L"BitSkimPreview",L"",0,PREF_PREVIEW,170,138,370,130);
        page_control(L"STATIC",L"Preview changes immediately. Save applies the theme.",0,0,170,280,370,36);
    } else if(pref_section==2) {
        page_check(L"Allow incoming transfers",PREF_RECEIVE,draft.receiving,68);
        page_check(L"Automatically accept file offers",PREF_AUTO,draft.automatic,104);
        page_check(L"Auto-accept only verified devices",PREF_TRUSTED_AUTO,draft.trusted_auto,140);
        page_check(L"Always ask where to save",PREF_ASK,draft.ask,176);
        page_control(L"STATIC",L"Download folder",0,0,170,212,370,22);
        page_control(L"EDIT",draft.folder,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,PREF_FOLDER,170,236,274,28);
        page_control(L"BUTTON",L"Browse...",WS_TABSTOP|BS_OWNERDRAW,PREF_BROWSE,452,236,88,28);
        page_control(L"STATIC",L"Unknown devices ask for approval when verified-only is on. Always ask still requests a save location.",0,0,170,278,370,52);
    } else if(pref_section==3) {
        page_check(L"Keep running in the tray",PREF_TRAY,draft.tray,68);
        w=page_check(L"Start minimized to the tray",PREF_START,draft.start,110);
        EnableWindow(w,draft.tray);
        page_control(L"STATIC",L"Closing the window keeps discovery and reception active in tray mode.\r\n\r\nThis does not add a Windows login item.",0,0,170,162,370,90);
    } else if(pref_section==4) {
        w=page_control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,PREF_SECURITY,170,68,370,180);
        SendMessageW(w,CB_ADDSTRING,0,(LPARAM)L"Disable encryption"); SendMessageW(w,CB_ADDSTRING,0,(LPARAM)L"Prefer encryption"); SendMessageW(w,CB_ADDSTRING,0,(LPARAM)L"Force encryption");SendMessageW(w,CB_SETCURSEL,draft.security,0);
        page_control(L"STATIC",L"Optional pairing key (64 hexadecimal characters)",0,0,170,106,370,22);
        w=page_control(L"EDIT",draft.key,WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,PREF_KEY,170,130,370,28);SendMessageW(w,EM_SETLIMITTEXT,64,0);
        page_control(L"BUTTON",L"Generate random key",WS_TABSTOP|BS_OWNERDRAW,PREF_GENERATE,170,166,200,28);
        page_check(L"Retain interrupted files for resume",PREF_RESUME,draft.resume,202);
        page_control(L"STATIC",L"Prefer encrypts automatically when possible; Force rejects plaintext. Without a matching optional pairing key, peers are not authenticated. Discovery remains public.",0,0,170,240,370,68);
    } else if(pref_section==5) {
        wchar_t number[32];
        page_control(L"STATIC",L"Transfer limits (KiB/s)",0,0,170,68,370,22);
        page_control(L"STATIC",L"Send",0,0,170,106,150,22);swprintf(number,32,L"%lu",draft.send_limit/1024UL);
        w=page_control(L"EDIT",number,WS_TABSTOP|WS_BORDER|ES_NUMBER,PREF_SEND_LIMIT,330,102,180,28);SendMessageW(w,EM_SETLIMITTEXT,7,0);
        page_control(L"STATIC",L"Receive",0,0,170,148,150,22);swprintf(number,32,L"%lu",draft.receive_limit/1024UL);
        w=page_control(L"EDIT",number,WS_TABSTOP|WS_BORDER|ES_NUMBER,PREF_RECEIVE_LIMIT,330,144,180,28);SendMessageW(w,EM_SETLIMITTEXT,7,0);
        page_control(L"STATIC",L"0 = Unlimited. File contents only; limits start with the next file. Receive caps require an updated sender.",0,0,170,198,370,86);
    } else {
        page_control(L"STATIC",L"BitSkim 0.2\r\n\r\nLocal file sharing for modern and retro computers.\r\n\r\nTCP/UDP 46440 - optional TLS",0,0,170,68,370,108);
        page_control(L"BUTTON",L"github.com/retrocs-org/bitskim",WS_TABSTOP|BS_OWNERDRAW,PREF_GITHUB,170,188,370,28);
        page_control(L"BUTTON",L"bitskim.retrocs.org",WS_TABSTOP|BS_OWNERDRAW,PREF_WEBSITE,170,224,370,28);
        page_control(L"STATIC",L"Website planned for a future launch.\r\nMIT license; Catppuccin MIT notices included.",0,0,170,264,370,48);
    }
    repaint(prefs_window);
}
static void choose_folder(void) {
    BROWSEINFOW browse; PIDLIST_ABSOLUTE item; wchar_t path[PATH_CAP];
    ZeroMemory(&browse,sizeof browse); browse.hwndOwner=prefs_window; browse.lpszTitle=L"Choose a download folder"; browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    item=SHBrowseForFolderW(&browse); if(item) { if(SHGetPathFromIDListW(item,path)) text(GetDlgItem(prefs_window,PREF_FOLDER),path); CoTaskMemFree(item); }
}
static LRESULT CALLBACK preview_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_PAINT) {
        PAINTSTRUCT paint; RECT r,part; HDC dc=BeginPaint(w,&paint); const WinTheme *t=&win_themes[draft.theme];
        HFONT old=(HFONT)SelectObject(dc,font); GetClientRect(w,&r); fill(dc,r,t->base);
        part=r; InflateRect(&part,-S(10),-S(10)); fill(dc,part,t->surface);
        part.left+=S(14); part.top+=S(6); part.bottom=part.top+S(32); draw_text(dc,L"BitSkim - Preview",part,t->accent,DT_SINGLELINE|DT_VCENTER);
        part.top+=S(38); part.bottom=part.top+S(26); draw_text(dc,L"Amiga     Mac     Windows PC",part,t->text,DT_SINGLELINE|DT_VCENTER);
        part.top+=S(34); part.bottom=part.top+S(8); fill(dc,part,t->base); part.right=part.left+(part.right-part.left)/2; fill(dc,part,t->accent);
        SelectObject(dc,old); EndPaint(w,&paint); return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
static LRESULT CALLBACK prefs_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_CREATE) {
        int i; prefs_window=w; draft=prefs; pref_section=0;pref_bad_limits=0;
        pref_nav=control(w,L"LISTBOX",L"",WS_TABSTOP|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,PREF_NAV); place(pref_nav,16,68,128,228);
        for(i=0;i<7;i++) SendMessageW(pref_nav,LB_ADDSTRING,0,(LPARAM)sections[i]);
        SendMessageW(pref_nav,LB_SETITEMHEIGHT,0,S(38)); SendMessageW(pref_nav,LB_SETCURSEL,0,0);
        place(button(w,L"Cancel",PREF_CANCEL),260,326,88,30); place(button(w,L"Revert",PREF_REVERT),356,326,88,30); place(button(w,L"Save",PREF_SAVE),452,326,88,30); build_page(); return 0;
    }
    if(msg==WM_COMMAND) {
        int id=LOWORD(wp),event=HIWORD(wp); HWND source=(HWND)lp;
        if(id==PREF_NAV && event==LBN_SELCHANGE) { capture_page(); pref_section=(int)SendMessageW(pref_nav,LB_GETCURSEL,0,0); build_page(); }
        else if(id==PREF_THEME && event==CBN_SELCHANGE) { capture_page(); repaint(w); }
        else if(event==BN_CLICKED && source && GetPropW(source,L"BitSkimChecked")) {
            checked(source,!is_checked(source));
            if(id==PREF_TRAY) {
                HWND start=GetDlgItem(w,PREF_START); int enabled=is_checked(source);
                if(!enabled) checked(start,0);
                EnableWindow(start,enabled);
            }
        }
        else if(id==PREF_GITHUB || id==PREF_WEBSITE) {
            const wchar_t *url=id==PREF_GITHUB?L"https://github.com/retrocs-org/bitskim":L"https://bitskim.retrocs.org";
            if((INT_PTR)ShellExecuteW(w,L"open",url,NULL,NULL,SW_SHOWNORMAL)<=32) error_box(w,L"Could not open your browser. The project address is shown on the button.");
        }
        else if(id==PREF_GENERATE) {char key[65];wchar_t value[65];if(bs_generate_pairing_key(key)){utf16(key,value,65);text(GetDlgItem(w,PREF_KEY),value);SecureZeroMemory(key,sizeof key);}}
        else if(id==PREF_BROWSE) choose_folder();
        else if(id==PREF_CANCEL) DestroyWindow(w);
        else if(id==PREF_REVERT) { draft=prefs;pref_bad_limits=0; build_page(); }
        else if(id==PREF_SAVE) {
            char name[64],folder[PATH_CAP]; capture_page();
            if(pref_bad_limits){error_box(w,L"Network speed limits: enter 0..1048575 KiB/s; 0 is Unlimited.");return 0;}
            if(!draft.name[0] || !utf8(draft.name,name,sizeof name) || !utf8(draft.folder,folder,sizeof folder)) { error_box(w,L"Use a nonempty name of at most 63 UTF-8 bytes and a folder path of at most 1023 UTF-8 bytes."); return 0; }
            {wchar_t directory[PATH_CAP];char path[PATH_CAP];wchar_t *leaf;wcscpy(directory,ini_path);leaf=wcsrchr(directory,L'\\');if(leaf){*leaf=0;wcscat(directory,L"\\Trust");CreateDirectoryW(directory,NULL);if(utf8(directory,path,sizeof path) && !bs_trust_configure(client,path))error_box(NULL,L"Cannot initialize device trust storage.");}}
    { char key[65];
              if(bs_status(client)->active || bs_queue_count(client)) {error_box(w,L"Wait until the transfer queue is empty before saving preferences.");return 0;}
              if(!utf8(draft.key,key,sizeof key) || !bs_security_configure(client,draft.security,key)) {core_error();return 0;}
              SecureZeroMemory(key,sizeof key);bs_resume_enable(client,draft.resume);
            }
            prefs=draft;bs_limits_configure(client,prefs.send_limit,prefs.receive_limit); bs_configure(client,name,prefs.visible,prefs.receiving); update_tray();
            if(!prefs.tray && !IsWindowVisible(main_window)) show_main();
            repaint(main_window); if(!save_preferences()) error_box(w,L"Could not save preferences in your AppData folder."); else DestroyWindow(w);
        }
        return 0;
    }
    if(msg==WM_DRAWITEM) { draw_control((DRAWITEMSTRUCT *)lp,w); return TRUE; }
    if(msg==WM_MEASUREITEM) { ((MEASUREITEMSTRUCT *)lp)->itemHeight=S(28); return TRUE; }
    if(msg==WM_CTLCOLORSTATIC || msg==WM_CTLCOLOREDIT || msg==WM_CTLCOLORLISTBOX) {
        const WinTheme *t=&win_themes[draft.theme]; HDC dc=(HDC)wp; SetTextColor(dc,t->text); SetBkColor(dc,msg==WM_CTLCOLORSTATIC?t->base:t->surface); return (LRESULT)(msg==WM_CTLCOLORSTATIC?base_brush[draft.theme]:surface_brush[draft.theme]);
    }
    if(msg==WM_PAINT) {
        PAINTSTRUCT paint; RECT r={S(170),S(16),S(540),S(50)},area,line; HDC dc=BeginPaint(w,&paint); const WinTheme *t=&win_themes[draft.theme];
        GetClientRect(w,&area); FillRect(dc,&area,base_brush[draft.theme]);
        SelectObject(dc,title_font); draw_text(dc,sections[pref_section],r,t->text,DT_SINGLELINE|DT_VCENTER);
        SelectObject(dc,font); r=(RECT){S(20),S(20),S(144),S(48)}; draw_text(dc,L"Preferences",r,t->muted,DT_SINGLELINE|DT_VCENTER);
        line=(RECT){S(154),S(68),S(155),S(302)}; fill(dc,line,t->surface);
        line=(RECT){S(20),S(314),S(540),S(315)}; fill(dc,line,t->surface);
        EndPaint(w,&paint); return 0;
    }
    if(msg==WM_ERASEBKGND) return 1;
    if(msg==WM_CLOSE) { DestroyWindow(w); return 0; }
    if(msg==WM_DESTROY) { prefs_window=NULL; pref_control_count=0; return 0; }
    return DefWindowProcW(w,msg,wp,lp);
}
static void show_preferences(void) {
    if(!prefs_window) {
        RECT r={0,0,S(560),S(374)}; AdjustWindowRectEx(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,0,0);
        prefs_window=CreateWindowExW(0,L"BitSkimPreferences",L"BitSkim Preferences",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,
            CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,main_window,NULL,instance,NULL);
    }
    if(prefs_window) { ShowWindow(prefs_window,SW_SHOW); SetForegroundWindow(prefs_window); }
}
static void tray_menu(void) {
    POINT point; HMENU menu=CreatePopupMenu(); GetCursorPos(&point);
    AppendMenuW(menu,MF_STRING,ID_HIDE,L"Show BitSkim"); AppendMenuW(menu,MF_STRING,ID_PREFS,L"Preferences...");
    AppendMenuW(menu,MF_STRING|(prefs.receiving?MF_CHECKED:0),ID_RECEIVE,L"Allow incoming transfers"); AppendMenuW(menu,MF_SEPARATOR,0,NULL); AppendMenuW(menu,MF_STRING,ID_QUIT,L"Quit BitSkim");
    SetForegroundWindow(main_window); TrackPopupMenu(menu,TPM_RIGHTBUTTON,point.x,point.y,0,main_window,NULL); PostMessageW(main_window,WM_NULL,0,0); DestroyMenu(menu);
}
static LRESULT CALLBACK main_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    if(taskbar_created && msg==taskbar_created) { tray_added=0; update_tray(); return 0; }
    switch(msg) {
    case WM_CREATE:
        main_window=w;
        create_peer_view(w);
        ip_edit=control(w,L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,ID_IP); SendMessageW(ip_edit,EM_SETLIMITTEXT,70,0); SendMessageW(ip_edit,EM_SETCUEBANNER,FALSE,(LPARAM)L"192.168.1.20 or 192.168.1.20:46440");
        {HMENU bar=CreateMenu();AppendMenuW(bar,MF_STRING,9010,L"Transfer tools");AppendMenuW(bar,MF_STRING,9011,L"Send folder...");AppendMenuW(bar,MF_STRING|(completion_alerts?MF_CHECKED:0),9012,L"Completion alerts");SetMenu(w,bar);}
        button(w,L"Refresh",ID_REFRESH); button(w,L"Send file...",ID_SEND); button(w,L"Preferences",ID_PREFS); button(w,L"Cancel",ID_CANCEL); button(w,L"Hide to tray",ID_HIDE);
        status_text=control(w,L"STATIC",L"Ready on your local network",SS_ENDELLIPSIS,0);
        amount_text=control(w,L"STATIC",L"",SS_ENDELLIPSIS,0); speed_text=control(w,L"STATIC",L"",0,0);
        progress_bar=control(w,PROGRESS_CLASSW,L"",0,0); SetWindowTheme(progress_bar,L"",L""); SendMessageW(progress_bar,PBM_SETRANGE,0,MAKELPARAM(0,100));
        DragAcceptFiles(w,TRUE); SetTimer(w,1,50,NULL); update_tray(); return 0;
    case WM_SIZE: if(peer_list) layout_main(); return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *limits=(MINMAXINFO *)lp;MONITORINFO monitor;
        limits->ptMinTrackSize.x=S(580);limits->ptMinTrackSize.y=S(500);
        ZeroMemory(&monitor,sizeof monitor);monitor.cbSize=sizeof monitor;
        if(GetMonitorInfoW(MonitorFromWindow(w,MONITOR_DEFAULTTONEAREST),&monitor)){
            int width=monitor.rcWork.right-monitor.rcWork.left,height=monitor.rcWork.bottom-monitor.rcWork.top;
            if(width>0 && limits->ptMinTrackSize.x>width)limits->ptMinTrackSize.x=width;
            if(height>0 && limits->ptMinTrackSize.y>height)limits->ptMinTrackSize.y=height;
        }
        return 0;
    }
    case WM_TIMER: bs_service(client,4); refresh(); return 0;
    case WM_ENGINE:
        if(wp==BS_EVENT_COMPLETE){if(bs_completion_notice(client,&completion_serial) && completion_alerts)notify(L"Transfer complete",L"Delivery finished successfully.");}
        else if(wp==BS_EVENT_ERROR) { wchar_t s[512]; utf16(bs_status(client)->message,s,512); notify(L"Transfer stopped",s); }
        return 0;
    case WM_OFFER: incoming(); return 0;
    case WM_COMMAND:
        switch(LOWORD(wp)) {
        
        case ID_IP: if(HIWORD(wp)==EN_CHANGE && GetWindowTextLengthW(ip_edit)>0) ListView_SetItemState(peer_list,-1,0,LVIS_SELECTED); break;
        case ID_SEND: send_file(NULL); break;
        case ID_REFRESH: bs_refresh(client); break;
        case ID_PREFS: show_preferences(); break;
        case 9011:send_folder();break;
        case 9010:bs_tools_open(main_window,instance,client);break;
        case 9012:completion_alerts=!completion_alerts;WritePrivateProfileStringW(L"BitSkim",L"CompletionAlerts",completion_alerts?L"1":L"0",ini_path);CheckMenuItem(GetMenu(w),9012,MF_BYCOMMAND|(completion_alerts?MF_CHECKED:MF_UNCHECKED));break;
        case ID_CANCEL: bs_cancel(client); break;
        case ID_HIDE: if((HWND)lp && prefs.tray && tray_added) ShowWindow(w,SW_HIDE); else show_main(); break;
        case ID_RECEIVE: prefs.receiving=!prefs.receiving; { char n[64]; if(utf8(prefs.name,n,64)) bs_configure(client,n,prefs.visible,prefs.receiving); } save_preferences(); break;
        case ID_QUIT: quit(); break;
        }
        return 0;
    case WM_TRAY: if(lp==WM_LBUTTONDBLCLK || lp==WM_LBUTTONUP) show_main(); else if(lp==WM_RBUTTONUP) tray_menu(); return 0;
    case WM_DROPFILES: {
        HDROP drop=(HDROP)wp; wchar_t path[PATH_CAP];
        UINT i,count=DragQueryFileW(drop,0xffffffff,NULL,0);
        if(count!=1) error_box(w,L"Drop one file or directory.");
        else for(i=0;i<count;i++) {if(DragQueryFileW(drop,i,NULL,0)>=PATH_CAP) {error_box(w,L"A path is too long.");break;}DragQueryFileW(drop,i,path,PATH_CAP);send_file(path);}
        DragFinish(drop); return 0;
    }
    case WM_NOTIFY: {
        NMHDR *header=(NMHDR *)lp;
        if(header->idFrom==ID_PEERS) {
            if(header->code==NM_CUSTOMDRAW) return draw_peer_tile((NMLVCUSTOMDRAW *)lp);
            if(header->code==LVN_GETINFOTIPW) {
                NMLVGETINFOTIPW *tip=(NMLVGETINFOTIPW *)lp;char summary[512];wchar_t detail[512];
                if(tip->iItem>=0 && tip->iItem<displayed_count) {
                    bs_peer_summary(client,displayed_peers[tip->iItem].id,summary,sizeof summary);
                    utf16(summary,detail,512);
                    lstrcpynW(tip->pszText,detail,tip->cchTextMax);
                }

            }
            if(header->code==LVN_ITEMCHANGED) { NMLISTVIEW *change=(NMLISTVIEW *)lp; if((change->uNewState&LVIS_SELECTED) && !(change->uOldState&LVIS_SELECTED)) text(ip_edit,L""); }
            if(header->code==NM_DBLCLK) { if(ListView_GetNextItem(peer_list,-1,LVNI_SELECTED)>=0) send_file(NULL); }
        }
        return 0;
    }
    case WM_DRAWITEM: draw_control((DRAWITEMSTRUCT *)lp,w); return TRUE;
    case WM_MEASUREITEM: ((MEASUREITEMSTRUCT *)lp)->itemHeight=S(50); return TRUE;
    case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: {
        const WinTheme *t=&win_themes[prefs.theme]; HDC dc=(HDC)wp; SetTextColor(dc,t->text); SetBkColor(dc,msg==WM_CTLCOLORSTATIC?t->base:t->surface);
        return (LRESULT)(msg==WM_CTLCOLORSTATIC?base_brush[prefs.theme]:surface_brush[prefs.theme]);
    }
    case WM_PAINT: {
        PAINTSTRUCT paint; RECT r,heading,sub; HDC dc=BeginPaint(w,&paint); const WinTheme *t=&win_themes[prefs.theme]; int h;
        GetClientRect(w,&r); FillRect(dc,&r,base_brush[prefs.theme]); h=MulDiv(r.bottom,96,dpi);
        DrawIconEx(dc,S(20),S(18),app_icon,S(32),S(32),0,NULL,DI_NORMAL);
        heading=(RECT){S(64),S(12),r.right-S(124),S(56)}; SelectObject(dc,title_font); draw_text(dc,L"BitSkim 0.2",heading,t->text,DT_SINGLELINE|DT_VCENTER);
        SelectObject(dc,font); sub=(RECT){S(20),S(62),r.right-S(20),S(88)}; draw_text(dc,L"Nearby computers   /   Drop a file onto a computer",sub,t->muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        sub=(RECT){S(20),S(h-224),r.right-S(20),S(h-202)}; draw_text(dc,L"Or connect by IP address",sub,t->muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        sub=(RECT){S(20),S(h-28),r.right-S(20),S(h-8)};
        draw_text(dc,bs_status(client)->active?(bs_transfer_encrypted(client)?L"Local network / TLS encrypted":prefs.security==2?L"Local network / TLS connecting":L"Local network / Unencrypted"):(prefs.security==2?L"Local network / Encryption required":prefs.security==1?L"Local network / Prefer encryption":L"Local network / Encryption disabled"),sub,t->muted,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        EndPaint(w,&paint); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: if(prefs.tray && tray_added) ShowWindow(w,SW_HIDE); else quit(); return 0;
    case WM_DESTROY:
        KillTimer(w,1); RevokeDragDrop(peer_list); if(peer_images) { ListView_SetImageList(peer_list,NULL,LVSIL_NORMAL); ImageList_Destroy(peer_images); peer_images=NULL; } if(tray_added) Shell_NotifyIconW(NIM_DELETE,&tray_data); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE previous,PWSTR command,int show) {
    WNDCLASSEXW cls; INITCOMMONCONTROLSEX common={sizeof common,ICC_PROGRESS_CLASS|ICC_LISTVIEW_CLASSES}; MSG msg; BSConfig config; HDC dc; int i,argc=0,startup_check=0; LPWSTR *argv; HANDLE mutex;
    (void)previous; (void)command; (void)show; instance=h;
    argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if(argv) { startup_check=argc==2 && !wcscmp(argv[1],L"--check-startup"); LocalFree(argv); }
    /* Exercises the real GUI image loader/imports without opening windows or saving settings. */
    if(startup_check) {
        int ok=InitCommonControlsEx(&common) && bs_platform_init();
        if(ok) { bs_platform_cleanup(); ok=check_peer_view(); }
        return ok?0:1;
    }
    mutex=CreateMutexW(NULL,FALSE,L"Local\\BitSkim0.2"); if(mutex && GetLastError()==ERROR_ALREADY_EXISTS) { error_box(NULL,L"BitSkim is already running. Check the notification area."); CloseHandle(mutex); return 1; }
    OleInitialize(NULL); InitCommonControlsEx(&common);
    if(!bs_platform_init()) { error_box(NULL,L"Windows networking could not be initialized."); return 1; }
    load_preferences();
    ZeroMemory(&config,sizeof config); if(!utf8(prefs.name,config.name,sizeof config.name)) strcpy(config.name,"Windows PC");
    strcpy(config.id,prefs.id); strcpy(config.platform,"Windows"); config.port=BS_PORT; config.discovery_port=BS_PORT; config.visible=prefs.visible; config.receiving=prefs.receiving;
    client=bs_create(&config,engine_event,NULL); if(!client) { error_box(NULL,L"Cannot listen on port 46440. Close another BitSkim instance or check your firewall/network settings."); bs_platform_cleanup(); return 1; }
    {char store[BS_PATH_MAX+1],*slash;
        if(utf8(ini_path,store,sizeof store)) {slash=strrchr(store,92);if(slash)strcpy(slash+1,"BSFOLD.DAT");bs_folders_configure(client,store);}
    }
    { char key[65];
      if(!utf8(prefs.key,key,sizeof key) || !bs_security_configure(client,prefs.security,key)) {core_error();bs_destroy(client);bs_platform_cleanup();return 1;}
      SecureZeroMemory(key,sizeof key);bs_limits_configure(client,prefs.send_limit,prefs.receive_limit);bs_resume_enable(client,prefs.resume);bs_batch_enable(client,1);
    }
    dc=GetDC(NULL); dpi=GetDeviceCaps(dc,LOGPIXELSY); ReleaseDC(NULL,dc);
    font=CreateFontW(-S(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    title_font=CreateFontW(-S(22),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(i=0;i<WIN_THEME_COUNT;i++) { base_brush[i]=CreateSolidBrush(win_themes[i].base); surface_brush[i]=CreateSolidBrush(win_themes[i].surface); }
    app_icon=(HICON)LoadImageW(h,MAKEINTRESOURCEW(ID_ICON),IMAGE_ICON,0,0,LR_DEFAULTSIZE|LR_SHARED); taskbar_created=RegisterWindowMessageW(L"TaskbarCreated");
    ZeroMemory(&cls,sizeof cls); cls.cbSize=sizeof cls; cls.hInstance=h; cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hIcon=cls.hIconSm=app_icon;
    cls.lpfnWndProc=main_proc; cls.lpszClassName=L"BitSkimMain"; RegisterClassExW(&cls);
    cls.lpfnWndProc=prefs_proc; cls.lpszClassName=L"BitSkimPreferences"; RegisterClassExW(&cls);
    cls.lpfnWndProc=preview_proc; cls.lpszClassName=L"BitSkimPreview"; RegisterClassExW(&cls);
    main_window=CreateWindowExW(0,L"BitSkimMain",L"BitSkim",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,S(720),S(660),NULL,NULL,h,NULL);
    if(!main_window) { bs_destroy(client); bs_platform_cleanup(); return 1; }
    { MONITORINFO monitor;ZeroMemory(&monitor,sizeof monitor);monitor.cbSize=sizeof monitor;
      if(GetMonitorInfoW(MonitorFromWindow(main_window,MONITOR_DEFAULTTONEAREST),&monitor)){
          RECT fit=bs_window_fit(monitor.rcWork,S(720),S(660));
          SetWindowPos(main_window,NULL,fit.left,fit.top,fit.right-fit.left,fit.bottom-fit.top,SWP_NOZORDER|SWP_NOACTIVATE);
      }
    }
    layout_main(); repaint(main_window); save_preferences();
    if(!prefs.start || !prefs.tray || !tray_added) show_main();
    while(GetMessageW(&msg,NULL,0,0)>0) {
        if(bs_tools_window && IsDialogMessageW(bs_tools_window,&msg))continue;
        if(prefs_window && IsDialogMessageW(prefs_window,&msg)) continue;
        if(IsDialogMessageW(main_window,&msg)) continue;
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    bs_destroy(client); bs_platform_cleanup();
    for(i=0;i<WIN_THEME_COUNT;i++) { DeleteObject(base_brush[i]); DeleteObject(surface_brush[i]); }
    DeleteObject(font); DeleteObject(title_font); OleUninitialize(); if(mutex) CloseHandle(mutex); return 0;
}
