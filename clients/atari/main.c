/* Native AES/VDI client. Modeless pages keep the bounded engine ticking. */
#define _GNU_SOURCE 1
#include "bitskim.h"
#include "platform.h"
#include "atari.h"
#include <gem.h>
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#ifdef BS_ATARI_STING
#define PLATFORM "Atari TOS"
#define STACK "STinG 1.26"
#else
#define PLATFORM "Atari MiNT"
#define STACK "MiNTNet"
#endif
#define CAP 128
#define MAIN 0
#define PREFS 1
#define OFFER 2
static int offer_top,pref_section;
#define QUIT 4
#define NETWORK 5
#define TOOLS 6
#define CONNECT 7
static char address_draft[64];
static char rate_fields[2][8];
static BSClient *engine;
static short window,vhandle,desktop[4],work[4],char_w=6,char_h=8,text_inset=1;
static int tools_record=-1,tools_top,tools_return;static unsigned long tools_serial;
static int page,previous_page,focus,selected=-1,top,running=1,dirty=1;
static int peers_dirty, footer_dirty, offer_path_dirty;
static char offer_space[256];
static char selected_id[33],manual[64],source[CAP],destination[CAP],notice[160],config_path[CAP];
static unsigned long last_draw,offer_serial,bell_serial;
static int quit_return,peer_count=-1,waiting_release,colour_count=2;

static struct Prefs {char name[64],id[33],folder[CAP];int visible,receiving,automatic,ask,bell;unsigned long send_limit,receive_limit;} prefs,draft;
static int ui_bg,ui_fg=1,ui_panel,ui_accent=1,ui_selected_text;
static short native_colours[256][3];static int native_count,palette_ready;
static int accent(void){return ui_accent;}
static void copy(char *out,const char *in,size_t cap){strncpy(out,in,cap-1);out[cap-1]=0;}
static void message(const char *s){copy(notice,s,sizeof notice);dirty=1;}
static int join(char *out,const char *folder,const char *leaf){size_t n=strlen(folder);if(n+strlen(leaf)+2>=CAP)return 0;strcpy(out,folder);if(n && folder[n-1]!='\\' && folder[n-1]!='/' && folder[n-1]!=':')strcat(out,"\\");strcat(out,leaf);return 1;}
static void load(void){FILE *f;char line[256],*value;int i;unsigned long seed=(unsigned long)time(NULL)^(unsigned long)clock();memset(&prefs,0,sizeof prefs);prefs.visible=prefs.receiving=prefs.ask=1;bs_hostname(prefs.name,sizeof prefs.name);{char *slash;copy(prefs.folder,config_path,CAP);slash=strrchr(prefs.folder,'\\');if(slash)slash[1]=0;}if((prefs.folder[0]=='A' || prefs.folder[0]=='B') && bsp_path_kind("C:\\")==2)copy(prefs.folder,"C:\\",CAP);srand((unsigned)seed);for(i=0;i<32;i++)prefs.id[i]="0123456789abcdef"[rand()&15];if(!(f=fopen(config_path,"r")))return;
 while(fgets(line,sizeof line,f)){line[strcspn(line,"\r\n")]=0;value=strchr(line,'=');if(!value)continue;*value++=0;
 if(!strcmp(line,"name"))copy(prefs.name,value,64);else if(!strcmp(line,"id") && strlen(value)==32){int valid=1;for(i=0;i<32;i++)if(!isxdigit((unsigned char)value[i]))valid=0;if(valid)copy(prefs.id,value,33);}else if(!strcmp(line,"folder"))copy(prefs.folder,value,CAP);
 else if(!strcmp(line,"visible"))prefs.visible=atoi(value)!=0;else if(!strcmp(line,"receiving"))prefs.receiving=atoi(value)!=0;else if(!strcmp(line,"bell"))prefs.bell=atoi(value)!=0;else if(!strcmp(line,"automatic"))prefs.automatic=atoi(value)!=0;else if(!strcmp(line,"ask"))prefs.ask=atoi(value)!=0;else if(!strcmp(line,"send_limit_kib")){if(!bs_limit_parse(value,&prefs.send_limit))prefs.send_limit=0;}else if(!strcmp(line,"receive_limit_kib")){if(!bs_limit_parse(value,&prefs.receive_limit))prefs.receive_limit=0;}}
 fclose(f);
}
static int save(void) {
    char buffer[512];
    long handle, written, closed;
    int length=snprintf(buffer,sizeof buffer,"name=%s\nid=%s\nfolder=%s\nvisible=%d\nreceiving=%d\nautomatic=%d\nask=%d\nbell=%d\nsend_limit_kib=%lu\nreceive_limit_kib=%lu\n",prefs.name,prefs.id,prefs.folder,prefs.visible,prefs.receiving,prefs.automatic,prefs.ask,prefs.bell,prefs.send_limit/1024UL,prefs.receive_limit/1024UL);
    if(length<0 || (size_t)length>=sizeof buffer)return 0;
    handle=Fcreate(config_path,0);
    if(handle<0)return 0;
    written=Fwrite((short)handle,(long)length,buffer);
    closed=Fclose((short)handle);
    return written==length && closed==0;
}
static int width(void){return work[2]/char_w;}
static void text(int col,int row,const char *s){char line[110];int i,max=width()-col-1;if(max<1)return;if(max>109)max=109;for(i=0;i<max && s[i];i++)line[i]=(unsigned char)s[i]>=32 && (unsigned char)s[i]<127?s[i]:'?';line[i]=0;v_gtext(vhandle,work[0]+col*char_w,work[1]+row*char_h+text_inset,line);}
static void bar(int row,const char *s,int selected_row){short box[4];box[0]=work[0]+char_w;box[1]=work[1]+row*char_h;box[2]=work[0]+work[2]-char_w;box[3]=box[1]+char_h-1;vsf_color(vhandle,selected_row?accent():ui_panel);v_bar(vhandle,box);vst_color(vhandle,selected_row?ui_selected_text:ui_fg);text(2,row,s);vst_color(vhandle,ui_fg);}
static void fill_box(short x,short y,short right,short bottom,int color){
 short box[4];
 if(right<x || bottom<y)return;
 box[0]=x;box[1]=y;box[2]=right;box[3]=bottom;
 vsf_color(vhandle,color);v_bar(vhandle,box);
}
static void frame(short x,short y,short right,short bottom,int border,int fill){
 fill_box(x,y,right,bottom,border);
 fill_box(x+1,y+1,right-1,bottom-1,fill);
}
static short paint_clip[4];
static void control(int column,int row,int columns,const char *label,int selected){
 short x=work[0]+column*char_w,y=work[1]+row*char_h;
 if(char_w>=8 && char_h>=18){
  OBJECT object;
  memset(&object,0,sizeof object);
  object.ob_next=object.ob_head=object.ob_tail=-1;
  object.ob_type=G_BUTTON;
  object.ob_flags=OF_LASTOB|OF_SELECTABLE;
  object.ob_state=selected?OS_SELECTED:OS_NORMAL;
  object.ob_spec.free_string=(char *)label;
  object.ob_x=x+2;object.ob_y=y+2;
  object.ob_width=columns*char_w-5;object.ob_height=char_h-5;
  objc_draw(&object,0,0,paint_clip[0],paint_clip[1],
            paint_clip[2]-paint_clip[0]+1,paint_clip[3]-paint_clip[1]+1);
  vs_clip(vhandle,1,paint_clip);
  return;
 }

 frame(x,y,x+columns*char_w-2,y+char_h-2,selected?accent():ui_fg,selected?accent():ui_panel);
 vst_color(vhandle,selected?ui_selected_text:ui_fg);
 text(column+1,row,label);
 vst_color(vhandle,ui_fg);
}
static void button(int column,int row,const char *s){control(column,row,(int)strlen(s)+2,s,0);}
static void input_field(int col,int row,const char *value,int field){
 int room=width()-col-2;
 short x=work[0]+col*char_w,y=work[1]+row*char_h;
 if(room<1)return;
 if(strlen(value)>(size_t)room)value+=strlen(value)-room;
 frame(x,y,work[0]+work[2]-char_w,y+char_h-2,focus==field?accent():ui_fg,ui_panel);
 text(col+1,row,value);
}
static void line_field(int row,const char *label,const char *value,int field){
 text(1,row,label);
 input_field(9,row,value,field);
}
static void size_text(unsigned long n,char *s){if(n<1024)sprintf(s,"%luB",n);else if(n<1048576UL)sprintf(s,"%luK",n/1024);else sprintf(s,"%luM",n/1048576UL);}
static unsigned long percent(unsigned long done,unsigned long total){unsigned long low=0,high=100;if(!total)return 0;while(low<high){unsigned long mid=(low+high+1)/2,threshold=(total/100)*mid+((total%100)*mid+99)/100;if(done>=threshold)low=mid;else high=mid-1;}return low;}
#include "ui.h"
static void paint(const short clip[4]){char line[256];const BSStatus *s=bs_status(engine);int i;
 memcpy(paint_clip,clip,sizeof paint_clip);palette_select();vsf_interior(vhandle,FIS_SOLID);vsf_color(vhandle,ui_bg);v_bar(vhandle,(short *)clip);vst_color(vhandle,ui_fg);
 if(clip[1]<work[1]+char_h){vst_color(vhandle,accent());text(1,0,"BitSkim 0.2 / " STACK " / F1 Tools");vst_color(vhandle,ui_fg);}
 if(clip[1]<work[1]+13*char_h){
 if(page==TOOLS){int row;char report[81];int columns=width()-2;const char *policy=bs_conflict_get(engine)==2?"REPLACE":bs_conflict_get(engine)==1?"SKIP":"RENAME";if(columns>80)columns=80;if(columns<10)columns=10;
    if(tools_record>=0 && tools_serial){tools_record=bs_history_find(engine,tools_serial);if(tools_record<0)tools_record=-2;}
    sprintf(line,"Transfer tools / conflicts: %s",policy);text(1,2,line);
    for(row=0;row<7;row++){bs_tools_line(engine,tools_record,tools_top+row,report,(size_t)columns+1);text(1,3+row,report);}
    button(1,11,"Pg");button(6,11,"Hist");button(13,11,"Old");button(20,11,"Retry");button(28,11,"Rule");button(35,11,"Back");button(1,10,"Test connection");
 }else if(page==MAIN){text(1,2,"Nearby computers");if(clip[1]<work[1]+7*char_h && clip[3]>=work[1]+3*char_h){frame(work[0]+char_w-2,work[1]+3*char_h,work[0]+work[2]-char_w+2,work[1]+7*char_h-1,ui_fg,ui_panel);for(i=0;i<4;i++){const BSPeer *p=bs_peer(engine,top+i);line[0]=0;if(p)sprintf(line,"%.24s  %s",p->name,p->address);if(p)bar(3+i,line,top+i==selected);}}if(clip[3]>=work[1]+8*char_h){button(1,8,"Connect by IP");if(manual[0])text(17,8,manual);button(1,11,"Send File");button(13,11,"Send Directory");button(1,12,"Scan");button(10,12,"Prefs");button(20,12,"Quit");}
 }else if(page==CONNECT){text(1,3,"Connect by IP");text(1,5,"Enter IPv4 address or IPv4:port");line_field(7,"IP",address_draft,focus);button(1,9,"Connect");button(15,9,"Cancel");
 }else if(page==PREFS){paint_preferences();
 }else if(page==OFFER){text(1,1,offer_space);if((bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)) {
  int index,row=3;char save_name[256];bs_batch_summary(engine,line,sizeof line);text(1,2,line);
  for(index=offer_top;index<bs_batch_count(engine) && row<10;index++) {const BSBatchItem *item=bs_batch_item(engine,index);if(!bs_batch_save_name(engine,index,save_name,sizeof save_name))strcpy(save_name,"unsupported");if(bs_batch_is_tree(engine)){int columns=width()-2,chunk;size_t len=strlen(save_name);if(columns>100)columns=100;if(columns<10)columns=10;for(chunk=0;chunk<7;chunk++){const char *part=(size_t)(chunk*columns)<len?save_name+chunk*columns:"";text(1,row++,part);}break;}else {sprintf(line,"%.12s -> %.12s  %lu B",item->filename,save_name,item->total);text(1,row++,line);}}
  sprintf(line,"F2: existing %s / Up,Down browse",bs_conflict_get(engine)==2?"REPLACE":bs_conflict_get(engine)==1?"SKIP":"RENAME");text(1,10,line);line_field(11,"Folder",destination,4);button(1,12,"Accept all");button(17,12,"Decline");button(29,12,"Browse...");
 }else{text(1,2,"Incoming file / sender name unverified");sprintf(line,"From: %.63s",s->peer_name);text(1,4,line);sprintf(line,"File: %.200s",s->filename);text(1,5,line);sprintf(line,"Size: %lu bytes",s->total);text(1,6,line);line_field(8,"Save",destination,4);sprintf(line,"F2: existing %s",bs_conflict_get(engine)==2?"REPLACE":bs_conflict_get(engine)==1?"SKIP":"RENAME");text(1,9,line);button(1,11,"Accept");button(15,11,"Decline");
} 
 }else{ text(1,3,"Quit BitSkim?");text(1,5,"An active transfer will be cancelled.");button(1,9,"Quit");button(15,9,"Back");}
 }
 if(clip[3]<work[1]+13*char_h)return;
 if(s->active && page!=OFFER) {unsigned long position=1,total=1;
  if(!s->incoming && bs_queue_count(engine))bs_queue_progress(engine,&position,&total);
  if(s->incoming && (bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0))sprintf(line,"Receiving %d/%d: %.180s",bs_batch_index(engine)+1,bs_batch_count(engine),s->filename);else if(s->incoming)sprintf(line,"Receiving: %.180s",s->filename);else sprintf(line,"File %lu/%lu: %.180s",position,total,s->filename);text(1,13,line);
 }else text(1,13,notice[0]?notice:s->message);
 if(s->active || bs_queue_scanning(engine)){char done[20],total[20],left[20],rate[20];unsigned long pct=percent(s->transferred,s->total);size_text(s->transferred,done);size_text(s->total,total);size_text(s->total-s->transferred,left);size_text(bs_transfer_rate(engine),rate);sprintf(line,"%s/%s %lu%% | %s left | %s/s",done,total,pct,left,rate);text(1,14,line);button(1,15,"Cancel transfer");
 if(work[3]>17*char_h){
  bs_progress_summary(engine,line,sizeof line);text(1,17,line);
  short x=work[0]+char_w,y=work[1]+16*char_h+1,right=work[0]+work[2]-char_w;
  frame(x,y,right,y+char_h-3,ui_fg,ui_panel);
  if(pct)fill_box(x+1,y+1,x+(short)((long)(right-x-1)*pct/100),y+char_h-4,accent());
 }
 }
 else{strcpy(line,page==CONNECT?"Enter: connect   Esc: cancel":"Esc: back/quit");text(1,14,line);}
}
/* Ignore discovery timestamps so repeated announcements do not repaint GEM. */
static void check_display(const BSStatus *status)
{
 static BSPeer shown[4];
 static char message_text[256];
 static int active, scanning, waiting, paused;
 int i, scan=bs_queue_scanning(engine), count=bs_queue_waiting(engine);
 int pause=bs_queue_paused(engine);
 if(active!=status->active || scanning!=scan || waiting!=count ||
    paused!=pause || strcmp(message_text,status->message)){
  active=status->active;scanning=scan;waiting=count;paused=pause;
  strcpy(message_text,status->message);
  footer_dirty=1;
 }
 if(page!=MAIN)return;
 for(i=0;i<4;i++){
  const BSPeer *peer=bs_peer(engine,top+i);
  if(!peer){
   if(shown[i].id[0]){shown[i].id[0]=0;peers_dirty=1;}
  }else if(strcmp(shown[i].id,peer->id) || strcmp(shown[i].name,peer->name) ||
           strcmp(shown[i].address,peer->address)){
   shown[i]=*peer;
   peers_dirty=1;
  }
 }
}
static void redraw(short x,short y,short w,short h){short r[4],clip[4];int left,right,bottom,top_y;wind_update(BEG_UPDATE);graf_mouse(M_OFF,NULL);wind_get(window,WF_FIRSTXYWH,&r[0],&r[1],&r[2],&r[3]);while(r[2] && r[3]){left=r[0]>x?r[0]:x;top_y=r[1]>y?r[1]:y;right=r[0]+r[2]<x+w?r[0]+r[2]:x+w;bottom=r[1]+r[3]<y+h?r[1]+r[3]:y+h;if(left<right && top_y<bottom){clip[0]=left;clip[1]=top_y;clip[2]=right-1;clip[3]=bottom-1;vs_clip(vhandle,1,clip);paint(clip);}wind_get(window,WF_NEXTXYWH,&r[0],&r[1],&r[2],&r[3]);}vs_clip(vhandle,0,clip);graf_mouse(M_ON,NULL);wind_update(END_UPDATE);}
#include "drop.h"
static void send_path(int directory){char path[CAP],address[64],*colon,*end;unsigned short port=BS_PORT;int ok;if(!source[0]){message("Choose a file first.");return;}if(!manual[0] && !selected_id[0]){message("Select a computer or enter its IPv4 address.");return;}if(!bs_at_absolute(source,path,sizeof path)){message("Use a valid ASCII file path (TOS: 8.3 names).");return;}if(bsp_path_kind(path)!=(directory?2:1)){message(directory?"Choose a directory.":"Choose a regular file.");return;}if(manual[0]){copy(address,manual,sizeof address);colon=strchr(address,':');if(colon){long n;*colon++=0;n=strtol(colon,&end,10);if(*end || n<1 || n>65535){message("Invalid TCP port.");return;}port=(unsigned short)n;}ok=bs_queue_address(engine,address,port,path);}else ok=bs_queue_peer(engine,selected_id,path);if(ok){bs_queue_continue(engine);notice[0]=0;footer_dirty=1;}else message(bs_last_error(engine));}
static void choose_send(int directory)
{
 char mask[CAP],leaf[CAP],*slash;
 short chosen=0;
 if(bs_status(engine)->active || bs_queue_scanning(engine) || drop_adding){
  message("A transfer is already active.");return;
 }
 if(bs_queue_paused(engine))bs_queue_clear(engine);
 if(bs_queue_count(engine)){message("A transfer is already pending.");return;}
 if(drop_paths){if(drop_count!=1){drop_discard();message("Drop one file or directory.");return;}drop_queue_begin();return;}
 if(!manual[0] && !selected_id[0]){message("Select a computer or enter its IPv4 address.");return;}
 if(!join(mask,bs_last_folder(engine,0)[0]?bs_last_folder(engine,0):prefs.folder,"*.*")){message("Folder path is too long.");return;}
 if(directory && form_alert(1,"[1][Open the directory, then click OK.][Choose|Cancel]")!=1)return;
 leaf[0]=0;
 fsel_input(mask,leaf,&chosen);
 dirty=1;
 if(!chosen)return;
 slash=strrchr(mask,'\\');
 if(directory){
  if(!slash || slash<=mask+2){message("Choose a directory below the drive root.");return;}
  *slash=0;
 }else{
  if(!leaf[0]){message("Choose a file first.");return;}
  if(slash)slash[1]=0;else mask[0]=0;
  if(strlen(mask)+strlen(leaf)>=CAP){message("File path too long.");return;}
  strcat(mask,leaf);
 }
 copy(source,mask,CAP);
 focus=1;
 send_path(directory);
}
static void browse_destination(void){
 char mask[CAP],leaf[CAP],*slash;
 short chosen=0;
 if(!join(mask,destination,"*.*") && !join(mask,prefs.folder,"*.*")){
  message("Folder path is too long.");return;
 }
 leaf[0]=0;
 fsel_input(mask,leaf,&chosen);
 if(chosen && (slash=strrchr(mask,92))!=NULL){
  slash[1]=0;
  copy(destination,mask,sizeof destination);
  offer_path_dirty=1;
 }
}
static void accept_offer(void){char native[CAP];
 if((bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)) {char dummy[CAP],*slash;
  if(!join(dummy,destination,"BSFOLDER.TMP") || !bs_at_absolute(dummy,native,sizeof native)){message("Choose an existing ASCII folder path.");return;}
  slash=strrchr(native,'\\');if(!slash){message("Choose a folder path.");return;}slash[1]=0;
  if(!bs_accept_batch(engine,native))message(bs_last_error(engine));else{page=previous_page;notice[0]=0;dirty=1;}return;
 }
if(!bs_at_absolute(destination,native,sizeof native)){message("Use an ASCII save path (TOS: 8.3 filename).");return;}copy(destination,native,CAP);if(!bs_accept_conflict(engine,destination))message(bs_last_error(engine));else{page=previous_page;notice[0]=0;dirty=1;}}
static void tools_action(int col){BSHistoryItem h;char line[81];int columns=width()-2;if(columns>80)columns=80;if(columns<10)columns=10;
    if(col<6){if(bs_tools_line(engine,tools_record,tools_top+7,line,(size_t)columns+1))tools_top+=7;else tools_top=0;}
    else if(col<13){tools_record=tools_record<0?0:-1;tools_top=0;if(bs_history_item(engine,tools_record,&h))tools_serial=h.serial;}
    else if(col<20){tools_record++;if(tools_record>=bs_history_count(engine))tools_record=0;tools_top=0;if(bs_history_item(engine,tools_record,&h))tools_serial=h.serial;}
    else if(col<28){int i;for(i=0;i<bs_history_count(engine);i++)if(bs_history_item(engine,i,&h) && h.serial==tools_serial){if(!bs_retry_history(engine,i))message(bs_last_error(engine));break;}}
    else if(col<35)bs_conflict_policy(engine,(bs_conflict_get(engine)+1)%3);else page=tools_return;dirty=1;
}
static void connect_address(void){
 unsigned int a,b,c,d;
 unsigned long port=BS_PORT;
 int used=0,port_used=0;
 const char *suffix;
 if(sscanf(address_draft,"%3u.%3u.%3u.%3u%n",&a,&b,&c,&d,&used)!=4 ||
    strspn(address_draft,"0123456789.:")!=strlen(address_draft) ||
    a>255 || b>255 || c>255 || d>255){message("Enter a valid IPv4 address.");return;}
 suffix=address_draft+used;
 if(*suffix && (*suffix!=':' || strlen(suffix)>6 ||
    sscanf(suffix+1,"%lu%n",&port,&port_used)!=1 || suffix[1+port_used] ||
    !port || port>65535U)){message("Enter a TCP port from 1 to 65535.");return;}
 sprintf(manual,"%u.%u.%u.%u:%lu",a,b,c,d,port);
 selected=-1;selected_id[0]=0;notice[0]=0;page=MAIN;dirty=1;
}
static void request_quit(void){
 if(!bs_status(engine)->active && !bs_queue_scanning(engine) && !drop_adding){
  running=0;
 }else if(page!=QUIT){
  quit_return=page;
  page=QUIT;
  dirty=1;
 }
}
static void click(int col,int row){if(page==PREFS){preferences_click(col,row);return;}if(page==TOOLS){if(row==10 && col>=1 && col<19){if(!bs_network_test(engine,manual[0]?manual:NULL,0))message(bs_last_error(engine));tools_record=-1;tools_top=0;dirty=1;}if(row==11)tools_action(col);return;}if(row==15 && (bs_status(engine)->active || bs_queue_scanning(engine))){bs_cancel(engine);bs_queue_clear(engine);message("Transfer cancelled.");return;}
 if(page==MAIN){if(row>=3 && row<=6){const BSPeer *p=bs_peer(engine,top+row-3);if(p){selected=top+row-3;copy(selected_id,p->id,33);manual[0]=0;}}else if(row==8 && col>=1 && col<16){copy(address_draft,manual,sizeof address_draft);focus=0;page=CONNECT;}else if(row==11){if(col>=1 && col<12)choose_send(0);else if(col>=13 && col<29)choose_send(1);}else if(row==12){if(col>=1 && col<7)bs_refresh(engine);else if(col>=10 && col<17)open_preferences();else if(col>=20 && col<26){request_quit();}}}
 else if(page==CONNECT){if(row==9 && col>=1 && col<10)connect_address();else if(row==9 && col>=15 && col<23){notice[0]=0;page=MAIN;}}
 else if(page==OFFER){int batch=(bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0);if(row==(batch?11:8))focus=4;else if(row==(batch?12:11)){if(batch && col>=29 && col<40){browse_destination();return;}if(col>=1 && col<(batch?13:10))accept_offer();else if(col>=(batch?17:15) && col<(batch?26:24)){bs_decline(engine);page=previous_page;}else return;}}
 else if(row==9){if(col<14)running=0;else page=quit_return;}
 dirty=1;
}
static void key(unsigned short code){if(page==PREFS){preferences_key(code);return;}if((code>>8)==0x3b){if(page!=TOOLS)tools_return=page;page=TOOLS;tools_record=-1;tools_top=0;dirty=1;return;}unsigned char ch=(unsigned char)code;char *field=NULL;size_t cap=0,n;if(ch==27){if(page==CONNECT){notice[0]=0;page=MAIN;dirty=1;return;}if(page==MAIN && drop_paths){drop_discard();return;}if(page==TOOLS){page=tools_return;dirty=1;return;}if(page==NETWORK){page=PREFS;focus=2;dirty=1;return;}if(page==OFFER)bs_decline(engine);if(page==MAIN){request_quit();}else if(page==QUIT)page=quit_return;else page=MAIN;dirty=1;return;}if(ch==9){if(page==PREFS)focus=focus==2?3:2;else if(page==NETWORK)focus=focus==5?6:5;dirty=1;return;}
 if(page==OFFER && ((code>>8)==0x3c)){bs_conflict_policy(engine,(bs_conflict_get(engine)+1)%3);dirty=1;return;}
 if(page==OFFER && (bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0)){if((code>>8)==0x48){if(offer_top>bs_batch_index(engine))offer_top--;dirty=1;return;}if((code>>8)==0x50){if(offer_top+(bs_batch_is_tree(engine)?1:7)<bs_batch_count(engine))offer_top++;dirty=1;return;}}
 if(page==MAIN){if((code>>8)==0x48){if(top>0)top--;dirty=1;return;}if((code>>8)==0x50){if(top+4<bs_peer_count(engine))top++;dirty=1;return;}}else if(page==CONNECT){if(ch==13){connect_address();return;}field=address_draft;cap=sizeof address_draft;}else if(page==PREFS){field=focus==3?draft.folder:draft.name;cap=focus==3?CAP:64;}else if(page==NETWORK){field=rate_fields[focus==6?1:0];cap=8;}else if(page==OFFER){field=destination;cap=CAP;}
 if(!field)return;n=strlen(field);if(ch==21)field[0]=0;else if(ch==8 && n)field[n-1]=0;else if(ch>=32 && ch<127 && (page!=NETWORK || (ch>='0' && ch<='9')) && n+1<cap){field[n]=ch;field[n+1]=0;}if(page==OFFER)offer_path_dirty=1;else dirty=1;
}
int main(int argc,char **argv){short input[11],output[57],msg[8],mx,my,buttons,meta,pressed,clicks,events,handle,cw,ch,bw,bh;int i;char wire[64];short bx,by,bw2,bh2;
 if(appl_init()<0)return 1;handle=graf_handle(&cw,&ch,&bw,&bh);for(i=0;i<10;i++)input[i]=1;input[10]=2;vhandle=handle;v_opnvwk(input,&vhandle,output);colour_count=output[13];if(!vhandle){appl_exit();return 1;}
 vswr_mode(vhandle,MD_TRANS);
 vst_alignment(vhandle,0,5,&cw,&ch);

 vst_height(vhandle,6,&cw,&ch,&char_w,&char_h);if(char_h<8)char_h=8;char_h+=1;
 wind_get(0,WF_WORKXYWH,&desktop[0],&desktop[1],&desktop[2],&desktop[3]);
 if(desktop[3]>=280){char_h+=5;text_inset+=2;}
 if(desktop[3]>=380 && char_h<20){text_inset+=(20-char_h)/2;char_h=20;}
 if(desktop[2]<320 || desktop[3]<170){form_alert(1,"[1][BitSkim needs at least 320x200.][OK]");v_clsvwk(vhandle);appl_exit();return 1;}
 if(!bs_at_absolute("BITSKIM.CFG",config_path,sizeof config_path)){v_clsvwk(vhandle);appl_exit();return 1;}
 if(!bs_platform_init()){form_alert(1,"[1][" STACK " is unavailable.|Start and configure the stack first.][OK]");v_clsvwk(vhandle);appl_exit();return 1;}
 load();
 if(!bs_to_utf8(prefs.name,wire,sizeof wire))copy(wire,"Atari",64);engine=bs_create_app(wire,PLATFORM,prefs.id,prefs.visible,prefs.receiving);if(!engine){form_alert(1,"[1][Cannot listen on TCP/UDP 46440.][OK]");bs_platform_cleanup();v_clsvwk(vhandle);appl_exit();return 1;}
 {char store[CAP],*slash;copy(store,config_path,CAP);slash=strrchr(store,92);if(slash)copy(slash+1,"BSFOLD.DAT",(size_t)(store+CAP-slash-1));else copy(store,"BSFOLD.DAT",CAP);bs_folders_configure(engine,store);}
 save();window=wind_create(NAME|CLOSER|MOVER,desktop[0],desktop[1],desktop[2],desktop[3]);if(window<0){bs_destroy(engine);bs_platform_cleanup();v_clsvwk(vhandle);appl_exit();return 1;}wind_set_str(window,WF_NAME,"BitSkim 0.2 / " STACK);
 bs_limits_configure(engine,prefs.send_limit,prefs.receive_limit);bs_batch_enable(engine,1);
 bw2=desktop[2]>620?620:desktop[2];bh2=17*char_h+24;if(bh2>desktop[3])bh2=desktop[3];bx=desktop[0]+(desktop[2]-bw2)/2;by=desktop[1]+(desktop[3]-bh2)/2;wind_open(window,bx,by,bw2,bh2);wind_get(window,WF_WORKXYWH,&work[0],&work[1],&work[2],&work[3]);graf_mouse(ARROW,NULL);palette_read();drop_launch(argc,argv);
 while(running){const BSStatus *s;unsigned long now;drop_tick();drop_queue_tick();bs_tick(engine);if(bs_completion_notice(engine,&bell_serial) && prefs.bell){fputc(7,stderr);fflush(stderr);}s=bs_status(engine);now=bs_transfer_serial(engine);
  if(s->awaiting_decision && now!=offer_serial){offer_serial=now;offer_top=bs_batch_index(engine);previous_page=page;page=OFFER;focus=4;wind_set(window,WF_TOP,0,0,0,0);notice[0]=0;if((bs_batch_count(engine)>1 || bs_batch_is_tree(engine)!=0))copy(destination,bs_last_folder(engine,1)[0]?bs_last_folder(engine,1):prefs.folder,CAP);else {char leaf[256];if(!bs_suggest_name(s->filename,0,leaf,sizeof leaf) || !join(destination,bs_last_folder(engine,1)[0]?bs_last_folder(engine,1):prefs.folder,leaf))destination[0]=0;}
#ifdef BS_ATARI_STING
   copy(notice,"Save as ASCII 8.3 (Ctrl+U clears the path).",sizeof notice);
#endif
   offer_path_dirty=1;if(prefs.automatic && !prefs.ask)accept_offer();dirty=1;}
  if(page==OFFER && !s->awaiting_decision){page=previous_page;dirty=1;}
  if(peer_count!=bs_peer_count(engine)){peer_count=bs_peer_count(engine);}selected=-1;for(i=0;i<peer_count;i++)if(!strcmp(bs_peer(engine,i)->id,selected_id))selected=i;
  check_display(s);
  if(page==OFFER && offer_path_dirty){
   char native[CAP];
   offer_space[0]=0;
   if(bs_at_absolute(destination,native,sizeof native))
    bs_destination_space_report(engine,native,bs_batch_count(engine)>1 || bs_batch_is_tree(engine),offer_space,sizeof offer_space);
   if(!dirty){
    int row=bs_batch_count(engine)>1 || bs_batch_is_tree(engine)?11:8;
    redraw(work[0],work[1]+char_h,work[2],char_h);
    redraw(work[0],work[1]+row*char_h,work[2],char_h);
   }
  }
  offer_path_dirty=0;
  if(page==TOOLS && bsp_seconds()!=last_draw){static char prior[2048];char report[2048];bs_diagnostics(engine,report,sizeof report);if(strcmp(prior,report)){copy(prior,report,sizeof prior);dirty=1;}last_draw=bsp_seconds();}
  if(((s->active && !s->awaiting_decision) || bs_queue_scanning(engine)) && bsp_seconds()!=last_draw)footer_dirty=1;
  if(dirty || (page==TOOLS && footer_dirty)){
   redraw(work[0],work[1],work[2],work[3]);
  }else{
   if(page==MAIN && peers_dirty)redraw(work[0],work[1]+3*char_h,work[2],4*char_h);
   if(footer_dirty && work[3]>13*char_h)
    redraw(work[0],work[1]+13*char_h,work[2],work[3]-13*char_h);
  }
  if(dirty || footer_dirty)last_draw=bsp_seconds();
  dirty=peers_dirty=footer_dirty=0;
  events=evnt_multi(MU_MESAG|MU_TIMER|MU_KEYBD|MU_BUTTON,1,1,waiting_release?0:1,0,0,0,0,0,0,0,0,0,0,msg,50,&mx,&my,&buttons,&meta,&pressed,&clicks);
  if(events&MU_MESAG){if(msg[0]==63)drop_open(msg);else if(msg[0]==WM_REDRAW)redraw(msg[4],msg[5],msg[6],msg[7]);else if(msg[0]==WM_CLOSED){request_quit();}else if(msg[0]==WM_TOPPED)wind_set(window,WF_TOP,0,0,0,0);else if(msg[0]==WM_MOVED){wind_set(window,WF_CURRXYWH,msg[4],msg[5],msg[6],msg[7]);wind_get(window,WF_WORKXYWH,&work[0],&work[1],&work[2],&work[3]);dirty=1;}else if(msg[0]==AP_TERM)running=0;}
  if((events&MU_BUTTON) && mx>=work[0] && mx<work[0]+work[2] && my>=work[1] && my<work[1]+work[3]){if(!waiting_release)click((mx-work[0])/char_w,(my-work[1])/char_h);}
  if(events&MU_BUTTON)waiting_release=(buttons&1)!=0;
  if(events&MU_KEYBD)key((unsigned short)pressed);
 }
 drop_close();if(drop_paths)fclose(drop_paths);wind_close(window);wind_delete(window);bs_cancel(engine);bs_destroy(engine);bs_platform_cleanup();v_clsvwk(vhandle);appl_exit();return 0;
}
