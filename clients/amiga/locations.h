/* Installer-written local companion paths; portable copies need no sidecars. */
static int amiga_read_path(const char *file,char *out,size_t cap) {
    FILE *f; size_t n,i; int extra;
    if(cap<2) return 0;
    out[0]=0; f=fopen(file,"r"); if(!f) return 0;
    if(!fgets(out,cap,f)) { fclose(f); return 0; }
    n=strlen(out);
    if(!strchr(out,'\n')) { extra=fgetc(f); if(extra!=EOF) { fclose(f); out[0]=0; return 0; } }
    fclose(f);
    while(n && (out[n-1]=='\n' || out[n-1]=='\r')) out[--n]=0;
    if(!n) return 0;
    for(i=0;i<n;i++) if((unsigned char)out[i]<32 || (unsigned char)out[i]==127) { out[0]=0; return 0; }
    return 1;
}
static int amiga_quote_path(char *out,size_t cap,const char *path) {
    size_t n=0; unsigned char c;
    if(cap<3) return 0;
    out[n++]='"';
    while((c=(unsigned char)*path++)) {
        if(c<32 || c==127 || n+3>=cap) return 0;
        if(c=='*' || c=='"') out[n++]='*';
        out[n++]=(char)c;
    }
    out[n++]='"'; out[n]=0; return 1;
}
static int amiga_install_path(const char *leaf,char *out,size_t cap) {
    char file[128];
    if(strlen(leaf)>64) return 0;
    sprintf(file,"ENV:BitSkim/%s",leaf);
    if(amiga_read_path(file,out,cap)) return 1;
    sprintf(file,"ENVARC:BitSkim/%s",leaf);
    return amiga_read_path(file,out,cap);
}
#ifndef BS_LOCATIONS_TEST
static int amiga_local_path(char *path,size_t cap,const char *leaf) {
    return NameFromLock(GetProgramDir(),path,cap) && AddPart(path,(STRPTR)leaf,cap);
}
static int amiga_companion_path(char *out,size_t cap,const char *sidecar,const char *leaf,const char *fallback) {
    char file[1024]; BPTR lock;
    /* Installed location is authoritative when present, so stale legacy copies
     * beside the client cannot shadow the separately installed Preferences. */
    if(amiga_local_path(file,sizeof file,sidecar) && amiga_read_path(file,out,cap)) {
        lock=Lock(out,ACCESS_READ); if(lock) { UnLock(lock); return 1; }
    }
    if(amiga_local_path(out,cap,leaf)) {
        lock=Lock(out,ACCESS_READ); if(lock) { UnLock(lock); return 1; }
    }
    if(amiga_install_path(!strcmp(leaf,"BitSkimPrefs")?"prefs.path":"guide.path",out,cap)) {
        lock=Lock(out,ACCESS_READ); if(lock) { UnLock(lock); return 1; }
    }
    if(fallback && strlen(fallback)<cap) {
        strcpy(out,fallback); lock=Lock(out,ACCESS_READ); if(lock) { UnLock(lock); return 1; }
    }
    out[0]=0; return 0;
}
#endif
