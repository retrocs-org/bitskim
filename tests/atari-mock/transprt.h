/* Host-only test double for calls, NOT an Atari ABI header. Real build uses SDK. */
#include <stddef.h>
typedef struct {unsigned short lport,rport;unsigned long rhost,lhost;} CAB;
typedef struct {unsigned short protocol;CAB address;unsigned short status;} CIB;
typedef struct {unsigned long request;unsigned short state;} TCPIB;
typedef TCPIB UDPIB;
typedef struct ndb{char *ptr,*ndata;unsigned short len;struct ndb *next;}NDB;
typedef struct {const char *version;} TPL;
typedef struct {char magic[10];void *(*get_dftab)(const char *);}DRV_LIST;
extern DRV_LIST *drivers;extern TPL *tpl;
#define STIK_COOKIE_MAGIC 0x5354694bL
#define STIK_DRVR_MAGIC "STiKmagic"
#define TRANSPORT_DRIVER "TRANSPORT_TCPIP"
#define get_dftab(n) drivers->get_dftab(n)
#define E_NORMAL 0
#define E_OBUFFULL -1
#define E_NODATA -2
#define E_EOF -3
#define E_LISTEN -10
#define E_LOCKED -27
#define TCP_IMMEDIATE 0
#define TCP_HALFDUPLEX -1
#define TCLOSED 0
#define TLISTEN 1
#define TSYN_SENT 2
#define TESTABLISH 4
#define TCLOSE_WAIT 7
#define TCPI_defer 8
#define TCPI_state 1
#define UDPI_defer 8
#define UDP_EXTEND 0
int TCP_open(unsigned long,unsigned short,unsigned short,unsigned short);
int TCP_close(int,int,short *);
int TCP_info(int,TCPIB *);
int TCP_send(int,const void *,short);
int UDP_open(unsigned long,unsigned short);
int UDP_close(int);
int UDP_info(int,UDPIB *);
int UDP_send(int,const void *,short);
CIB *CNgetinfo(int);
NDB *CNget_NDB(int);
int CNbyte_count(int);
int CNget_block(int,void *,short);
void KRfree(void *);
const char *getvstr(const char *);
