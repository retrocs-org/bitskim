/* Optional versioned metadata. Sidecars bind to payload size and CRC32.
   Native attribute domains are never translated into another OS's permissions. */
void bs_metadata_enable(BSClient *c,int enabled){if(c && !c->status.active){c->metadata_enabled=enabled!=0;bs_refresh(c);}}
int bs_metadata_enabled(const BSClient *c){return c?c->metadata_enabled:0;}
static int metadata_valid(const unsigned char *p,int n){int i,len;if(n!=184 || memcmp(p,"BSM1",4) || p[4]<1 || p[4]>2 || p[5] || p[6] || p[7] || get32(p+8)>255UL || get32(p+12)>4095UL)return 0;
    if(p[4]==1 && get32(p+12))return 0;
    if(p[4]==2 && (get32(p+8) || p[24]))return 0;
    for(len=0;len<160 && p[24+len];len++){if(p[24+len]<32 || p[24+len]==127)return 0;}if(len==160)return 0;
    for(i=24+len;i<184;i++)if(p[i])return 0;
    return valid_utf8(p+24);
}
static int metadata_native(const char *path,BSMetadata *metadata,int apply){
#if defined(__amigaos__) || (defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__))
    return apply?bsp_metadata_apply(path,metadata):bsp_metadata_get(path,metadata);
#else
    (void)path;(void)metadata;(void)apply;return 0;
#endif
}
static int metadata_name(const char *path,char out[BS_PATH_MAX+1]){if(strlen(path)>BS_PATH_MAX-9)return 0;
    strcpy(out,path);strcat(out,".bskmeta");return 1;}
static void metadata_encode(const BSMetadata *m,unsigned long crc,unsigned long size,unsigned char p[184]){memset(p,0,184);memcpy(p,"BSM1",4);p[4]=m->kind;put32(p+8,m->protection);put32(p+12,m->filetype);put32(p+16,crc);put32(p+20,size);copystr((char *)p+24,m->comment,160);}
static int metadata_write(const char *path,const unsigned char p[184]){char sidecar[BS_PATH_MAX+1];FILE *f;int okay;if(!metadata_name(path,sidecar) || !(f=bsp_create_file(sidecar)))return 0;
    okay=fwrite(p,1,184,f)==184;if(fclose(f))okay=0;if(!okay)bsp_remove_file(sidecar);return okay;}
int bs_metadata_seed(const char *path,const BSMetadata *metadata){FILE *f;unsigned char data[BS_CHUNK],wire[184];unsigned long crc=0,size=0,expected;size_t n;int okay;
    if(!path || !metadata || bsp_path_kind(path)!=1 || !bsp_size(path,&expected) || expected>BS_FILE_MAX || !(f=bsp_open_file(path,"rb")))return 0;
    while((n=fread(data,1,sizeof data,f))>0){crc=bs_crc32(crc,data,n);size+=(unsigned long)n;if(size>BS_FILE_MAX)break;}okay=!ferror(f) && size==expected;fclose(f);metadata_encode(metadata,crc,size,wire);return okay && metadata_valid(wire,184) && metadata_write(path,wire);
}
static int metadata_source(BSClient *c){BSMetadata m;char sidecar[BS_PATH_MAX+1];FILE *f;int okay;memset(&m,0,sizeof m);
    if(metadata_native(c->source,&m,0)){metadata_encode(&m,c->crc,c->status.total,c->metadata);return metadata_valid(c->metadata,184);}
    if(!metadata_name(c->source,sidecar) || bsp_path_kind(sidecar)!=1 || !(f=bsp_open_file(sidecar,"rb")))return 0;
    okay=fread(c->metadata,1,184,f)==184 && fgetc(f)==EOF && !ferror(f);fclose(f);return okay && metadata_valid(c->metadata,184) && get32(c->metadata+16)==c->crc && get32(c->metadata+20)==c->status.total;
}
static int metadata_finish(BSClient *c){BSMetadata m;if(!c->metadata_received)return 0;
    memset(&m,0,sizeof m);m.kind=c->metadata[4];m.protection=get32(c->metadata+8);m.filetype=get32(c->metadata+12);copystr(m.comment,(const char *)c->metadata+24,sizeof m.comment);
    if(metadata_native(c->dest,&m,1))return 1;
    return metadata_write(c->dest,c->metadata)?2:-1;
}

static int metadata_companion(const char *path){char base[BS_PATH_MAX+1];unsigned char wire[184];size_t n=strlen(path);FILE *f;int okay;if(n<9 || n>BS_PATH_MAX || strcmp(path+n-8,".bskmeta"))return 0;
    copystr(base,path,sizeof base);base[n-8]=0;if(bsp_path_kind(base)!=1 || !(f=bsp_open_file(path,"rb")))return 0;
    okay=fread(wire,1,184,f)==184 && fgetc(f)==EOF && !ferror(f) && metadata_valid(wire,184);fclose(f);return okay;}
