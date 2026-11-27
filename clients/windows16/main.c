/* Experimental Windows 3.1+ client; Winsock 1.1 must be installed. */
#include "../../core/bitskim.h"
#include "../../core/platform.h"
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PEERS 100
#define ADDRESS 101
#define SEND 102
#define REFRESH 103
#define CANCEL 104
#define DETAILS 105
static BSClient *client;static HWND peers,address,status;static int offering,dirty=1;static unsigned long shown_serial;
static void core_event(void *data,BSEvent event){(void)data;if(event==BS_EVENT_PEERS)dirty=1;}
static void failure(HWND w){MessageBox(w,bs_user_error(client),"BitSkim",MB_OK|MB_ICONEXCLAMATION);}
static void incoming(HWND w){const BSStatus *s=bs_status(client);OPENFILENAME dialog;char path[128],question[512];unsigned long serial;
 if(offering || !s->awaiting_decision || shown_serial==bs_transfer_serial(client))return;offering=1;shown_serial=serial=bs_transfer_serial(client);
 sprintf(question,"%.63s wants to send %.255s (%lu bytes). Names are unverified. This port uses plaintext. Receive?",s->peer_name,s->filename,s->total);
 if(MessageBox(w,question,"Incoming transfer",MB_YESNO|MB_DEFBUTTON2|MB_ICONQUESTION)==IDYES){
  if(bs_transfer_serial(client)!=serial || !bs_status(client)->awaiting_decision){offering=0;return;}
  if(!bs_suggest_name(s->filename,0,path,sizeof path)){failure(w);bs_decline(client);offering=0;return;}
  memset(&dialog,0,sizeof dialog);dialog.lStructSize=sizeof dialog;dialog.hwndOwner=w;dialog.lpstrFile=path;dialog.nMaxFile=sizeof path;dialog.lpstrTitle="Receive file (ASCII 8.3; existing files preserved)";dialog.Flags=OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
  if(GetSaveFileName(&dialog)){if(bs_transfer_serial(client)==serial && bs_status(client)->awaiting_decision && !bs_accept_conflict(client,path)){failure(w);bs_decline(client);}}
  else if(bs_transfer_serial(client)==serial && bs_status(client)->awaiting_decision)bs_decline(client);
 }else if(bs_transfer_serial(client)==serial && bs_status(client)->awaiting_decision)bs_decline(client);offering=0;
}
static void send_file(HWND w){OPENFILENAME dialog;char path[128]="",ip[48];int index,okay=0;const BSPeer *peer;
 memset(&dialog,0,sizeof dialog);dialog.lStructSize=sizeof dialog;dialog.hwndOwner=w;dialog.lpstrFile=path;dialog.nMaxFile=sizeof path;dialog.lpstrTitle="Send file (ASCII 8.3)";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY;
 if(!GetOpenFileName(&dialog))return;GetWindowText(address,ip,sizeof ip);index=(int)SendMessage(peers,LB_GETCURSEL,0,0);
 if(ip[0])okay=bs_queue_address(client,ip,BS_PORT,path);else if(index>=0 && (peer=bs_peer(client,index))!=NULL)okay=bs_queue_peer_address(client,peer->id,peer->address,peer->port,path);else {MessageBox(w,"Select a client or enter its IPv4 address.","BitSkim",MB_OK);return;}if(!okay)failure(w);
}
LRESULT FAR PASCAL _export window_proc(HWND w,UINT message,WPARAM wp,LPARAM lp){
 if(message==WM_CREATE){char title[160];sprintf(title,"BitSkim 0.2 / Windows 3.x - Build %s - Plaintext",bs_build_id());CreateWindow("STATIC",title,WS_CHILD|WS_VISIBLE,16,12,590,22,w,0,0,NULL);
  peers=CreateWindow("LISTBOX","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,16,40,590,220,w,(HMENU)PEERS,0,NULL);
  CreateWindow("STATIC","Or IPv4 address:",WS_CHILD|WS_VISIBLE,16,276,120,20,w,0,0,NULL);address=CreateWindow("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,145,272,250,24,w,(HMENU)ADDRESS,0,NULL);SendMessage(address,EM_LIMITTEXT,47,0);
  CreateWindow("BUTTON","Send file...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,16,310,140,30,w,(HMENU)SEND,0,NULL);
  CreateWindow("BUTTON","Refresh",WS_CHILD|WS_VISIBLE|WS_TABSTOP,166,310,130,30,w,(HMENU)REFRESH,0,NULL);
  CreateWindow("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,306,310,130,30,w,(HMENU)CANCEL,0,NULL);
  CreateWindow("BUTTON","Diagnostics",WS_CHILD|WS_VISIBLE|WS_TABSTOP,446,310,160,30,w,(HMENU)DETAILS,0,NULL);
  status=CreateWindow("STATIC","Ready",WS_CHILD|WS_VISIBLE,16,355,590,62,w,0,0,NULL);SetTimer(w,1,50,NULL);return 0;}
 if(message==WM_TIMER){char report[512],old[512];int i,index;bs_tick(client);if(dirty){dirty=0;index=(int)SendMessage(peers,LB_GETCURSEL,0,0);SendMessage(peers,LB_RESETCONTENT,0,0);for(i=0;i<bs_peer_count(client);i++){const BSPeer *p=bs_peer(client,i);sprintf(report,"%.63s / %.31s / %.47s:%u",p->name,p->platform,p->address,p->port);SendMessage(peers,LB_ADDSTRING,0,(LPARAM)(LPSTR)report);}if(index>=0)SendMessage(peers,LB_SETCURSEL,index,0);}
  sprintf(report,"%.255s\r\n%lu / %lu bytes",bs_user_status(client),bs_status(client)->transferred,bs_status(client)->total);GetWindowText(status,old,sizeof old);if(strcmp(old,report))SetWindowText(status,report);incoming(w);return 0;}
 if(message==WM_COMMAND){switch(wp){case SEND:send_file(w);break;case REFRESH:bs_refresh(client);break;case CANCEL:bs_cancel(client);bs_queue_clear(client);break;case DETAILS:{char report[4096];bs_diagnostics(client,report,sizeof report);MessageBox(w,report,"BitSkim diagnostics",MB_OK);break;}}return 0;}
 if(message==WM_CLOSE){if(bs_status(client)->active && MessageBox(w,"Cancel the active transfer and quit?","BitSkim",MB_YESNO|MB_DEFBUTTON2)!=IDYES)return 0;DestroyWindow(w);return 0;}
 if(message==WM_DESTROY){KillTimer(w,1);PostQuitMessage(0);return 0;}return DefWindowProc(w,message,wp,lp);
}
int PASCAL WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){WNDCLASS wc;HWND window;MSG msg;BSConfig config;int i;unsigned long seed;char id[33];(void)command;
 if(!bs_platform_init()){MessageBox((HWND)0,"Install a Winsock 1.1 TCP/IP stack before running BitSkim.","BitSkim",MB_OK|MB_ICONSTOP);return 1;}
 memset(&config,0,sizeof config);GetPrivateProfileString("BitSkim","Name","Windows 3.x",config.name,sizeof config.name,"BITSKIM.INI");strcpy(config.platform,"Windows 3.x");GetPrivateProfileString("BitSkim","ID","",id,sizeof id,"BITSKIM.INI");
 if(strlen(id)!=32){seed=GetTickCount()^(unsigned long)instance;for(i=0;i<32;i++){seed=seed*1664525UL+1013904223UL;id[i]="0123456789abcdef"[(seed>>24)&15];}id[32]=0;WritePrivateProfileString("BitSkim","ID",id,"BITSKIM.INI");}strcpy(config.id,id);config.port=BS_PORT;config.visible=config.receiving=1;client=bs_create(&config,core_event,NULL);if(!client){MessageBox((HWND)0,"Cannot listen on TCP/UDP 46440. Close other clients or check TCP/IP.","BitSkim",MB_OK);bs_platform_cleanup();return 1;}
 bs_conflict_policy(client,BS_CONFLICT_RENAME);
 if(!previous){memset(&wc,0,sizeof wc);wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName="BitSkim16";if(!RegisterClass(&wc)){bs_destroy(client);bs_platform_cleanup();return 1;}}
 window=CreateWindow("BitSkim16","BitSkim 0.2 - Experimental Windows 3.x",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX, CW_USEDEFAULT,CW_USEDEFAULT,640,465,(HWND)0,(HMENU)0,instance,NULL);if(!window){bs_destroy(client);bs_platform_cleanup();return 1;}ShowWindow(window,show);UpdateWindow(window);while(GetMessage(&msg,(HWND)0,0,0)){if(!IsDialogMessage(window,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}}bs_destroy(client);bs_platform_cleanup();return (int)msg.wParam;
}
