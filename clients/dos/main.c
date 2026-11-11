#include "bitskim.h"
/* A directory can have multiple internal entries, but only one user send. */
static int prepare_single_send(BSClient *client)
{
    if(bs_status(client)->active || bs_queue_scanning(client) ||
       (bs_queue_count(client) && !bs_queue_paused(client))) return 0;
    /* Keep an explicitly paused queue when adding another user selection. */
    return 1;
}
#include "platform.h"
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned long send_limit,receive_limit,bell_serial;static int completion_bell;
static BSClient *client; static BSConfig config;
static int redraw=1,offer_seen,files_changed;
static char settings_path[BS_PATH_MAX+1]="BITSKIM.CFG"; static char input[192]; static unsigned used;
static void event(void *context,BSEvent what) { (void)context; redraw=1;if(what==BS_EVENT_COMPLETE)files_changed=1; }
static void help(void) {
    puts("BitSkim 0.1 DOS / mTCP - unencrypted local file sharing");
    puts("LIST | SEND <number> <8.3 path> | SENDIP <IPv4[:port]> <8.3 path>");
    puts("ACCEPT <new 8.3 path> | ACCEPTALL <folder> | DECLINE | CANCEL | STATUS");
    puts("TEST <IPv4[:port]> | DIAGNOSTICS | HISTORY | RETRY <number> | CONFLICT RENAME/SKIP/REPLACE");
    puts("LIMITS <send KiB/s> <receive KiB/s> (0 = Unlimited)");
    puts("NAME <ASCII name> | VISIBLE ON/OFF | RECEIVE ON/OFF | HELP | QUIT");
    puts("Incoming batches list all files; ACCEPTALL chooses one existing folder.");
}
static int save_config(void) {
    FILE *f=fopen(settings_path,"w"); if(!f) return 0;
    fprintf(f,"ID=%s\nNAME=%s\nVISIBLE=%d\nRECEIVE=%d\nSEND_KIB=%lu\nRECEIVE_KIB=%lu\nBELL=%d\n",config.id,config.name,config.visible,config.receiving,send_limit/1024UL,receive_limit/1024UL,completion_bell); return fclose(f)==0;
}
static int read_config(void) {
    char line[160]; const char *port_text=getenv("BITSKIM_PORT"); FILE *f=fopen(settings_path,"r");
    memset(&config,0,sizeof config); bs_hostname(config.name,sizeof config.name); strcpy(config.platform,"MS-DOS"); config.port=config.discovery_port=BS_PORT; config.visible=config.receiving=1;
    if(f) { while(fgets(line,sizeof line,f)) { char *value=strchr(line,'='); if(!value) continue; *value++=0; value[strcspn(value,"\r\n")]=0;
        if(!strcmp(line,"NAME") && *value && strlen(value)<=BS_NAME_MAX) strcpy(config.name,value);
        else if(!strcmp(line,"ID") && strlen(value)==32) strcpy(config.id,value);
        else if(!strcmp(line,"BELL")) completion_bell=atoi(value)!=0;
        else if(!strcmp(line,"VISIBLE")) config.visible=atoi(value)!=0;
        else if(!strcmp(line,"SEND_KIB")){if(!bs_limit_parse(value,&send_limit))send_limit=0;}
        else if(!strcmp(line,"RECEIVE_KIB")){if(!bs_limit_parse(value,&receive_limit))receive_limit=0;}
        else if(!strcmp(line,"RECEIVE")) config.receiving=atoi(value)!=0;
    } fclose(f); }
    if(!config.id[0]) bs_dos_identity(config.id);
    if(port_text) {char *end;unsigned long port=strtoul(port_text,&end,10);if(!*port_text || *end || port<1 || port>65535) {puts("Invalid BITSKIM_PORT (use 1-65535).");return 0;}config.port=(unsigned short)port;}
    return 1;
}
static void peers(void) { int i; printf("\nNearby (%d):\n",bs_peer_count(client)); for(i=0;i<bs_peer_count(client);i++) { const BSPeer *p=bs_peer(client,i); char label[64]; if(!bs_to_local(p->name,label,sizeof label)) strcpy(label,"[non-ASCII name]"); printf("%2d  %-24s %-12s %s\n",i+1,label,p->platform,p->address); } }
static void status(void) {
    const BSStatus *s=bs_status(client); printf("\n%s\n",s->message);
    if(s->active) printf("%s: %lu / %lu bytes, %lu left, %lu bytes/s\n",s->filename,s->transferred,s->total,s->total-s->transferred,bs_transfer_rate(client));
    if(s->awaiting_decision && !offer_seen) { offer_seen=1; printf("Offer from %s (%s).\n",s->peer_name,s->peer_address);
      if((bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0)) {int i;char name[256];bs_batch_summary(client,name,sizeof name);puts(name);for(i=bs_batch_index(client);i<bs_batch_count(client);i++){const BSBatchItem *item=bs_batch_item(client,i);bs_batch_save_name(client,i,name,sizeof name);printf("  %s -> %s (%lu bytes)\n",item->filename,name,item->total);}puts("Type ACCEPTALL <folder> to receive this batch, or DECLINE.");}
      else puts("Type ACCEPT <new 8.3 path> or DECLINE."); }
    if(!s->awaiting_decision) offer_seen=0;
}
static int command(char *line) {
    char *argument=strchr(line,' '),*path; int ok=1;
    if(argument) { *argument++=0; while(*argument==' ') argument++; } else argument=line+strlen(line);
    strupr(line);
    if(!strcmp(line,"QUIT")) return 0;
    if(!strcmp(line,"DIAGNOSTICS")){char report[2048];bs_diagnostics(client,report,sizeof report);puts(report);}
    else if(!strcmp(line,"TEST"))ok=bs_network_test(client,argument,0);
    else if(!strcmp(line,"HISTORY")){int i;char report[2048];for(i=0;i<bs_history_count(client);i++){bs_history_text(client,i,report,sizeof report);printf("\n%d: %s\n",i+1,report);}}
    else if(!strcmp(line,"RETRY"))ok=bs_retry_history(client,atoi(argument)-1);
    else if(!strcmp(line,"CONFLICT")){int policy=!strcmp(argument,"SKIP")?1:!strcmp(argument,"REPLACE")?2:0;bs_conflict_policy(client,policy);}
    else if(!strcmp(line,"HELP")) help();
    else if(!strcmp(line,"LIST")) peers();
    else if(!strcmp(line,"REFRESH")) bs_refresh(client);
    else if(!strcmp(line,"STATUS")) status();
    else if(!strcmp(line,"DECLINE")) bs_decline(client);
    else if(!strcmp(line,"CANCEL")) bs_cancel(client);
    else if(!strcmp(line,"ACCEPTALL")) ok=bs_accept_batch(client,argument);
    else if(!strcmp(line,"ACCEPT")) ok=bs_accept_conflict(client,argument);
    else if(!strcmp(line,"SEND") || !strcmp(line,"SENDIP")) {
        if(!prepare_single_send(client)){puts("A transfer is already in progress.");return 1;}
        path=strchr(argument,' '); if(!path) puts("Specify a recipient and an 8.3 file path.");
        else { *path++=0; while(*path==' ') path++;
            if(!strcmp(line,"SEND")) { char *end; long n=strtol(argument,&end,10); if(*end || n<1 || n>bs_peer_count(client)) puts("Invalid peer number; use LIST."); else { char id[33]; strcpy(id,bs_peer(client,(int)n-1)->id); ok=bs_queue_peer(client,id,path); } }
            else { char *porttext=strchr(argument,':'); unsigned long port=BS_PORT; if(porttext) { char *end; *porttext++=0;port=strtoul(porttext,&end,10); if(*end || port<1 || port>65535) { puts("Invalid port."); return 1; } } ok=bs_queue_address(client,argument,(unsigned short)port,path); }
        }
    } else if(!strcmp(line,"LIMITS")){char *space=strchr(argument,' ');unsigned long up,down;if(!space){puts("LIMITS <send KiB/s> <receive KiB/s>; 0 = Unlimited");return 1;}*space++=0;if(!bs_limit_parse(argument,&up) || !bs_limit_parse(space,&down)){puts("Use 0..1048575 KiB/s for each limit.");return 1;}send_limit=up;receive_limit=down;bs_limits_configure(client,up,down);if(!save_config())puts("Could not save limits.");
} else if(!strcmp(line,"NAME")) { if(!*argument || strlen(argument)>BS_NAME_MAX) puts("Use a name of 1 to 63 ASCII characters."); else { strcpy(config.name,argument); bs_configure(client,config.name,config.visible,config.receiving); if(!save_config()) puts("Could not save BITSKIM.CFG in the launch directory."); } }
    else if(!strcmp(line,"VISIBLE") || !strcmp(line,"RECEIVE")) {
        strupr(argument); if(strcmp(argument,"ON") && strcmp(argument,"OFF")) puts("Use ON or OFF.");
        else { if(!strcmp(line,"VISIBLE")) config.visible=!strcmp(argument,"ON"); else config.receiving=!strcmp(argument,"ON"); bs_configure(client,config.name,config.visible,config.receiving); if(!save_config()) puts("Could not save BITSKIM.CFG in the launch directory."); }
    } else if(*line) puts("Unknown command. Type HELP.");
    if(!ok) printf("Cannot do that: %s\n",bs_last_error(client)); return 1;
}
static int selftest(void) {
    FILE *f; unsigned long size; int failures=0;
    if(bsp_size("BSCHK1.TMP",&size) || bsp_size("BSCHK2.TMP",&size) || bsp_size("BSCHK3.TMP",&size)) { puts("Run /CHECK in an empty scratch directory."); return 1; }
    if(bs_crc32(0,(const unsigned char *)"123456789",9)!=0xcbf43926UL) failures++;
    f=bsp_create_file("BSCHK1.TMP"); if(!f) return 1;
    fputs("one",f); fclose(f);
    f=bsp_create_file("BSCHK1.TMP"); if(f) { fclose(f); failures++; }
    f=bsp_create_file("BSCHK2.TMP"); if(!f) return 1; fputs("two",f); fclose(f);
    if(bsp_commit("BSCHK1.TMP","BSCHK2.TMP")==0) failures++;
    if(!bsp_size("BSCHK1.TMP",&size) || size!=3) failures++;
    if(bsp_commit("BSCHK1.TMP","BSCHK3.TMP")) failures++;
    f=bsp_create_file("CON.TXT"); if(f) { fclose(f); failures++; }
    f=bsp_create_file("TOOLONGNAME.TXT"); if(f) { fclose(f); failures++; }
    bsp_remove_file("BSCHK1.TMP");bsp_remove_file("BSCHK2.TMP");bsp_remove_file("BSCHK3.TMP");
    printf("DOS self-check: %s (CRC, exclusive creation, no-replace rename, 8.3/device names)\n",failures?"FAIL":"PASS"); return failures?1:0;
}
#include "tui.h"
int main(int argc,char **argv) {
    unsigned long last=0; int running=1,batch_send=0,batch_receive=0,batch_started=0,cli=0; clock_t ui_last=0;
    if(argc==2 && !stricmp(argv[1],"/CLI")) cli=1;
    if(argc==2 && !stricmp(argv[1],"/CHECK")) return selftest();
    if(argc==4 && !stricmp(argv[1],"/SEND")) batch_send=1;
    if(argc==3 && !stricmp(argv[1],"/RECEIVE")) batch_receive=1;
    if(argc>1 && !batch_send && !batch_receive && !cli) { help(); puts("Setup: packet driver + SET MTCPCFG=<mTCP config> + valid IP lease."); return 0; }
    if(!bs_platform_init()) { puts("Cannot start mTCP. Check packet driver and MTCPCFG (run DHCP first)."); return 1; }
    if(!_fullpath(settings_path,"BITSKIM.CFG",sizeof settings_path)) strcpy(settings_path,"BITSKIM.CFG");
    if(!read_config()) {bs_platform_cleanup();return 1;} client=bs_create(&config,event,NULL);
    if(!client) { puts("Cannot create BitSkim client (memory, config or port error)."); bs_platform_cleanup(); return 1; }
    {char store[sizeof settings_path],*leaf;strcpy(store,settings_path);leaf=strrchr(store,'\\');if(leaf)strcpy(leaf+1,"BSFOLD.DAT");else strcpy(store,"BSFOLD.DAT");bs_folders_configure(client,store);}
    if(!batch_receive)bs_batch_enable(client,1);
    if(batch_send) { char cmd[192];
        if(strlen(argv[2])+strlen(argv[3])+9>=sizeof cmd) { bs_destroy(client); bs_platform_cleanup(); return 1; }
        sprintf(cmd,"SENDIP %s %s",argv[2],argv[3]); command(cmd); bs_tick(client);if(bs_completion_notice(client,&bell_serial) && completion_bell)putch(7); batch_started=bs_status(client)->active;
        if(!batch_started) { bs_destroy(client); bs_platform_cleanup(); return 1; }
    }
    bs_limits_configure(client,send_limit,receive_limit);
    if(!save_config()) puts("Cannot save BITSKIM.CFG in the launch directory.");
    if(!batch_send && !batch_receive && !cli) ui_start();
    else {help(); printf("Visible as %s. Type HELP for commands.\n> ",config.name);}
    while(running && !bs_dos_break()) {
        bs_dos_pump(); bs_tick(client);if(bs_completion_notice(client,&bell_serial) && completion_bell)putch(7);
        if(ui_active) {
            ui_offer_update();
            if(files_changed) {ui_rescan=1;files_changed=0;}
            if(ui_dirty || (redraw && clock()-ui_last>=CLOCKS_PER_SEC/4)) {ui_draw();ui_last=clock();redraw=0;}
        }
        if(!ui_active && redraw && bsp_seconds()!=last) { status(); last=bsp_seconds(); redraw=0; printf("> %s",input); }
        if(batch_receive && bs_status(client)->active) batch_started=1;
        if(batch_receive && bs_status(client)->awaiting_decision) {
            if(!bs_accept(client,argv[2])) { printf("Cannot save: %s\n",bs_last_error(client)); bs_decline(client); }
        }
        if((batch_send || batch_receive) && batch_started && !bs_status(client)->active) break;
        if(kbhit()) { int ch=getch(); if(!ch || ch==224) ch=0x100+getch(); if(ch==3) break;
            if(ui_active) {running=ui_key(ch);continue;}
            if(ch>=0x100) continue;
            if(ch==13) { putch('\r');putch('\n'); input[used]=0; running=command(input); used=0;input[0]=0;printf("> "); }
            else if(ch==8 && used) { used--;input[used]=0; cputs("\b \b"); }
            else if(ch>=32 && ch<=126 && used+1<sizeof input) { input[used++]=(char)ch;input[used]=0;putch(ch); }
        }
    }
    if(batch_send || batch_receive) { int success=bs_transfer_result(client)==BS_RESULT_COMPLETE; bs_destroy(client); bs_platform_cleanup(); return success?0:1; }
    ui_stop(); bs_cancel(client); bs_destroy(client); bs_platform_cleanup(); puts("\nBitSkim stopped."); return 0;
}
