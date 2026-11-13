/* Small WBStartup entry: launch the installed main program, never a stale copy. */
#ifndef BS_STARTUP_TEST
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#endif
#include <stdio.h>
#include <string.h>
#define BS_LOCATIONS_TEST
#include "../../clients/amiga/locations.h"
long __stack=32768;
const char bitskim_start_version[]="$VER: BitSkimStart 0.2 (02.11.2026)";
int main(void) {
    char path[1024],command[2100]; BPTR lock,input,output; int running;
    Forbid(); running=FindPort((STRPTR)"BITSKIM")!=NULL; Permit();
    if(running) return 0;
    if(!amiga_install_path("app.path",path,sizeof path) || !amiga_quote_path(command,sizeof command,path)) return 20;
    lock=Lock(path,ACCESS_READ); if(!lock) return 20; UnLock(lock);
    input=Open("NIL:",MODE_OLDFILE); output=Open("NIL:",MODE_NEWFILE);
    if(!input || !output) { if(input) Close(input); if(output) Close(output); return 20; }
    if(SystemTags((STRPTR)command,SYS_Asynch,TRUE,SYS_Input,input,SYS_Output,output,NP_StackSize,32768UL,TAG_DONE)==-1) {
        Close(input); Close(output); return 20;
    }
    return 0;
}
