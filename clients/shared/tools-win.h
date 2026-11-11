/* Modeless local tools; timers continue servicing the application's engine. */
#include <tchar.h>
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
static void bs_tools_refresh(void){char report[4096],history[2048],code[20],normalized[4096];TCHAR native[4096],old[4096];size_t used,i,j=0;
    bs_diagnostics(bs_tools_client,report,sizeof report);used=strlen(report);bs_tools_index=bs_tools_find();
    if(bs_tools_index>=0 && bs_history_text(bs_tools_client,bs_tools_index,history,sizeof history)){strcat(report,"\n\nRECENT TRANSFER\n");strcat(report,history);}else if(!used)strcpy(report,"No transfer history.");
    for(i=0;report[i] && j<sizeof normalized-2;i++){if(report[i]=='\n' && (!i || report[i-1]!='\r'))normalized[j++]='\r';normalized[j++]=report[i];}normalized[j]=0;
    bs_tools_convert(normalized,native,4096);GetWindowText(bs_tools_report,old,4096);if(_tcscmp(native,old))SetWindowText(bs_tools_report,native);
    if(bs_pairing_code(bs_tools_client,code)){char label[160];sprintf(label,"Compare both screens: %s   %s",code,bs_trust_status(bs_tools_client)==1?"(trusted identity matches)":"(not yet trusted)");bs_tools_convert(label,native,4096);SetWindowText(bs_tools_code,native);}else SetWindowText(bs_tools_code,TEXT("Pair during an unpaired TLS connection. Shared-key mode uses its saved key."));
}
static HWND bs_tools_control(HWND parent,LPCTSTR kind,LPCTSTR text,DWORD style,int x,int y,int width,int height,int id){HWND h=CreateWindow(kind,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,(HMENU)(INT_PTR)id,(HINSTANCE)GetWindowLongPtr(parent,GWLP_HINSTANCE),NULL);SendMessage(h,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return h;}
static LRESULT CALLBACK bs_tools_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_CREATE){
        bs_tools_control(w,TEXT("STATIC"),TEXT("Existing files (this session; applies to remaining collection files):"),0,16,12,560,20,0);
        bs_tools_policy=bs_tools_control(w,TEXT("COMBOBOX"),TEXT(""),CBS_DROPDOWNLIST|WS_TABSTOP,16,36,560,110,1);
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
        bs_tools_pick(0);SetTimer(w,1,500,NULL);bs_tools_refresh();return 0;
    }
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
    if(msg==WM_DESTROY){KillTimer(w,1);bs_tools_window=NULL;return 0;}
    return DefWindowProc(w,msg,wp,lp);
}
static void bs_tools_open(HWND owner,HINSTANCE instance,BSClient *client){WNDCLASS cls;if(bs_tools_window){SetForegroundWindow(bs_tools_window);return;}bs_tools_client=client;bs_tools_serial=0;memset(&cls,0,sizeof cls);cls.lpfnWndProc=bs_tools_proc;cls.hInstance=instance;cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.lpszClassName=TEXT("BitSkimTransferTools");RegisterClass(&cls);bs_tools_window=CreateWindow(cls.lpszClassName,TEXT("BitSkim - Transfer tools"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,610,680,owner,NULL,instance,NULL);ShowWindow(bs_tools_window,SW_SHOW);}

static int bs_tools_pair_offer(HWND owner,BSClient *client){char code[20],label[600];TCHAR native[600];int answer;
    if(bs_trust_status(client)==1 || !bs_pairing_code(client,code))return 1;
    sprintf(label,"Compare this entire code with the sending computer:\n\n%s\n\nOnly choose Yes if both codes match.\nYes: remember this device identity\nNo: accept this transfer without remembering\nCancel: decline the transfer",code);bs_tools_convert(label,native,600);
    answer=MessageBox(owner,native,TEXT("Verify sending device"),MB_YESNOCANCEL|MB_ICONQUESTION);
    if(answer==IDCANCEL)return 0;
    if(answer==IDYES && !bs_trust_current(client,code)){bs_tools_convert(bs_last_error(client),native,600);MessageBox(owner,native,TEXT("Pairing failed"),MB_OK);return 0;}
    return 1;
}

static int bs_tools_space(HWND owner,BSClient *client,const char *path,int collection,int ask){char report[256];TCHAR native[512];int enough;if(!collection && bs_conflict_get(client)==BS_CONFLICT_SKIP && bsp_path_kind(path)==1)return 1;enough=bs_destination_space_report(client,path,collection,report,sizeof report);bs_tools_convert(report,native,512);
    if(enough<0){MessageBox(owner,native,TEXT("Destination disk space"),MB_OK|MB_ICONEXCLAMATION);return 0;}
    return !ask || MessageBox(owner,native,TEXT("Destination disk space - continue?"),MB_YESNO|MB_ICONQUESTION)==IDYES;
}
