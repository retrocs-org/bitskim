/* Two checksummed slots per direction preserve the previous value on a torn write. */
struct BSFolders {
    char store[BS_PATH_MAX+1],path[2][BS_PATH_MAX+1];
    unsigned long sequence[2];
};
const char *bs_last_folder(const BSClient *c,int incoming) {
    const char *path;
    if(!c || !c->folders)return "";
    path=c->folders->path[incoming!=0];
    return path[0] && bsp_path_kind(path)==2?path:"";
}
int bs_folders_configure(BSClient *c,const char *store) {
    FILE *file;unsigned char record[BS_PATH_MAX+9];int slot;
    if(!c || !store || !*store || strlen(store)>BS_PATH_MAX)return 0;
    if(!c->folders)c->folders=(BSFolders *)calloc(1,sizeof *c->folders);
    if(!c->folders)return 0;
    memset(c->folders,0,sizeof *c->folders);copystr(c->folders->store,store,BS_PATH_MAX+1);
    file=bsp_open_file(store,"rb");if(!file)return 1;
    for(slot=0;slot<4;slot++){
        unsigned long sequence;int direction=slot/2;
        if(fread(record,sizeof record,1,file)!=1)break;
        sequence=get32(record);
        if(!sequence || !memchr(record+8,0,BS_PATH_MAX+1) ||
           get32(record+4)!=bs_crc32(0,record+8,BS_PATH_MAX+1))continue;
        if(sequence>c->folders->sequence[direction]){
            c->folders->sequence[direction]=sequence;
            copystr(c->folders->path[direction],(const char *)record+8,BS_PATH_MAX+1);
        }
    }
    fclose(file);return 1;
}
static void remember_folder(BSClient *c,int incoming,const char *path,int directory) {
    char folder[BS_PATH_MAX+1],*slash,*other;unsigned char record[BS_PATH_MAX+9];
    BSFolders *f=c->folders;FILE *file;int slot;unsigned long sequence;
    if(!f || !path || strlen(path)>BS_PATH_MAX)return;
    incoming=incoming!=0;copystr(folder,path,sizeof folder);
    if(!directory){
        slash=strrchr(folder,'/');
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
        other=strrchr(folder,'\\');if(other && (!slash || other>slash))slash=other;
#else
        other=NULL;(void)other;
#endif
#if defined(__amigaos__) || defined(BS_DOS) || defined(BS_ATARI)
        other=strrchr(folder,':');if(other && (!slash || other>slash))slash=other;
#endif
        if(!slash)return;
        slash[1]=0;
    }
    if(bsp_path_kind(folder)!=2 || !strcmp(folder,f->path[incoming]))return;
    copystr(f->path[incoming],folder,BS_PATH_MAX+1);
    sequence=f->sequence[incoming]+1;if(!sequence)sequence=1;
    memset(record,0,sizeof record);put32(record,sequence);
    copystr((char *)record+8,folder,BS_PATH_MAX+1);put32(record+4,bs_crc32(0,record+8,BS_PATH_MAX+1));
    file=bsp_open_file(f->store,"r+b");if(!file)file=bsp_open_file(f->store,"w+b");if(!file)return;
    slot=incoming*2+(int)(sequence&1);
    if(!fseek(file,(long)slot*(long)sizeof record,SEEK_SET) &&
       fwrite(record,sizeof record,1,file)==1 && !fflush(file))f->sequence[incoming]=sequence;
    fclose(file);
}
