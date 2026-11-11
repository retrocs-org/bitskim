/* Opt-in resume extension, available only in OpenSSL-enabled modern builds.
   Checkpoints are bound to the offered peer ID/name/size and validated against
   the sender's SHA-256 prefix before any remaining DATA is accepted. */
#ifdef BS_OPENSSL
static int resume_paths(const char *path,char *part,char *meta) {
    if(!path || strlen(path)>BS_PATH_MAX-12)return 0;
    strcpy(part,path);strcat(part,".bsk-part");strcpy(meta,path);strcat(meta,".bsk-resume");return 1;
}
static int resume_header(BSClient *c,unsigned char *out) {
    int n=(int)strlen(c->status.filename);memcpy(out,"BSRSM01",8);memcpy(out+8,c->remote_id,32);put32(out+40,c->status.total);put16(out+44,(unsigned short)n);memcpy(out+46,c->status.filename,(size_t)n);return 46+n;
}
/* Returns absent=0, valid=1, unsafe/mismatched=-1; never changes either path. */
static int resume_existing(BSClient *c,const char *path,FILE **data,unsigned long *offset) {
    char part[BS_PATH_MAX+1],meta[BS_PATH_MAX+1];unsigned char expected[302],actual[302];unsigned long part_size=0,meta_size=0;int n,has_part,has_meta;FILE *f;
    *data=NULL;*offset=0;if(!resume_paths(path,part,meta))return -1;
    has_part=bsp_size(part,&part_size);has_meta=bsp_size(meta,&meta_size);
    if(!has_part && !has_meta) {
        /* Exclusive creation below will still refuse links/directories that
           size() cannot inspect. Never remove such an unknown object. */
        return 0;
    }
    if(!has_part || !has_meta || part_size>c->status.total)return -1;
    n=resume_header(c,expected);if(meta_size!=(unsigned long)n)return -1;
    f=bsp_resume_file(meta,meta_size);if(!f)return -1;
    if(fread(actual,1,(size_t)n,f)!=(size_t)n || fgetc(f)!=EOF || ferror(f) || CRYPTO_memcmp(expected,actual,(size_t)n)){fclose(f);return -1;}fclose(f);
    f=bsp_resume_file(part,part_size);if(!f)return -1;*data=f;*offset=part_size;return 1;
}
int bs_resume_enable(BSClient *c,int enabled) {if(!c || c->status.active || c->queue_count)return 0;c->resume_enabled=enabled!=0;bs_refresh(c);return 1;}
int bs_resume_available(BSClient *c,const char *path) {FILE *f;unsigned long offset;int rc;if(!c || !c->resume_enabled || !c->resume_offer)return 0;rc=resume_existing(c,path,&f,&offset);if(f)fclose(f);return rc;}
int bs_accept_resumable(BSClient *c,const char *path,int restart) {
    char part[BS_PATH_MAX+1],meta[BS_PATH_MAX+1];unsigned char header[302],start[4];FILE *file=NULL,*metadata;unsigned long offset;int rc,n,ok;
    if(!c || c->state!=DECISION || !c->status.incoming || !c->status.awaiting_decision || c->tx_count)return 0;
    if(!c->resume_enabled || !c->resume_offer)return bs_accept(c,path);
    if(!resume_paths(path,part,meta)){copystr(c->error,"Destination path is too long for resume files",256);return 0;}
    rc=resume_existing(c,path,&file,&offset);
    if(rc<0){copystr(c->error,"Resume files are unsafe or belong to a different offer; choose another destination",256);return 0;}
    if(restart && rc==1) {fclose(file);file=NULL;if(bsp_remove_file(part) || bsp_remove_file(meta)){copystr(c->error,"Cannot remove this transfer's checkpoint",256);return 0;}rc=0;offset=0;}
    if(!storage_resume(c,path,offset)){if(file)fclose(file);return 0;}
    if(!rc) {
        file=bsp_create_file(part);if(!file){copystr(c->error,"Cannot exclusively create the resume data file",256);return 0;}
        metadata=bsp_create_file(meta);if(!metadata){fclose(file);bsp_remove_file(part);copystr(c->error,"Cannot exclusively create resume metadata; unknown files were preserved",256);return 0;}
        n=resume_header(c,header);ok=fwrite(header,1,(size_t)n,metadata)==(size_t)n;if(fclose(metadata))ok=0;
        if(!ok){fclose(file);bsp_remove_file(part);bsp_remove_file(meta);copystr(c->error,"Cannot write resume metadata",256);return 0;}
    }
    copystr(c->temp,part,sizeof c->temp);copystr(c->dest,path,sizeof c->dest);copystr(c->resume_meta,meta,sizeof c->resume_meta);c->resume_cache=1;c->file=file;
    c->prefix_hash=EVP_MD_CTX_new();
    if(!c->prefix_hash || EVP_DigestInit_ex(c->prefix_hash,EVP_sha256(),NULL)!=1){finish_error(c,FRAME_ERROR,"Cannot initialize resume validation");return 0;}
    c->resume_offset=c->verify_remaining=offset;c->crc=0;c->status.awaiting_decision=0;c->state=CHECK_RECEIVE;c->last_io=c->last_resume_heartbeat=bsp_seconds();
    put32(start,offset);frame(c,PREPARE_RESUME,start,4);message(c,offset?"Checking retained partial data...":"Preparing resumable receive...");return 1;
}
static int resume_frame(BSClient *c,int type,const unsigned char *p,int n) {
    if(type==RESUME_WAIT && n==0 && (c->state==PREPARE_SEND || c->state==CHECK_SEND || c->state==WAIT_RESUME || c->state==CHECK_RECEIVE))return 1;
    if(type==PREPARE_RESUME && n==4 && c->state==DECISION && !c->status.incoming && c->resume_offer) {
        c->resume_offset=get32(p);if(c->resume_offset>c->status.total){fail(c,"Invalid resume offset");return 1;}
        c->state=PREPARE_SEND;message(c,"Recipient is checking retained partial data...");return 1;
    }
    if(type==ACCEPT_RESUME && n==36 && c->state==PREPARE_SEND && !c->status.incoming && c->resume_offer && get32(p)==c->resume_offset) {
        c->file=bsp_open_file(c->source,"rb");if(!c->file){fail(c,"Source is no longer readable");return 1;}
        c->prefix_hash=EVP_MD_CTX_new();if(!c->prefix_hash || EVP_DigestInit_ex(c->prefix_hash,EVP_sha256(),NULL)!=1){fail(c,"Cannot initialize resume validation");return 1;}
        memcpy(c->prefix_digest,p+4,32);c->verify_remaining=c->resume_offset;c->crc=0;c->state=CHECK_SEND;message(c,"Validating retained prefix against source...");return 1;
    }
    if(type==RESUME_READY && !n && c->state==WAIT_RESUME && c->status.incoming && c->resume_cache) {c->state=RECEIVING;message(c,"Receiving remaining file data...");return 1;}
    return 0;
}
static void resume_tick(BSClient *c,unsigned long now) {
    unsigned char buffer[BS_CHUNK],digest[32],accept[36];unsigned digest_length=0;unsigned long count;int steps;
    if(c->state==CHECK_RECEIVE || c->state==CHECK_SEND) {
        for(steps=0;steps<4 && c->verify_remaining;steps++) {
            count=c->verify_remaining>BS_CHUNK?BS_CHUNK:c->verify_remaining;
            if(fread(buffer,1,(size_t)count,c->file)!=(size_t)count || EVP_DigestUpdate(c->prefix_hash,buffer,(size_t)count)!=1){finish_error(c,FRAME_ERROR,"Cannot validate resume prefix");return;}
            c->crc=bs_crc32(c->crc,buffer,(size_t)count);c->verify_remaining-=count;c->last_io=now;
        }
        if(!c->verify_remaining && !c->tx_count) {
            if(EVP_DigestFinal_ex(c->prefix_hash,digest,&digest_length)!=1 || digest_length!=32){finish_error(c,FRAME_ERROR,"Resume digest failed");return;}
            EVP_MD_CTX_free(c->prefix_hash);c->prefix_hash=NULL;
            if(c->state==CHECK_SEND && CRYPTO_memcmp(digest,c->prefix_digest,32)){finish_error(c,FRAME_ERROR,"Retained partial data does not match the source; restart the transfer");return;}
            if(fseek(c->file,(long)c->resume_offset,SEEK_SET)){finish_error(c,FRAME_ERROR,"Cannot seek to resume position");return;}
            c->status.transferred=c->rate_bytes=c->resume_offset;c->rate_time=now;
            if(c->state==CHECK_RECEIVE){put32(accept,c->resume_offset);memcpy(accept+4,digest,32);frame(c,ACCEPT_RESUME,accept,36);c->state=WAIT_RESUME;message(c,"Waiting for sender to validate resume...");}
            else {frame(c,RESUME_READY,NULL,0);c->state=SENDING;message(c,"Sending remaining file data...");}
        }
    }
}
static void resume_keepalive(BSClient *c,unsigned long now) {
    /* WAIT_RESUME must not emit: a duplex heartbeat could arrive after the
       sender has emitted RESUME_READY and entered SENDING. The hashing sender
       supplies keepalives during this phase; TCP orders them before READY. */
    if((c->state==CHECK_RECEIVE || c->state==CHECK_SEND || c->state==PREPARE_SEND) && !c->tx_count && now-c->last_resume_heartbeat>=1){frame(c,RESUME_WAIT,NULL,0);c->last_resume_heartbeat=now;}
}
#else
int bs_resume_enable(BSClient *c,int enabled) {(void)c;return !enabled;}
int bs_resume_available(BSClient *c,const char *path) {(void)c;(void)path;return 0;}
int bs_accept_resumable(BSClient *c,const char *path,int restart) {(void)restart;return bs_accept(c,path);}
#endif
