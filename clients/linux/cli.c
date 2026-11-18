#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <signal.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static volatile sig_atomic_t interrupted;
static void on_signal(int signal_number){(void)signal_number;interrupted=1;}
static gboolean prompt_line(char *out,size_t cap){struct pollfd input={STDIN_FILENO,POLLIN,0};if(poll(&input,1,0)<=0)return FALSE;if(!fgets(out,(int)cap,stdin)){interrupted=1;return FALSE;}g_strchomp(out);return TRUE;}
static void result(LinuxClient *app,gboolean json){const BSStatus *s=bs_status(app->engine);char *message=linux_display(s->message);
    if(json){printf("{\"result\":%d,\"filename\":",bs_transfer_result(app->engine));linux_json_string(s->filename);printf(",\"message\":");linux_json_string(message);printf(",\"bytes\":%lu,\"encrypted\":%s,\"verified\":%s}\n",s->transferred,bs_transfer_encrypted(app->engine)?"true":"false",bs_transfer_result(app->engine)==BS_RESULT_COMPLETE&&strstr(s->message,"verified")?"true":"false");}
    else printf("%s: %s (%lu/%lu bytes)\n",s->incoming?"Received":"Sent",message,s->transferred,s->total);g_free(message);fflush(stdout);
}
int main(int argc,char **argv){LinuxOptions options={0};gboolean json=FALSE,accept=FALSE,once=FALSE,version=FALSE,unknown_space=FALSE;gint timeout=0;char *peer=NULL,*invitation=NULL,*invite_address=NULL;GError *error=NULL;options.port=BS_PORT;
    GOptionEntry entries[]={
        {"profile",0,0,G_OPTION_ARG_FILENAME,&options.profile,"Separate private profile (absolute path)","DIR"},
        {"port",0,0,G_OPTION_ARG_INT,&options.port,"Local listening port","PORT"},
        {"bind",0,0,G_OPTION_ARG_STRING,&options.bind,"Bind to this local IPv4 address","IP"},
        {"name",0,0,G_OPTION_ARG_STRING,&options.name,"Name advertised to peers","NAME"},
        {"security",0,0,G_OPTION_ARG_STRING,&options.security,"Encryption: off, prefer or require","MODE"},
        {"metadata",0,0,G_OPTION_ARG_NONE,&options.metadata,"Preserve optional retro metadata",NULL},
        {"no-discovery",0,0,G_OPTION_ARG_NONE,&options.no_discovery,"Disable broadcast discovery",NULL},
        {"directory",'d',0,G_OPTION_ARG_FILENAME,&options.destination,"Existing receive directory (default Downloads)","DIR"},
        {"peer",0,0,G_OPTION_ARG_STRING,&peer,"Send to a discovered device ID","ID"},
        {"invitation",0,0,G_OPTION_ARG_STRING,&invitation,"Send using a fresh identity-checked invitation","URI"},
        {"invite-address",0,0,G_OPTION_ARG_STRING,&invite_address,"Display an invitation while receiving","IP"},
        {"accept",0,0,G_OPTION_ARG_NONE,&accept,"Explicitly accept incoming files into --directory",NULL},
        {"allow-unknown-space",0,0,G_OPTION_ARG_NONE,&unknown_space,"Allow accepting when free-space information is unavailable",NULL},
        {"once",0,0,G_OPTION_ARG_NONE,&once,"Exit after one complete incoming offer/collection",NULL},
        {"timeout",0,0,G_OPTION_ARG_INT,&timeout,"Total runtime limit in seconds (0: no limit)","SECONDS"},
        {"json",0,0,G_OPTION_ARG_NONE,&json,"Machine-readable command results",NULL},
        {"version",'v',0,G_OPTION_ARG_NONE,&version,"Show version",NULL},{NULL}
    };
    GOptionContext *context=g_option_context_new("COMMAND [TARGET] [FILES...]\n\nCommands:\n  discover              List nearby endpoints (default: 3 seconds)\n  send IP[:PORT] FILE... Send files/folders; or use --peer/--invitation\n  receive               Receive, prompting before each offer\n  continue              Continue the saved paused outgoing queue\n  queue                 List saved queued paths\n  clear-queue           Discard waiting entries\n  history               Show the last eight saved results\n  clear-history         Clear saved results, keeping received files\n  diagnostics           Print local connection diagnostics\n\nDesktop and CLI must use separate profiles/ports when run together.");g_option_context_add_main_entries(context,entries,NULL);
    if(!g_option_context_parse(context,&argc,&argv,&error)){g_printerr("%s\n",error->message);g_error_free(error);g_option_context_free(context);return 2;}
    if(version){printf("BitSkim %s / Linux CLI\n",BS_VERSION);g_option_context_free(context);return 0;}
    if(argc<2){char *help=g_option_context_get_help(context,TRUE,NULL);g_print("%s",help);g_free(help);g_option_context_free(context);return 2;}
    const char *command=argv[1];gboolean sending=!strcmp(command,"send"),receiving=!strcmp(command,"receive"),continuing=!strcmp(command,"continue"),discovering=!strcmp(command,"discover");
    if(strcmp(command,"queue")&&strcmp(command,"clear-queue")&&strcmp(command,"history")&&strcmp(command,"clear-history")&&strcmp(command,"diagnostics")&&!sending&&!receiving&&!continuing&&!discovering){g_printerr("Unknown command: %s\n",command);return 2;}
    if(timeout<0||((accept||once||invite_address)&&!receiving)||(peer&&invitation)){g_printerr("Invalid option combination\n");return 2;}
    if(receiving&&!accept&&!isatty(STDIN_FILENO)){g_printerr("Receiving needs an interactive terminal, or explicit --accept and --directory\n");return 2;}
    if(receiving&&accept&&!options.destination){g_printerr("--accept requires an explicit --directory\n");return 2;}
    LinuxClient app;if(!linux_open(&app,&options,receiving?1:0,&error)){g_printerr("%s\n",error->message);g_error_free(error);return 2;}
    if(bs_state_error(app.engine)[0])g_printerr("%s\n",bs_state_error(app.engine));
    int exit_code=0;
    if(!strcmp(command,"history")){int count=bs_history_count(app.engine);if(json)putchar('[');for(int i=0;i<count;i++){char text[2048];bs_history_text(app.engine,i,text,sizeof text);if(json){if(i)putchar(',');linux_json_string(text);}else printf("%s\n\n",text);}if(json)puts("]");goto done;}
    if(!strcmp(command,"queue")){if(json)putchar('[');for(int i=0;i<bs_queue_count(app.engine);i++){BSQueueItem item;bs_queue_item(app.engine,i,&item);if(json){if(i)putchar(',');printf("{\"path\":");linux_json_string(item.path);printf(",\"bytes\":%lu,\"paused\":true}",item.total);}else{char *path=linux_display(item.path);printf("%d: %s (%lu bytes; paused)\n",i+1,path,item.total);g_free(path);}}if(json)puts("]");goto done;}
    if(!strcmp(command,"clear-queue")||!strcmp(command,"clear-history")){if(!strcmp(command,"clear-queue"))bs_queue_clear(app.engine);else bs_history_clear(app.engine);if(!bs_state_flush(app.engine)){g_printerr("%s\n",bs_state_error(app.engine));exit_code=1;}goto done;}
    if(!strcmp(command,"diagnostics")){char report[4096];bs_diagnostics(app.engine,report,sizeof report);if(json){printf("{\"diagnostics\":");linux_json_string(report);puts("}");}else puts(report);goto done;}
    if(sending){int first=2;char address[48]="";unsigned short port=BS_PORT;
        if(!peer&&!invitation){if(argc<4||!linux_target(argv[2],address,&port,&error))goto argument_error;first=3;}
        if(argc<=first){g_set_error_literal(&error,G_IO_ERROR,G_IO_ERROR_FAILED,"Specify at least one file or folder");goto argument_error;}
        if(peer){gint64 end=g_get_monotonic_time()+3*G_USEC_PER_SEC;while(!interrupted&&g_get_monotonic_time()<end)bs_wait(app.engine,0,30);}
        GPtrArray *paths=g_ptr_array_new_with_free_func(g_free);for(int i=first;i<argc;i++)g_ptr_array_add(paths,g_canonicalize_filename(argv[i],NULL));
        gboolean okay=linux_enqueue(&app,peer,address,port,invitation,paths,&error);g_ptr_array_unref(paths);if(!okay)goto argument_error;
    }
    if(continuing){if(!bs_queue_count(app.engine)){g_printerr("No saved outgoing queue\n");exit_code=1;goto done;}bs_queue_continue(app.engine);}
    if(receiving){if(!g_file_test(app.destination,G_FILE_TEST_IS_DIR)){g_printerr("Receive directory does not exist: %s\n",app.destination);exit_code=2;goto done;}if(invite_address){char uri[256];if(!bs_invitation_create(app.engine,invite_address,uri,sizeof uri)){g_printerr("Cannot create invitation; enable certificate encryption and use a reachable IPv4 address\n");exit_code=2;goto done;}g_printerr("Connection invitation (expires in five minutes): %s\n",uri);}g_printerr("Listening on TCP %u. %s\n",app.config.port,accept?"Explicit automatic acceptance for this process":"Approve: y; decline: n; trust matching code: trust XXXX-XXXX-XXXX");}
    signal(SIGINT,on_signal);signal(SIGTERM,on_signal);gint64 start=g_get_monotonic_time();unsigned long observed=0,offered=0;gboolean offered_any=FALSE;char last_code[20]="";
    while(!interrupted){bs_wait(app.engine,0,20);bs_service(app.engine,4);const BSStatus *status=bs_status(app.engine);unsigned long serial=bs_transfer_serial(app.engine);
        if(discovering&&g_get_monotonic_time()-start>=(gint64)(timeout?timeout:3)*G_USEC_PER_SEC)break;
        if(!discovering&&timeout&&g_get_monotonic_time()-start>=(gint64)timeout*G_USEC_PER_SEC){g_printerr("Command timed out; saved outgoing work will require Continue\n");exit_code=124;break;}
        if(sending||continuing){char code[20];if(bs_pairing_code(app.engine,code)&&strcmp(last_code,code)){g_strlcpy(last_code,code,sizeof last_code);g_printerr("Verify on both screens: %s%s\n",code,bs_trust_status(app.engine)==1?" (saved identity matches)":"; type trust CODE to remember");}if(isatty(STDIN_FILENO)){char line[128];if(prompt_line(line,sizeof line)&&g_str_has_prefix(line,"trust ")){char *upper=g_ascii_strup(line+6,-1);if(!bs_trust_current(app.engine,g_strstrip(upper)))g_printerr("%s\n",bs_last_error(app.engine));else g_printerr("Device identity remembered\n");g_free(upper);}}}
        if(status->awaiting_decision&&serial!=offered){offered=serial;offered_any=TRUE;if(isatty(STDIN_FILENO))tcflush(STDIN_FILENO,TCIFLUSH);char *name=linux_display(status->filename),*remote=linux_display(status->peer_name);g_printerr("Incoming %s from %s (%lu bytes)\n",name,remote,status->total);g_free(name);g_free(remote);int count=bs_batch_count(app.engine);for(int i=0;i<count;i++){const BSBatchItem *item=bs_batch_item(app.engine,i);if(item){char *label=linux_display(item->filename);g_printerr("  %s (%lu bytes)\n",label,item->total);g_free(label);}}char code[20];if(bs_pairing_code(app.engine,code))g_printerr("%s; verify %s%s\n",bs_transfer_encrypted(app.engine)?"TLS encrypted":"Plaintext",code,bs_trust_status(app.engine)==1?" (saved identity matches)":" (not remembered)");if(accept&&!linux_accept(&app,app.destination,unknown_space,&error)){g_printerr("%s\n",error->message);g_clear_error(&error);bs_decline(app.engine);exit_code=1;}}
        if(status->awaiting_decision&&!accept){char line[128];if(prompt_line(line,sizeof line)){if(!strcmp(line,"y")||!strcmp(line,"yes")){if(!linux_accept(&app,app.destination,unknown_space,&error)){g_printerr("%s\n",error->message);g_clear_error(&error);}}else if(g_str_has_prefix(line,"trust ")){char *code=g_ascii_strup(line+6,-1);if(!bs_trust_current(app.engine,g_strstrip(code)))g_printerr("%s\n",bs_last_error(app.engine));else g_printerr("Device identity remembered; y to accept\n");g_free(code);}else if(!strcmp(line,"n")||!strcmp(line,"no"))bs_decline(app.engine);}}
        if(!status->active&&bs_transfer_result(app.engine)!=BS_RESULT_NONE&&serial!=observed){observed=serial;result(&app,json);if(receiving&&once&&(bs_transfer_result(app.engine)!=BS_RESULT_COMPLETE||bs_batch_count(app.engine)==0||bs_batch_index(app.engine)+1>=bs_batch_count(app.engine))){exit_code=bs_transfer_result(app.engine)==BS_RESULT_COMPLETE?0:1;break;}}
        if((sending||continuing)&&!status->active&&(!bs_queue_count(app.engine)||bs_queue_paused(app.engine))){exit_code=bs_queue_count(app.engine)?1:0;break;}
        if(receiving&&once&&offered_any&&!status->active&&exit_code)break;
    }
    if(interrupted){if(bs_status(app.engine)->active)bs_cancel(app.engine);exit_code=130;}
    if(discovering){int count=bs_peer_count(app.engine);if(json)putchar('[');for(int i=0;i<count;i++){const BSPeer *p=bs_peer(app.engine,i);if(json){if(i)putchar(',');printf("{\"id\":");linux_json_string(p->id);printf(",\"name\":");linux_json_string(p->name);printf(",\"platform\":");linux_json_string(p->platform);printf(",\"address\":");linux_json_string(p->address);printf(",\"port\":%u}",p->port);}else{char *name=linux_display(p->name),*platform=linux_display(p->platform);printf("%s  %s  %s  %s:%u\n",p->id,name,platform,p->address,p->port);g_free(name);g_free(platform);}}if(json)puts("]");}
    goto done;
argument_error:g_printerr("%s\n",error?error->message:"Specify target and files");g_clear_error(&error);exit_code=2;
done:linux_close(&app);g_option_context_free(context);return exit_code;
}
