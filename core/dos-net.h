#ifndef BITSKIM_DOS_NET_H
#define BITSKIM_DOS_NET_H
/* Small socket-shaped boundary for the C core. mTCP itself remains C++. */
typedef int BSSocket;
typedef int BSSockLen;
#define BS_INVALID_SOCKET (-1)
#define AF_INET 2
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_BROADCAST 3
#define INADDR_ANY 0UL
#define INADDR_NONE 0xffffffffUL
struct in_addr { unsigned long s_addr; };
struct sockaddr { unsigned short sa_family; char data[14]; };
struct sockaddr_in { unsigned short sin_family,sin_port; struct in_addr sin_addr; };
#ifdef __cplusplus
extern "C" {
#endif
unsigned short bs_dos_htons(unsigned short);
unsigned long bs_dos_inet_addr(const char *);
char *bs_dos_inet_ntoa(struct in_addr);
int bs_dos_socket(int,int,int);
int bs_dos_bind(int,const struct sockaddr *,int);
int bs_dos_listen(int,int);
int bs_dos_accept(int,struct sockaddr *,int *);
int bs_dos_connect(int,const struct sockaddr *,int);
int bs_dos_setsockopt(int,int,int,const char *,int);
int bs_dos_shutdown(int,int);
int bs_dos_sendto(int,const char *,int,int,const struct sockaddr *,int);
int bs_dos_recvfrom(int,char *,int,int,struct sockaddr *,int *);
void bs_dos_pump(void);
int bs_dos_break(void);
void bs_dos_identity(char *);
#ifdef __cplusplus
}
#endif
#ifndef BS_DOS_NO_SOCKET_MACROS
#define htons bs_dos_htons
#define inet_addr bs_dos_inet_addr
#define inet_ntoa bs_dos_inet_ntoa
#define socket bs_dos_socket
#define bind bs_dos_bind
#define listen bs_dos_listen
#define accept bs_dos_accept
#define connect bs_dos_connect
#define setsockopt bs_dos_setsockopt
#define shutdown bs_dos_shutdown
#define sendto bs_dos_sendto
#define recvfrom bs_dos_recvfrom
#endif
#endif
