/* Modeless local tools; timers continue servicing the application's engine. */
#include <tchar.h>
#include "../windows/window-fit.h"
static int bs_tools_dpi=96;
static int bs_tools_scale(int n){return MulDiv(n,bs_tools_dpi,96);}
static HWND bs_tools_window,bs_tools_report,bs_tools_code,bs_tools_entry,bs_tools_policy,bs_tools_address;
static BSClient *bs_tools_client;
static int bs_tools_index=-1;static unsigned long bs_tools_serial;
static void bs_tools_convert(const char *source,TCHAR *target,int count){
#ifdef UNICODE
    if(!MultiByteToWideChar(CP_UTF8,0,source,-1,target,count))target[0]=0;
#else
    wchar_t wide[4096];if(!MultiByteToWideChar(CP_UTF8,0,source,-1,wide,4096) || !WideCharToMultiByte(CP_ACP,0,wide,-1,target,count,NULL,NULL))target[0]=0;
#endif
}
static int bs_tools_find(void){int i;BSHistoryItem h;for(i=0;i<bs_history_count(bs_tools_client);i++)if(bs_history_item(bs_tools_client,i,&h) && h.serial==bs_tools_serial)return i;return -1;}
static void bs_tools_pick(int index){BSHistoryItem h;if(bs_history_item(bs_tools_client,index,&h))bs_tools_serial=h.serial;}
/* Replacing edit text resets its viewport; restore it after live diagnostics. */
static void bs_tools_update_report(const TCHAR *text){
    TCHAR old[4096];DWORD start=0,end=0;int first,now;
    GetWindowText(bs_tools_report,old,4096);if(!_tcscmp(old,text))return;
    first=(int)SendMessage(bs_tools_report,EM_GETFIRSTVISIBLELINE,0,0);
    SendMessage(bs_tools_report,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);
    SendMessage(bs_tools_report,WM_SETREDRAW,FALSE,0);
    SetWindowText(bs_tools_report,text);
    SendMessage(bs_tools_report,EM_SETSEL,start,end);
    now=(int)SendMessage(bs_tools_report,EM_GETFIRSTVISIBLELINE,0,0);
    SendMessage(bs_tools_report,EM_LINESCROLL,0,first-now);
    SendMessage(bs_tools_report,WM_SETREDRAW,TRUE,0);
    InvalidateRect(bs_tools_report,NULL,FALSE);
}
static const WinTheme *bs_tools_palette;
static HBRUSH bs_tools_base,bs_tools_surface,bs_tools_accent;
static void bs_tools_free_theme(void){
    if(bs_tools_base)DeleteObject(bs_tools_base);
    if(bs_tools_surface)DeleteObject(bs_tools_surface);
    if(bs_tools_accent)DeleteObject(bs_tools_accent);
    bs_tools_base=bs_tools_surface=bs_tools_accent=NULL;bs_tools_palette=NULL;
}
static void bs_tools_use_palette(const WinTheme *palette){
    if(palette==bs_tools_palette)return;
    bs_tools_free_theme();bs_tools_palette=palette;
    bs_tools_base=CreateSolidBrush(palette->base);bs_tools_surface=CreateSolidBrush(palette->surface);bs_tools_accent=CreateSolidBrush(palette->accent);
}
static BOOL CALLBACK bs_tools_repaint_child(HWND child,LPARAM ignored){(void)ignored;InvalidateRect(child,NULL,TRUE);return TRUE;}
static void bs_tools_apply_theme(void){
#ifdef BS_WINDOWS_LEGACY
    const WinTheme *palette=theme();
#else
    const WinTheme *palette=&win_themes[prefs.theme];
#endif
    if(palette==bs_tools_palette)return;
    bs_tools_use_palette(palette);
    if(bs_tools_window){
#ifndef BS_WINDOWS_LEGACY
        BOOL dark=palette->dark;DwmSetWindowAttribute(bs_tools_window,20,&dark,sizeof dark);
#endif
        InvalidateRect(bs_tools_window,NULL,TRUE);EnumChildWindows(bs_tools_window,bs_tools_repaint_child,0);
    }
}
static LRESULT bs_tools_paint(HWND w,UINT msg,WPARAM wp,LPARAM lp,int *handled){
    const WinTheme *palette=bs_tools_palette;*handled=1;
    if(!palette){*handled=0;return 0;}
    if(msg==WM_ERASEBKGND){RECT rect;GetClientRect(w,&rect);FillRect((HDC)wp,&rect,bs_tools_base);return 1;}
    if(msg==WM_CTLCOLORSTATIC || msg==WM_CTLCOLOREDIT || msg==WM_CTLCOLORLISTBOX || msg==WM_CTLCOLORBTN){
        HWND child=(HWND)lp;int surface=msg!=WM_CTLCOLORSTATIC || child==bs_tools_report || child==bs_tools_policy;
        SetTextColor((HDC)wp,palette->text);SetBkColor((HDC)wp,surface?palette->surface:palette->base);
        return (LRESULT)(surface?bs_tools_surface:bs_tools_base);
    }
    if(msg==WM_DRAWITEM){
        DRAWITEMSTRUCT *item=(DRAWITEMSTRUCT *)lp;RECT rect=item->rcItem;TCHAR label[256];int selected=(item->itemState&ODS_SELECTED)!=0;
        HGDIOBJ old_font=SelectObject(item->hDC,GetStockObject(DEFAULT_GUI_FONT));
        FillRect(item->hDC,&rect,selected?bs_tools_accent:bs_tools_surface);
        SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,(item->itemState&ODS_DISABLED)?palette->muted:selected?palette->base:palette->text);
        label[0]=0;
        if(item->CtlType==ODT_COMBOBOX){int index=(int)item->itemID;if(index<0)index=(int)SendMessage(bs_tools_policy,CB_GETCURSEL,0,0);if(index>=0)SendMessage(bs_tools_policy,CB_GETLBTEXT,index,(LPARAM)label);rect.left+=bs_tools_scale(8);}
        else GetWindowText(item->hwndItem,label,256);
        DrawText(item->hDC,label,-1,&rect,DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|(item->CtlType==ODT_COMBOBOX?DT_LEFT:DT_CENTER));
        if(item->itemState&ODS_FOCUS){InflateRect(&rect,-3,-3);DrawFocusRect(item->hDC,&rect);}
        SelectObject(item->hDC,old_font);return TRUE;
    }
    if(msg==WM_MEASUREITEM){((MEASUREITEMSTRUCT *)lp)->itemHeight=bs_tools_scale(22);return TRUE;}
    *handled=0;return 0;
}
static void bs_tools_refresh(void){char report[4096],history[2048],code[20],normalized[4096];TCHAR native[4096];size_t used,i,j=0;int peer_index;char compatibility[512];const BSPeer *peer;
    bs_tools_apply_theme();bs_diagnostics(bs_tools_client,report,sizeof report);used=strlen(report);bs_tools_index=bs_tools_find();
    if(bs_tools_index>=0 && bs_history_text(bs_tools_client,bs_tools_index,history,sizeof history)){strncat(report,"\n\nRECENT TRANSFER\n",sizeof report-strlen(report)-1);strncat(report,history,sizeof report-strlen(report)-1);}else if(!used)strcpy(report,"No transfer history.");
    if(bs_peer_count(bs_tools_client))strncat(report,"\n\nNEARBY COMPATIBILITY (DISCOVERY HINTS)\n",sizeof report-strlen(report)-1);
    for(peer_index=0;peer_index<bs_peer_count(bs_tools_client);peer_index++){
        if(sizeof report-strlen(report)<650){strncat(report,"Additional computers omitted from this report.\n",sizeof report-strlen(report)-1);break;}
        peer=bs_peer(bs_tools_client,peer_index);if(!peer)continue;
        bs_peer_summary(bs_tools_client,peer->id,compatibility,sizeof compatibility);
        strncat(report,peer->name,sizeof report-strlen(report)-1);strncat(report,": ",sizeof report-strlen(report)-1);
        strncat(report,compatibility,sizeof report-strlen(report)-1);strncat(report,"\n",sizeof report-strlen(report)-1);
    }
    for(i=0;report[i] && j<sizeof normalized-2;i++){if(report[i]=='\n' && (!i || report[i-1]!='\r'))normalized[j++]='\r';normalized[j++]=report[i];}normalized[j]=0;
    bs_tools_convert(normalized,native,4096);bs_tools_update_report(native);
    if(bs_pairing_code(bs_tools_client,code)){
        char label[160];sprintf(label,"Compare both screens: %s   %s",code,bs_trust_status(bs_tools_client)==1?"(trusted identity matches)":"(not yet trusted)");bs_tools_convert(label,native,4096);
    }else lstrcpy(native,TEXT("Pair during an unpaired TLS connection. Shared-key mode uses its saved key."));
    {TCHAR old[256];GetWindowText(bs_tools_code,old,256);if(_tcscmp(old,native))SetWindowText(bs_tools_code,native);}

}
static HWND bs_tools_control(HWND parent,LPCTSTR kind,LPCTSTR text,DWORD style,int x,int y,int width,int height,int id){HWND h;if(!lstrcmpi(kind,TEXT("BUTTON")))style=(style&~15UL)|BS_OWNERDRAW;h=CreateWindow(kind,text,WS_CHILD|WS_VISIBLE|style,bs_tools_scale(x),bs_tools_scale(y),bs_tools_scale(width),bs_tools_scale(height),parent,(HMENU)(INT_PTR)id,(HINSTANCE)GetWindowLongPtr(parent,GWLP_HINSTANCE),NULL);SendMessage(h,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return h;}
static void bs_tools_layout(HWND w){
    RECT rect;int width,height,report_height,row;GetClientRect(w,&rect);
    width=MulDiv(rect.right,96,bs_tools_dpi);height=MulDiv(rect.bottom,96,bs_tools_dpi);
    report_height=height-164-88;if(report_height<40)report_height=40;
    MoveWindow(bs_tools_report,bs_tools_scale(16),bs_tools_scale(164),bs_tools_scale(width-32),bs_tools_scale(report_height),TRUE);
    row=height-78;
    MoveWindow(GetDlgItem(w,6),bs_tools_scale(16),bs_tools_scale(row),bs_tools_scale(75),bs_tools_scale(28),TRUE);
    MoveWindow(GetDlgItem(w,7),bs_tools_scale(99),bs_tools_scale(row),bs_tools_scale(75),bs_tools_scale(28),TRUE);
    MoveWindow(GetDlgItem(w,8),bs_tools_scale(182),bs_tools_scale(row),bs_tools_scale(120),bs_tools_scale(28),TRUE);
    MoveWindow(GetDlgItem(w,9),bs_tools_scale(310),bs_tools_scale(row),bs_tools_scale(100),bs_tools_scale(28),TRUE);
    MoveWindow(GetDlgItem(w,10),bs_tools_scale(width-116),bs_tools_scale(row),bs_tools_scale(100),bs_tools_scale(28),TRUE);
    MoveWindow(GetDlgItem(w,11),bs_tools_scale(16),bs_tools_scale(height-42),bs_tools_scale(150),bs_tools_scale(26),TRUE);
    MoveWindow(bs_tools_address,bs_tools_scale(176),bs_tools_scale(height-42),bs_tools_scale(width-192),bs_tools_scale(26),TRUE);
}
static LRESULT CALLBACK bs_tools_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    int handled;LRESULT painted=bs_tools_paint(w,msg,wp,lp,&handled);if(handled)return painted;
    if(msg==WM_CREATE){
        bs_tools_control(w,TEXT("STATIC"),TEXT("Existing files (this session; applies to remaining collection files):"),0,16,12,560,20,0);
        bs_tools_policy=bs_tools_control(w,TEXT("COMBOBOX"),TEXT(""),CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,16,36,560,110,1);
        SendMessage(bs_tools_policy,CB_ADDSTRING,0,(LPARAM)TEXT("Rename safely (default)"));SendMessage(bs_tools_policy,CB_ADDSTRING,0,(LPARAM)TEXT("Skip existing files"));SendMessage(bs_tools_policy,CB_ADDSTRING,0,(LPARAM)TEXT("Replace existing files"));SendMessage(bs_tools_policy,CB_SETCURSEL,bs_conflict_get(bs_tools_client),0);
        bs_tools_code=bs_tools_control(w,TEXT("STATIC"),TEXT(""),0,16,76,560,36,0);
        bs_tools_entry=bs_tools_control(w,TEXT("EDIT"),TEXT(""),WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,16,120,220,26,2);SendMessage(bs_tools_entry,EM_SETLIMITTEXT,14,0);
        bs_tools_control(w,TEXT("BUTTON"),TEXT("Trust code from other screen"),WS_TABSTOP,246,120,220,28,3);
        bs_tools_control(w,TEXT("BUTTON"),TEXT("Forget"),WS_TABSTOP,476,120,100,28,4);
        bs_tools_report=bs_tools_control(w,TEXT("EDIT"),TEXT(""),WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,16,164,560,380,5);
        bs_tools_control(w,TEXT("BUTTON"),TEXT("Older"),WS_TABSTOP,16,556,75,28,6);bs_tools_control(w,TEXT("BUTTON"),TEXT("Newer"),WS_TABSTOP,99,556,75,28,7);
        bs_tools_control(w,TEXT("BUTTON"),TEXT("Retry outgoing"),WS_TABSTOP,182,556,120,28,8);bs_tools_control(w,TEXT("BUTTON"),TEXT("Open folder"),WS_TABSTOP,310,556,100,28,9);bs_tools_control(w,TEXT("BUTTON"),TEXT("Close"),WS_TABSTOP,476,556,100,28,10);
        bs_tools_control(w,TEXT("BUTTON"),TEXT("Test connection"),WS_TABSTOP,16,590,150,26,11);
        bs_tools_address=bs_tools_control(w,TEXT("EDIT"),TEXT(""),WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,176,590,400,26,12);
        bs_tools_layout(w);bs_tools_pick(0);SetTimer(w,1,500,NULL);bs_tools_refresh();return 0;
    }
    if(msg==WM_SIZE){if(bs_tools_report)bs_tools_layout(w);return 0;}
    if(msg==WM_TIMER){if(!bs_tools_serial)bs_tools_pick(0);bs_tools_refresh();return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp);char error[256];TCHAR native[512];
        if(id==1 && HIWORD(wp)==CBN_SELCHANGE)bs_conflict_policy(bs_tools_client,(int)SendMessage(bs_tools_policy,CB_GETCURSEL,0,0));
        else if(id==3){char code[20];TCHAR entered[20];int i;GetWindowText(bs_tools_entry,entered,20);for(i=0;entered[i] && i<19;i++)code[i]=(char)toupper((unsigned char)entered[i]);code[i]=0;if(!bs_trust_current(bs_tools_client,code)){bs_tools_convert(bs_last_error(bs_tools_client),native,512);MessageBox(w,native,TEXT("Device pairing"),MB_OK|MB_ICONEXCLAMATION);}else SetWindowText(bs_tools_entry,TEXT(""));}
        else if(id==4){if(MessageBox(w,TEXT("Forget this device identity? Verify codes again before trusting it."),TEXT("BitSkim"),MB_YESNO)==IDYES)bs_forget_current(bs_tools_client);}
        else if(id==6 || id==7){int index=bs_tools_find();if(index>=0)bs_tools_pick(index+(id==6?1:-1));}
        else if(id==8){int index=bs_tools_find();if(index>=0 && !bs_retry_history(bs_tools_client,index)){strncpy(error,bs_last_error(bs_tools_client),255);error[255]=0;bs_tools_convert(error,native,512);MessageBox(w,native,TEXT("Retry"),MB_OK);}}
        else if(id==9){BSHistoryItem h;int index=bs_tools_find();if(index>=0 && bs_history_item(bs_tools_client,index,&h)){TCHAR path[BS_PATH_MAX+1],*slash,*other;bs_tools_convert(h.path,path,BS_PATH_MAX+1);slash=_tcsrchr(path,TEXT('/'));other=_tcsrchr(path,TEXT('\\'));if(other && (!slash || other>slash))slash=other;if(slash){slash[1]=0;ShellExecute(w,TEXT("explore"),path,NULL,NULL,SW_SHOWNORMAL);}}}
        else if(id==11){TCHAR entered[64];char address[64];int i;GetWindowText(bs_tools_address,entered,64);for(i=0;i<63 && entered[i];i++)address[i]=(char)entered[i];address[i]=0;if(!bs_network_test(bs_tools_client,address,0)){bs_tools_convert(bs_last_error(bs_tools_client),native,512);MessageBox(w,native,TEXT("Connection check"),MB_OK);}bs_tools_serial=0;}
        else if(id==10)DestroyWindow(w);
        if(bs_tools_window)bs_tools_refresh();
        return 0;
    }
    if(msg==WM_CLOSE){DestroyWindow(w);return 0;}
    if(msg==WM_DESTROY){KillTimer(w,1);bs_tools_window=NULL;bs_tools_free_theme();return 0;}
    return DefWindowProc(w,msg,wp,lp);
}
static void bs_tools_open(HWND owner,HINSTANCE instance,BSClient *client){
    WNDCLASS cls;RECT work,fit,client_rect;HDC dc;int width,height;
    if(bs_tools_window){SetForegroundWindow(bs_tools_window);return;}
    bs_tools_client=client;bs_tools_serial=0;bs_tools_apply_theme();
    dc=GetDC(owner);bs_tools_dpi=GetDeviceCaps(dc,LOGPIXELSY);ReleaseDC(owner,dc);if(bs_tools_dpi<96)bs_tools_dpi=96;
    work.left=work.top=0;work.right=GetSystemMetrics(SM_CXSCREEN);work.bottom=GetSystemMetrics(SM_CYSCREEN);
    SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
#ifndef BS_WINDOWS_LEGACY
    {MONITORINFO monitor;ZeroMemory(&monitor,sizeof monitor);monitor.cbSize=sizeof monitor;
     if(GetMonitorInfo(MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST),&monitor))work=monitor.rcWork;}
#endif
    memset(&cls,0,sizeof cls);cls.lpfnWndProc=bs_tools_proc;cls.hInstance=instance;cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=NULL;cls.lpszClassName=TEXT("BitSkimTransferTools");RegisterClass(&cls);
    /* Request a client size; include the caption/frame before fitting to the work area. */
    client_rect.left=client_rect.top=0;client_rect.right=bs_tools_scale(592);client_rect.bottom=bs_tools_scale(640);
    AdjustWindowRect(&client_rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE);
    width=client_rect.right-client_rect.left;height=client_rect.bottom-client_rect.top;
    fit=bs_window_fit(work,width,height);
    bs_tools_window=CreateWindow(cls.lpszClassName,TEXT("BitSkim - Transfer tools"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,fit.left,fit.top,fit.right-fit.left,fit.bottom-fit.top,owner,NULL,instance,NULL);
#ifndef BS_WINDOWS_LEGACY
    {BOOL dark=bs_tools_palette->dark;DwmSetWindowAttribute(bs_tools_window,20,&dark,sizeof dark);}
#endif
    ShowWindow(bs_tools_window,SW_SHOW);
}

static int bs_tools_pair_offer(HWND owner,BSClient *client){char code[20],label[600];TCHAR native[600];int answer;
    if(bs_trust_status(client)==1 || !bs_pairing_code(client,code))return 1;
    sprintf(label,"Compare this entire code with the sending computer:\n\n%s\n\nOnly choose Yes if both codes match.\nYes: remember this device identity\nNo: accept this transfer without remembering\nCancel: decline the transfer",code);bs_tools_convert(label,native,600);
    answer=MessageBox(owner,native,TEXT("Verify sending device"),MB_YESNOCANCEL|MB_ICONQUESTION);
    if(answer==IDCANCEL)return 0;
    if(answer==IDYES && !bs_trust_current(client,code)){bs_tools_convert(bs_last_error(client),native,600);MessageBox(owner,native,TEXT("Pairing failed"),MB_OK);return 0;}
    return 1;
}

static int bs_tools_space(HWND owner,BSClient *client,const char *path,int collection,int ask){char report[256]="Destination free space could not be determined.";TCHAR native[512];BSStorage info;int enough;if(!collection && bs_conflict_get(client)==BS_CONFLICT_SKIP && bsp_path_kind(path)==1)return 1;enough=bs_destination_space_report(client,path,collection,report,sizeof report);bs_tools_convert(report,native,512);
    if(enough<0){MessageBox(owner,native,TEXT("Destination disk space"),MB_OK|MB_ICONEXCLAMATION);return 0;}
    if(bs_destination_space_info(client,path,collection,&info) && info.known && info.enough)return 1;
    return !ask || MessageBox(owner,native,TEXT("Destination disk space - continue?"),MB_YESNO|MB_ICONQUESTION)==IDYES;
}
