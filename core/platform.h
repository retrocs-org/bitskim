#ifndef BS_PLATFORM_H
#define BS_PLATFORM_H
/* glibc hides POSIX/BSD declarations under -std=c89; host tests build on Linux. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <stddef.h>
#if defined(BS_ATARI_STING)
#include "atari-net.h"
#elif defined(BS_DOS)
#include "dos-net.h"
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef BS_WINDOWS_LEGACY
#include <winsock.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <windows.h>
typedef SOCKET BSSocket;
typedef int BSSockLen;
#define BS_INVALID_SOCKET INVALID_SOCKET
#elif defined(__amigaos__)
#include <exec/types.h>
#include <libraries/bsdsocket.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#define inet_ntoa(a) ((char *)Inet_NtoA((a).s_addr))
typedef int BSSocket;
typedef long BSSockLen;
#define BS_INVALID_SOCKET (-1)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int BSSocket;
typedef socklen_t BSSockLen;
#define BS_INVALID_SOCKET (-1)
#endif
#include "tls-api.h"
int bs_to_utf8(const char *, char *, size_t);
int bs_to_local(const char *, char *, size_t);
int bsp_close(BSSocket);
int bsp_nonblock(BSSocket);
void bsp_watch_connection(BSSocket);
int bsp_wouldblock(void);
int bsp_connected(BSSocket);
int bsp_send(BSSocket, const void *, int);
int bsp_recv(BSSocket, void *, int);
unsigned long bsp_seconds(void);
unsigned long bsp_millis(void);
int bsp_size(const char *, unsigned long *);
FILE *bsp_open_file(const char *, const char *);
int bsp_remove_file(const char *);
FILE *bsp_create_file(const char *);
#ifdef _WIN32
FILE *bsp_temp_file(void);
#else
#define bsp_temp_file tmpfile
#endif
#ifdef BS_OPENSSL
FILE *bsp_resume_file(const char *,unsigned long expected_size);
#endif
/* kind: 0 missing, 1 regular file, 2 directory, -1 unsafe/unsupported/error. */
int bsp_path_kind(const char *);
int bsp_mkdir(const char *);
void *bsp_dir_open(const char *);
int bsp_dir_next(void *,char *name,size_t capacity);
void bsp_dir_close(void *);
int bsp_disk_free(const char *,unsigned long *high,unsigned long *low,unsigned long *allocation_unit);
int bsp_commit(const char *temporary, const char *destination);
void bsp_broadcast(BSSocket sock, const void *bytes, int length, unsigned short port);
/* Wait for readability of reads[], writability of writer (may be invalid), a
   signal in sigmask (AmigaOS only) or timeout. Returns received Amiga signals. */
unsigned long bsp_wait(const BSSocket *reads, int count, BSSocket writer, unsigned long sigmask, unsigned long timeout_ms);
#endif
