/* Open the installed local manual without blocking network/ARexx servicing. */
static void launch_help(void) {
    char path[1024],command[2100]; unsigned int i,n; BPTR lock,input,output;
    if(!amiga_companion_path(path,sizeof path,"BitSkimPrefs.guide-path","BitSkim.guide","HELP:BitSkim.guide")) {
        alert("Cannot locate BitSkim.guide. Reinstall or keep the manual beside this program."); return;
    }
    lock=Lock("SYS:Utilities/MultiView",ACCESS_READ);
    if(lock) { UnLock(lock); strcpy(command,"SYS:Utilities/MultiView \""); }
    else {
        lock=Lock("C:MultiView",ACCESS_READ);
        if(!lock) { alert("MultiView is required to open the manual."); return; }
        UnLock(lock); strcpy(command,"C:MultiView \"");
    }
    n=strlen(command);
    for(i=0;path[i];i++) {
        if((unsigned char)path[i]<32 || (unsigned char)path[i]==127) { alert("Invalid manual path."); return; }
        if(path[i]=='*' || path[i]=='\"') command[n++]='*';
        command[n++]=path[i];
    }
    command[n++]='\"'; command[n]=0;
    input=Open("NIL:",MODE_OLDFILE); output=Open("NIL:",MODE_NEWFILE);
    if(!input || !output) { if(input) Close(input); if(output) Close(output); alert("Cannot open manual viewer streams."); return; }
    if(SystemTags(command,SYS_Asynch,TRUE,SYS_Input,input,SYS_Output,output,NP_StackSize,32768,TAG_DONE)==-1) {
        Close(input); Close(output); alert("Could not start SYS:Utilities/MultiView.");
    }
}
