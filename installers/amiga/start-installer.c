/* Adapted from NovaDock tools/startinstaller.c (MIT, NovaDock contributors).
 * See assets/licenses/NOVADOCK-LICENSE. */
/* Workbench entry point for Install-BitSkim. Run the system Installer as
 * a synchronous CLI tool with an explicit stack and package directory. */
#ifndef BS_INSTALLER_TEST
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <workbench/startup.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#endif
#include <string.h>
#include <stdio.h>
#ifndef BS_INSTALLER_TEST
const char bitskim_installer_version[]="$VER: StartInstaller 0.2 (02.11.2026)";
long __stack=32768;
struct IntuitionBase *IntuitionBase=NULL;
static void problem(const char *text)
{
    struct EasyStruct request;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",37);
    if (!IntuitionBase) return;
    memset(&request,0,sizeof(request));
    request.es_StructSize=sizeof(request);
    request.es_Title=(STRPTR)"BitSkim Installer";
    request.es_TextFormat=(STRPTR)text;
    request.es_GadgetFormat=(STRPTR)"OK";
    EasyRequestArgs(NULL,&request,NULL,NULL);
    CloseLibrary((struct Library *)IntuitionBase);
    IntuitionBase=NULL;
}
 #endif
/* AmigaDOS quotes use '*' as the escape character, including inside quotes. */
static int quoted(char *out,int size,const char *in)
{
    int at=0;
    if (size<3) return 0;
    out[at++]='"';
    while (*in) {
        unsigned char c=(unsigned char)*in++;
        if (c<32 || at+4>=size) return 0;
        if (c=='*' || c=='"') out[at++]='*';
        out[at++]=(char)c;
    }
    out[at++]='"';out[at]=0;
    return 1;
}
static int installer_command(char *out,int cap,const char *installer,const char *script)
{
    char tool[1030],file[1030]; size_t length=strlen(script);
    const char *options=" APPNAME BitSkim MINUSER NOVICE DEFUSER NOVICE";
    if(!length || script[length-1]=='/' || script[length-1]==':') return 0;
    if(!quoted(tool,sizeof tool,installer) || !quoted(file,sizeof file,script)) return 0;
    if(strlen(tool)+strlen(file)+strlen(options)+2>(size_t)cap) return 0;
    sprintf(out,"%s %s%s",tool,file,options); return 1;
}
#ifndef BS_INSTALLER_TEST
static int exists(const char *path)
{
    BPTR lock=Lock((STRPTR)path,ACCESS_READ);
    if (!lock) return 0;
    UnLock(lock);return 1;
}
static int regular_file(const char *path)
{
    struct FileInfoBlock *info; BPTR lock; int ok=0;
    lock=Lock((STRPTR)path,ACCESS_READ); if(!lock) return 0;
    info=(struct FileInfoBlock *)AllocDosObject(DOS_FIB,TAG_DONE);
    if(info) { ok=Examine(lock,info) && info->fib_DirEntryType<0; FreeDosObject(DOS_FIB,info); }
    UnLock(lock); return ok;
}
int main(int argc,char **argv)
{
    char script[512],directory[512],command[2200];
    const char *installer; BPTR lock,input,output,previous,restored;
    LONG result; int ok=0;
    script[0]=0;
    /* Workbench project arguments can identify a drawer rather than the
     * script. Always resolve our known script beside the actual launcher. */
    if(argc<=1) {
        ok=NameFromLock(GetProgramDir(),script,sizeof script) &&
           AddPart(script,(STRPTR)"Install-BitSkim",sizeof script);
    } else {
        lock=Lock((STRPTR)argv[1],ACCESS_READ);
        if(lock) { ok=NameFromLock(lock,script,sizeof script); UnLock(lock); }
        if(ok && !regular_file(script)) ok=AddPart(script,(STRPTR)"Install-BitSkim",sizeof script);
    }
    if(!ok || !regular_file(script)) {problem("Cannot locate the Install-BitSkim script beside StartInstaller. Keep the unpacked package together.");return 20;}
    /* AmigaOS 3.2 keeps its supported Installer in SYS:System. C: may
     * contain an older third-party copy installed years earlier. */
    if (exists("SYS:System/Installer")) installer="SYS:System/Installer";
    else if (exists("C:Installer")) installer="C:Installer";
    else {problem("Installer was not found in SYS:System or C:.");return 20;}
    /* One explicit command: no CD/Stack shell script that can leave a
     * directory in the Installer's script argument. */
    if(!installer_command(command,sizeof command,installer,script)) return 20;
    input=Open("NIL:",MODE_OLDFILE);output=Open("NIL:",MODE_NEWFILE);
    if (!input || !output) {
        if (input) Close(input);
        if (output) Close(output);
        problem("Cannot open the installer command streams.");return 20;
    }
    strcpy(directory,script); *FilePart(directory)=0;
    lock=Lock((STRPTR)directory,ACCESS_READ);
    if(!lock) { Close(input); Close(output); problem("Cannot open the distribution drawer.");return 20; }
    previous=CurrentDir(lock);
    result=SystemTags((STRPTR)command,SYS_Input,input,SYS_Output,output,
                      SYS_Asynch,FALSE,NP_StackSize,65536UL,TAG_DONE);
    restored=CurrentDir(previous); if(restored) UnLock(restored);
    Close(input);Close(output);
    if (result<0) {problem("The system Installer could not be started.");return 20;}
    return (int)result;
}

#endif
