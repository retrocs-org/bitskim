/* Winsock 1.1 cooperative Win16 boundary: no Win32s or packet driver. */
#include "../../core/bitskim.h"
#include "../../core/platform.h"
#include <string.h>
int bs_platform_init(void){WSADATA data;return WSAStartup(0x0101,&data)==0;}
void bs_platform_cleanup(void){WSACleanup();}
unsigned long bsp_millis(void){return GetTickCount();}
unsigned long bsp_seconds(void){return GetTickCount()/1000UL;}
int bsp_close(BSSocket s){return closesocket(s);}
int bsp_nonblock(BSSocket s){u_long yes=1;return ioctlsocket(s,FIONBIO,&yes);}
void bsp_watch_connection(BSSocket s){int yes=1;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(char *)&yes,sizeof yes);}
int bsp_wouldblock(void){int e=WSAGetLastError();return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS || e==WSAEALREADY || e==WSAEINTR;}
int bsp_send(BSSocket s,const void *p,int n){return send(s,p,n,0);}
int bsp_recv(BSSocket s,void *p,int n){return recv(s,p,n,0);}
int bsp_connected(BSSocket s){fd_set w,e;struct timeval tv;int rc,error=0,n=sizeof error;FD_ZERO(&w);FD_ZERO(&e);FD_SET(s,&w);FD_SET(s,&e);tv.tv_sec=tv.tv_usec=0;rc=select(0,NULL,&w,&e,&tv);if(rc<=0)return rc;return getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&error,&n) || error?-1:1;}
unsigned long bsp_wait(const BSSocket *reads,int count,BSSocket writer,unsigned long mask,unsigned long ms){fd_set r,w;struct timeval tv;int i;(void)mask;FD_ZERO(&r);FD_ZERO(&w);for(i=0;i<count;i++)if(reads[i]!=INVALID_SOCKET)FD_SET(reads[i],&r);if(writer!=INVALID_SOCKET)FD_SET(writer,&w);tv.tv_sec=ms/1000;tv.tv_usec=ms%1000*1000;select(0,&r,&w,NULL,&tv);return 0;}
void bsp_broadcast(BSSocket s,const void *p,int n,unsigned short port){struct sockaddr_in address;memset(&address,0,sizeof address);address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=INADDR_BROADCAST;sendto(s,p,n,0,(struct sockaddr *)&address,sizeof address);}
/* DOS file services under Win16 enforce ASCII/8.3, exclusive creation and
   non-replacing rename. No TLS/checkpoint keys are persisted by this port. */
#include "../dos/files.c"
