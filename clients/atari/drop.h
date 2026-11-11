/* MiNT AES AP_DRAGDROP: bounded nonblocking ARGS exchange; no dropped data execution. */
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#define DROP_ARGS_MAX 65536UL
static FILE *drop_paths;
static int drop_count,drop_adding,drop_was_paused;
static char drop_id[33],drop_ip[48];
static unsigned short drop_port;
static struct {int fd,state,need,used,reply,count,quoted,quote_end,ended,len;unsigned long remaining,stamp;FILE *paths;char path[CAP];unsigned char bytes[512];void (*oldpipe)(int);} drop={.fd=-1};
static void drop_close(void){if(drop.fd>=0){close(drop.fd);if(drop.oldpipe!=SIG_ERR)signal(SIGPIPE,drop.oldpipe);}if(drop.paths)fclose(drop.paths);memset(&drop,0,sizeof drop);drop.fd=-1;}
static int drop_path(FILE *f,const char *path){char absolute[CAP],record[CAP];if(!bs_at_absolute(path,absolute,sizeof absolute))return 0;memset(record,0,sizeof record);copy(record,absolute,sizeof record);return fwrite(record,sizeof record,1,f)==1;}
static int drop_argument(void){if(!drop.len)return 1;drop.path[drop.len]=0;if(drop.count>=BS_QUEUE_MAX || !drop_path(drop.paths,drop.path))return 0;drop.count++;drop.len=0;return 1;}
static int drop_byte(unsigned char ch){if(drop.ended)return ch==0;if(drop.quote_end){drop.quote_end=0;if(ch=='\'')goto append;drop.quoted=0;}if(ch==0){if(drop.quoted || !drop_argument())return 0;drop.ended=1;return 1;}if(ch=='\''){if(drop.quoted)drop.quote_end=1;else drop.quoted=1;return 1;}if(!drop.quoted && (ch==' ' || ch=='\t'))return drop_argument();
append:if(ch<32 || ch>=127 || drop.len>=CAP-1)return 0;drop.path[drop.len++]=(char)ch;return 1;}
static void drop_ready(FILE *paths,int count){drop_paths=paths;drop_count=count;sprintf(source,"Desktop selection: %d item%s",count,count==1?"":"s");message("Desktop drop ready. Choose the recipient, then click Send File.");}
static int drop_begin(int fd){int flags=fcntl(fd,F_GETFL);if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){close(fd);return 0;}drop.fd=fd;drop.oldpipe=signal(SIGPIPE,SIG_IGN);drop.paths=bsp_temp_file();if(!drop.paths){drop_close();return 0;}drop.stamp=bsp_seconds();drop.need=33;memset(drop.bytes,0,33);memcpy(drop.bytes+1,"ARGS",4);return 1;}
static void drop_open(short *m){char path[]="U:\\PIPE\\DRAGDROP.XX";unsigned int id=(unsigned short)m[7];int a=id>>8,b=id&255,fd;
 if(m[3]!=window || !isalnum(a) || !isalnum(b))return;path[17]=(char)a;path[18]=(char)b;
 fd=open(path,O_RDWR|O_NONBLOCK);if(fd<0)return;
 if(drop.fd>=0 || drop_paths){unsigned char nak=1;void (*old)(int)=signal(SIGPIPE,SIG_IGN);write(fd,&nak,1);close(fd);if(old!=SIG_ERR)signal(SIGPIPE,old);return;}
 if(!drop_begin(fd))return;
 if(page!=OFFER)page=MAIN;
 if(m[5]>=work[1]+3*char_h && m[5]<work[1]+7*char_h){const BSPeer *p=bs_peer(engine,top+(m[5]-work[1])/char_h-3);if(p){copy(selected_id,p->id,33);manual[0]=0;}}
}
static void drop_tick(void){long n;int i;if(drop.fd<0)return;if(bsp_seconds()-drop.stamp>=5){drop_close();message("Desktop drop timed out.");return;}
 if(drop.state==0 || drop.state==3)n=write(drop.fd,drop.bytes+drop.used,(size_t)(drop.need-drop.used));else n=read(drop.fd,drop.bytes+drop.used,(size_t)(drop.need-drop.used));
 if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR))return;if(n<=0){drop_close();message("Desktop drop ended before all paths arrived.");return;}drop.stamp=bsp_seconds();drop.used+=(int)n;if(drop.used<drop.need)return;drop.used=0;
 if(drop.state==0){drop.state=1;drop.need=2;return;}
 if(drop.state==1){drop.need=(drop.bytes[0]<<8)|drop.bytes[1];if(drop.need<10 || drop.need>(int)sizeof drop.bytes)goto invalid;drop.state=2;return;}
 if(drop.state==2){drop.remaining=((unsigned long)drop.bytes[4]<<24)|((unsigned long)drop.bytes[5]<<16)|((unsigned long)drop.bytes[6]<<8)|drop.bytes[7];drop.reply=memcmp(drop.bytes,"ARGS",4)?2:(!drop.remaining || drop.remaining>DROP_ARGS_MAX)?3:0;drop.bytes[0]=(unsigned char)drop.reply;drop.state=3;drop.need=1;return;}
 if(drop.state==3){if(drop.reply)goto invalid;drop.state=4;drop.need=drop.remaining>sizeof drop.bytes?(int)sizeof drop.bytes:(int)drop.remaining;return;}
 for(i=0;i<drop.need;i++)if(!drop_byte(drop.bytes[i]))goto invalid;drop.remaining-=(unsigned long)drop.need;
 if(drop.remaining){drop.need=drop.remaining>sizeof drop.bytes?(int)sizeof drop.bytes:(int)drop.remaining;return;}
 if(drop.quote_end)drop.quoted=0;if(drop.quoted || !drop_argument() || !drop.count || fflush(drop.paths) || fseek(drop.paths,0,SEEK_SET))goto invalid;
 {FILE *paths=drop.paths;int count=drop.count;drop.paths=NULL;drop_close();drop_ready(paths,count);}return;
invalid:drop_close();message("Desktop drop rejected: use an ARGS file/folder list with valid native paths.");
}
static void drop_queue_begin(void){char address[64],*colon,*end;unsigned long p=BS_PORT;if(drop_adding)return;drop_id[0]=drop_ip[0]=0;if(manual[0]){copy(address,manual,sizeof address);colon=strchr(address,':');if(colon){*colon++=0;p=strtoul(colon,&end,10);if(!*colon || *end || !p || p>65535){message("Invalid TCP port.");return;}}if(strlen(address)>=sizeof drop_ip){message("Invalid IPv4 address.");return;}copy(drop_ip,address,sizeof drop_ip);}else if(selected_id[0])copy(drop_id,selected_id,sizeof drop_id);else{message("Select a computer before adding the desktop selection.");return;}drop_port=(unsigned short)p;drop_was_paused=bs_queue_paused(engine);bs_queue_pause(engine);drop_adding=1;}
static void drop_queue_tick(void){char path[CAP];int n=8;if(!drop_adding)return;while(n-- && drop_count){if(fread(path,sizeof path,1,drop_paths)!=1){message("Cannot read the desktop selection.");drop_count=0;break;}if(!(drop_id[0]?bs_queue_peer(engine,drop_id,path):bs_queue_address(engine,drop_ip,drop_port,path))){fseek(drop_paths,-(long)sizeof path,SEEK_CUR);drop_adding=0;if(!drop_was_paused)bs_queue_continue(engine);message(bs_last_error(engine));return;}drop_count--;}
 if(!drop_count){fclose(drop_paths);drop_paths=NULL;drop_adding=0;if(!drop_was_paused)bs_queue_continue(engine);source[0]=0;message("Desktop files and folders queued.");}}
static void drop_launch(int argc,char **argv){int i;FILE *paths;if(argc<=1)return;paths=bsp_temp_file();if(!paths){message("Cannot stage desktop launch paths.");return;}for(i=1;i<argc && i<=BS_QUEUE_MAX;i++)if(!drop_path(paths,argv[i]))break;if(i!=argc || fflush(paths) || fseek(paths,0,SEEK_SET)){fclose(paths);message("Desktop launch paths were not accepted.");return;}drop_ready(paths,argc-1);}

static void drop_discard(void){if(!drop_paths)return;if(drop_adding && !drop_was_paused)bs_queue_continue(engine);fclose(drop_paths);drop_paths=NULL;drop_count=drop_adding=0;source[0]=0;message("Remaining desktop selection discarded.");}
