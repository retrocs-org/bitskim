#ifndef BITSKIM_ATARI_NET_H
#define BITSKIM_ATARI_NET_H
/* Small socket-shaped boundary for the C core. STinG adapter uses its documented short-argument ABI. */
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
unsigned short bs_at_htons(unsigned short);
unsigned long bs_at_inet_addr(const char *);
char *bs_at_inet_ntoa(struct in_addr);
int bs_at_socket(int,int,int);
int bs_at_bind(int,const struct sockaddr *,int);
int bs_at_listen(int,int);
int bs_at_accept(int,struct sockaddr *,int *);
int bs_at_connect(int,const struct sockaddr *,int);
int bs_at_setsockopt(int,int,int,const char *,int);
int bs_at_shutdown(int,int);
int bs_at_sendto(int,const char *,int,int,const struct sockaddr *,int);
int bs_at_recvfrom(int,char *,int,int,struct sockaddr *,int *);
#ifdef __cplusplus
}
#endif
#ifndef BS_ATARI_NO_SOCKET_MACROS
#define htons bs_at_htons
#define inet_addr bs_at_inet_addr
#define inet_ntoa bs_at_inet_ntoa
#define socket bs_at_socket
#define bind bs_at_bind
#define listen bs_at_listen
#define accept bs_at_accept
#define connect bs_at_connect
#define setsockopt bs_at_setsockopt
#define shutdown bs_at_shutdown
#define sendto bs_at_sendto
#define recvfrom bs_at_recvfrom
#endif
#endif
