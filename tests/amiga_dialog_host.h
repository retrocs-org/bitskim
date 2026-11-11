/* Minimal Intuition substitute for exercising actual modeless controllers. */
#include "send_target.h"
typedef unsigned long ULONG;
typedef unsigned short UWORD;
typedef char *STRPTR;
#define DIALOG_OFFER 1
#define DIALOG_SEND 2
#define DIALOG_NOTICE 3
#define DIALOG_BATCH 4
#define DIALOG_TOOLS 5
extern unsigned long bsp_seconds(void);
#define IDCMP_GADGETUP 1
#define IDCMP_CLOSEWINDOW 2
#define IDCMP_REFRESHWINDOW 3
struct StringInfo {char *Buffer;};
struct Gadget {UWORD GadgetID;void *SpecialInfo;};
struct Window {void *UserPort;};
struct IntuiMessage {ULONG Class;void *IAddress;};
typedef struct {struct Window *w;void *screen;struct Gadget *path,*gadgets;int mode,list_top,history_index;unsigned long space_time;unsigned long serial;GuiSendTarget target;char error[256],space[256];} TransferDialog;
static TransferDialog transfer_dialog;
static struct Window fake_window;
static char field[1024],download[512],manual_ip[64];
static int prepare_single_send(BSClient *c){return !bs_status(c)->active && !bs_queue_count(c);}
static struct StringInfo field_info={field};
static struct Gadget field_gadget={40,&field_info},accept_gadget={42,NULL};
static struct IntuiMessage pending_message;
static int auto_accept,ask_location=1,queued,replied,close_before_reply;
static void copy(char *d,const char *s,size_t n) {strncpy(d,s,n-1);d[n-1]=0;}
static void dialog_error(const char *s) {copy(transfer_dialog.error,s,sizeof(transfer_dialog.error));}
static void dialog_close(void) {if(queued) {queued=0;replied++;}if(replied<0)close_before_reply=1;memset(&transfer_dialog,0,sizeof(transfer_dialog));}
static int dialog_open(int mode,const char *heading,const char *detail,const char *path) {(void)heading;(void)detail;memset(&transfer_dialog,0,sizeof(transfer_dialog));transfer_dialog.w=&fake_window;transfer_dialog.path=&field_gadget;transfer_dialog.mode=mode;transfer_dialog.serial=bs_transfer_serial(client);copy(field,path?path:"",sizeof(field));return 1;}
static void client_notice(const char *s) {dialog_error(s);}
static int AddPart(char *base,const char *leaf,long n) {size_t len=strlen(base);if(len+strlen(leaf)+2>=(size_t)n)return 0;if(len && base[len-1]!='/')strcat(base,"/");strcat(base,leaf);return 1;}
static void DisplayBeep(void *s) {(void)s;}
static struct IntuiMessage *GT_GetIMsg(void *port) {(void)port;if(!queued)return NULL;queued=0;replied=-1;return &pending_message;}
static void GT_ReplyIMsg(struct IntuiMessage *m) {(void)m;replied=1;}
static void GT_BeginRefresh(struct Window *w) {(void)w;}
static void GT_EndRefresh(struct Window *w,int b) {(void)w;(void)b;}
static void RefreshGList(struct Gadget *g,struct Window *w,void *r,int n) {(void)g;(void)w;(void)r;(void)n;}
static void dialog_draw(void) {}
#define TRUE 1
