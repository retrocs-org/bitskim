/* Modeless invitation window: engine timers and receive approval remain live. */
#include <iphlpapi.h>
#include "../../third_party/qrcodegen/qrcodegen.h"
static HWND invitation_window,invitation_address_edit,invitation_note;
static unsigned char invitation_qr[qrcodegen_BUFFER_LEN_MAX],invitation_scratch[qrcodegen_BUFFER_LEN_MAX];
static time_t invitation_until;
static void invitation_generate(void){wchar_t address[48];char ip[48],uri[256];
    GetWindowTextW(invitation_address_edit,address,48);
    if(!utf8(address,ip,sizeof ip) || !bs_invitation_create(client,ip,uri,sizeof uri)){error_box(invitation_window,L"Enable receiving and certificate encryption, and select a reachable LAN address.");return;}
    if(!qrcodegen_encodeText(uri,invitation_scratch,invitation_qr,qrcodegen_Ecc_MEDIUM,1,20,qrcodegen_Mask_AUTO,TRUE)){error_box(invitation_window,L"Cannot create QR invitation.");return;}
    invitation_until=time(NULL)+300;InvalidateRect(invitation_window,NULL,TRUE);
}
static LRESULT CALLBACK invitation_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
    switch(message){
    case WM_CREATE:{IP_ADAPTER_ADDRESSES *adapters,*a;ULONG length=16000;int found=0;BOOL dark=win_themes[prefs.theme].dark;
        DwmSetWindowAttribute(w,20,&dark,sizeof dark);
        invitation_address_edit=control(w,L"COMBOBOX",L"",CBS_DROPDOWN|WS_VSCROLL|WS_TABSTOP,1);place(invitation_address_edit,24,20,280,180);
        {HWND b=button(w,L"Generate QR",2);place(b,316,20,140,32);}
        invitation_note=control(w,L"STATIC",L"Select this computer's LAN address.",0,3);place(invitation_note,24,64,440,48);
        adapters=(IP_ADAPTER_ADDRESSES *)malloc(length);
        if(adapters && GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,adapters,&length)==ERROR_BUFFER_OVERFLOW){free(adapters);adapters=(IP_ADAPTER_ADDRESSES *)malloc(length);}
        if(adapters && GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,adapters,&length)==NO_ERROR){for(a=adapters;a;a=a->Next)if(a->OperStatus==IfOperStatusUp && a->IfType!=IF_TYPE_SOFTWARE_LOOPBACK){IP_ADAPTER_UNICAST_ADDRESS *u;for(u=a->FirstUnicastAddress;u;u=u->Next)if(u->Address.lpSockaddr && u->Address.lpSockaddr->sa_family==AF_INET){char *ip=inet_ntoa(((struct sockaddr_in *)u->Address.lpSockaddr)->sin_addr);wchar_t value[48];utf16(ip,value,48);SendMessageW(invitation_address_edit,CB_ADDSTRING,0,(LPARAM)value);found++;}}}free(adapters);if(found)SendMessageW(invitation_address_edit,CB_SETCURSEL,0,0);SetTimer(w,1,1000,NULL);return 0;}
    case WM_COMMAND:if(LOWORD(wp)==2)invitation_generate();return 0;
    case WM_TIMER:if(invitation_until){wchar_t note[160];long left=(long)(invitation_until-time(NULL));if(left<=0){invitation_until=0;invitation_qr[0]=0;lstrcpyW(note,L"Invitation expired. Generate a new code.");}else swprintf(note,160,L"Scan in BitSkim. Expires in %ld seconds.\nIncoming files still require approval.",left);text(invitation_note,note);InvalidateRect(w,NULL,FALSE);}return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,win_themes[prefs.theme].text);SetBkColor(dc,win_themes[prefs.theme].base);return (LRESULT)base_brush[prefs.theme];}
    case WM_DRAWITEM:draw_control((DRAWITEMSTRUCT *)lp,w);return TRUE;
    case WM_ERASEBKGND:{RECT rect;GetClientRect(w,&rect);FillRect((HDC)wp,&rect,base_brush[prefs.theme]);return 1;}
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(w,&paint);RECT r;GetClientRect(w,&r);FillRect(dc,&r,base_brush[prefs.theme]);if(invitation_until && invitation_until>time(NULL)){int size=qrcodegen_getSize(invitation_qr),available=r.bottom-S(150),scale=(available<S(350)?available:S(350))/(size+8),x,y,left=(r.right-(size+8)*scale)/2,top=S(130);RECT white={left,top,left+(size+8)*scale,top+(size+8)*scale};FillRect(dc,&white,(HBRUSH)GetStockObject(WHITE_BRUSH));for(y=0;y<size;y++)for(x=0;x<size;x++)if(qrcodegen_getModule(invitation_qr,x,y)){RECT cell={left+(x+4)*scale,top+(y+4)*scale,left+(x+5)*scale,top+(y+5)*scale};FillRect(dc,&cell,(HBRUSH)GetStockObject(BLACK_BRUSH));}}EndPaint(w,&paint);return 0;}
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_DESTROY:KillTimer(w,1);invitation_window=NULL;invitation_until=0;memset(invitation_qr,0,sizeof invitation_qr);return 0;
    }return DefWindowProcW(w,message,wp,lp);
}
static void show_invitation(void){WNDCLASSW wc;if(invitation_window){SetForegroundWindow(invitation_window);return;}memset(&wc,0,sizeof wc);wc.lpfnWndProc=invitation_proc;wc.hInstance=instance;wc.lpszClassName=L"BitSkimInvitation";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hIcon=app_icon;RegisterClassW(&wc);invitation_window=CreateWindowExW(WS_EX_TOOLWINDOW,wc.lpszClassName,L"BitSkim - Connection QR",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,S(500),S(550),main_window,NULL,instance,NULL);{MONITORINFO monitor;memset(&monitor,0,sizeof monitor);monitor.cbSize=sizeof monitor;if(GetMonitorInfo(MonitorFromWindow(main_window,MONITOR_DEFAULTTONEAREST),&monitor)){RECT fit=bs_window_fit(monitor.rcWork,S(500),S(550));SetWindowPos(invitation_window,NULL,fit.left,fit.top,fit.right-fit.left,fit.bottom-fit.top,SWP_NOZORDER);}}ShowWindow(invitation_window,SW_SHOW);}
