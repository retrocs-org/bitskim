/* Included by main.c: local ARexx host, no remote command execution. */
#include "arexx_protocol.h"
static void launch_preferences(void) {
    char path[1024],command[2100]; BPTR input,output;
    if(!amiga_companion_path(path,sizeof path,"BitSkim.prefs-path","BitSkimPrefs","SYS:Prefs/BitSkimPrefs") ||
       !amiga_quote_path(command,sizeof command,path)) {
        alert("Could not locate BitSkimPrefs. Reinstall it or keep a portable copy beside BitSkim."); return;
    }
    input=Open((STRPTR)"NIL:",MODE_OLDFILE); output=Open((STRPTR)"NIL:",MODE_NEWFILE);
    if(!input || !output) { if(input) Close(input); if(output) Close(output); alert("Could not launch BitSkimPrefs."); return; }
    if(SystemTags((STRPTR)command,SYS_Asynch,TRUE,SYS_Input,input,SYS_Output,output,TAG_DONE)==-1) {
        Close(input); Close(output); alert("Could not start BitSkimPrefs from its installed location.");
    }
}
static void rexx_open(void) {
    int i;
    RexxSysBase=(struct RxsLib *)OpenLibrary("rexxsyslib.library",36);
    if(!RexxSysBase) return;
    rexx_port=CreateMsgPort(); if(!rexx_port) return;
    Forbid();
    for(i=0;i<100;i++) {
        if(i) sprintf(rexx_name,"BITSKIM.%ld",(long)i); else strcpy(rexx_name,"BITSKIM");
        if(!FindPort((STRPTR)rexx_name)) break;
    }
    if(i<100) { rexx_port->mp_Node.ln_Name=(STRPTR)rexx_name; AddPort(rexx_port); }
    Permit();
    if(i==100) { DeleteMsgPort(rexx_port); rexx_port=NULL; }
}
static void rexx_reply(struct RexxMsg *m,int rc,const char *result) {
    m->rm_Result1=rc; m->rm_Result2=0;
    if(!rc && (m->rm_Action&RXFF_RESULT)) {
        m->rm_Result2=(LONG)CreateArgstring((STRPTR)result,strlen(result));
        if(!m->rm_Result2) m->rm_Result1=20;
    }
    ReplyMsg((struct Message *)m);
}
static void rexx_poll(void) {
    struct RexxMsg *m; int count=0;
    if(!rexx_port) return;
    while(count++<8 && (m=(struct RexxMsg *)GetMsg(rexx_port))) {
        if(IsRexxMsg(m)) {
            char command[1200],result[BS_REXX_RESULT_MAX]; int rc=10;
            result[0]=0;
            if((m->rm_Action&RXCODEMASK)==RXCOMM && m->rm_Args[0] && strlen((char *)m->rm_Args[0])<sizeof(command)) {
                copy(command,(char *)m->rm_Args[0],sizeof(command)); rc=rexx_command(command,result,sizeof(result));
            }
            rexx_reply(m,rc,result);
        } else ReplyMsg((struct Message *)m);
    }
}
static void rexx_close(void) {
    struct Message *m;
    if(rexx_port) {
        Forbid(); RemPort(rexx_port); Permit();
        while((m=GetMsg(rexx_port))) {
            if(IsRexxMsg((struct RexxMsg *)m)) rexx_reply((struct RexxMsg *)m,20,""); else ReplyMsg(m);
        }
        DeleteMsgPort(rexx_port); rexx_port=NULL;
    }
    if(RexxSysBase) CloseLibrary((struct Library *)RexxSysBase);
    RexxSysBase=NULL;
}
