/* V1 (Windows 95) notification structure and callbacks only. */
#define TRAY_MESSAGE (WM_APP+10)
#define TRAY_SHOW 200
#define TRAY_PREFS 201
#define TRAY_REFRESH 202
#define TRAY_CANCEL 203
#define TRAY_QUIT 204
static int tray_present,start_tray,close_tray=1;
static UINT taskbar_created;
static NOTIFYICONDATAA tray_data;
static int add_tray(void) {
    memset(&tray_data,0,sizeof tray_data); tray_data.cbSize=NOTIFYICONDATAA_V1_SIZE;
    tray_data.hWnd=window; tray_data.uID=1; tray_data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;
    tray_data.uCallbackMessage=TRAY_MESSAGE; tray_data.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(1));
    lstrcpynA(tray_data.szTip,"BitSkim 0.1 - local file sharing",64);
    tray_present=Shell_NotifyIconA(NIM_ADD,&tray_data)!=0; return tray_present;
}
static void show_main(void) { ShowWindow(window,SW_RESTORE); SetForegroundWindow(prefs?prefs:window); }
static void hide_main(void) {
    if(!tray_present && !add_tray()) { show_main(); MessageBoxA(window,"The system tray is unavailable. BitSkim will keep its window open.","BitSkim",MB_OK); return; }
    ShowWindow(window,SW_HIDE);
}
static void tray_menu(void) {
    HMENU menu=CreatePopupMenu(); POINT p; UINT choice;
    AppendMenuA(menu,MF_STRING,TRAY_SHOW,"Show BitSkim"); AppendMenuA(menu,MF_STRING,TRAY_PREFS,"Preferences...");
    AppendMenuA(menu,MF_STRING,TRAY_REFRESH,"Refresh computers"); AppendMenuA(menu,MF_STRING,TRAY_CANCEL,"Cancel transfer");
    AppendMenuA(menu,MF_SEPARATOR,0,NULL); AppendMenuA(menu,MF_STRING,TRAY_QUIT,"Quit BitSkim");
    GetCursorPos(&p); SetForegroundWindow(window);
    choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,window,NULL); DestroyMenu(menu);
    PostMessageA(window,WM_NULL,0,0); if(choice) PostMessageA(window,WM_COMMAND,choice,0);
}
