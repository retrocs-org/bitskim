/* Versioned local checkpoints. Four queue records per tick, constant memory.
   Every restored queue starts paused; no saved state grants receive consent. */
struct BSState {
    char path[BS_PATH_MAX+1],temporary[BS_PATH_MAX+1],backup[BS_PATH_MAX+1],error[128];
    FILE *job;unsigned long revision,saving,crc,next_save;int force;int dirty,index,count,restored;
};
static void state_mark(BSClient *c){if(c && c->saved_state){c->saved_state->dirty=1;c->saved_state->revision++;}}
const char *bs_state_error(const BSClient *c){return c && c->saved_state?c->saved_state->error:"";}
int bs_state_restored(const BSClient *c){return c && c->saved_state && c->queue_paused && c->queue_count?c->saved_state->restored:0;}
static int state_queue_valid(const BSQueued *q){int i;
    if(!memchr(q->id,0,sizeof q->id) || !memchr(q->address,0,sizeof q->address) || !memchr(q->path,0,sizeof q->path) || !memchr(q->relative,0,sizeof q->relative) || !q->path[0] || !q->port || q->total>BS_FILE_MAX || q->kind<0 || q->kind>2)return 0;
    if(q->id[0]){if(strlen(q->id)!=32)return 0;
    for(i=0;i<32;i++)if(hexval(q->id[i])<0)return 0;}
    else if(!q->address[0] || !inet_addr(q->address) || inet_addr(q->address)==INADDR_NONE)return 0;
    return !q->relative[0] || bs_safe_relative(q->relative);
}
static int state_history_valid(const BSHistoryItem *h){return memchr(h->filename,0,sizeof h->filename) && memchr(h->peer,0,sizeof h->peer) && memchr(h->path,0,sizeof h->path) && memchr(h->message,0,sizeof h->message) && memchr(h->peer_id,0,sizeof h->peer_id) && memchr(h->address,0,sizeof h->address) && h->total<=BS_FILE_MAX && h->transferred<=h->total && h->result>BS_RESULT_NONE && h->result<=BS_RESULT_SKIPPED;}
static void state_abort(struct BSState *s){if(s->job){fclose(s->job);s->job=NULL;}if(s->temporary[0])bsp_remove_file(s->temporary);}
static int state_error(struct BSState *s){state_abort(s);copystr(s->error,"Cannot save transfers; check private storage space and permissions",sizeof s->error);return 0;}
typedef struct {BSQueued q,dir;char previous[16][256];unsigned long group;int count;} BSStateLoad;
static int state_append(FILE *queue,BSStateLoad *load,BSQueued *q){
    int depth=0,i,k;size_t j;char *slash;
    if(q->relative[0]){
        for(j=0;q->relative[j];j++)if(q->relative[j]=='/')depth++;
        if(depth>16)return 0;
        if(load->group!=q->group){load->group=q->group;memset(load->previous,0,sizeof load->previous);}
        for(i=0;i<depth-(q->kind==1);i++){
            load->dir=*q;load->dir.kind=1;load->dir.total=load->dir.report=0;
            for(k=0,j=0;load->dir.relative[j];j++)if(load->dir.relative[j]=='/' && k++==i){load->dir.relative[j+1]=0;break;}
            if(!strcmp(load->dir.relative,load->previous[i]))continue;
            for(k=depth-i-(q->kind==1);k>0;k--){slash=strrchr(load->dir.path,'/');if(!slash)return 0;*slash=0;}
            if(load->count>=BS_QUEUE_MAX || fwrite(&load->dir,sizeof load->dir,1,queue)!=1)return 0;
            load->count++;copystr(load->previous[i],load->dir.relative,256);for(k=i+1;k<16;k++)load->previous[k][0]=0;
        }
        if(q->kind==1 && depth>0)copystr(load->previous[depth-1],q->relative,256);
    }else{load->group=0;memset(load->previous,0,sizeof load->previous);}
    if(load->count>=BS_QUEUE_MAX || fwrite(q,sizeof *q,1,queue)!=1)return 0;
    load->count++;return 1;
}
static int state_load(BSClient *c,const char *path){
    unsigned char header[32];unsigned long crc=0,count,hist,i;FILE *f=NULL,*queue=NULL;BSQueued *q=NULL;BSStateLoad *load=NULL;BSHistoryItem *history=NULL;int okay=0;
    f=bsp_open_file(path,"rb");if(!f)return 0;
    if(fread(header,1,sizeof header,f)!=sizeof header || memcmp(header,"BSSTATE2",8) || get32(header+8)!=sizeof(BSQueued) || get32(header+12)!=sizeof(BSHistoryItem) || get32(header+28)!=sizeof(unsigned long))goto done;
    count=get32(header+16);hist=get32(header+20);if(count>BS_QUEUE_MAX || hist>BS_HISTORY_MAX)goto done;
    load=(BSStateLoad *)calloc(1,sizeof *load);q=load?&load->q:NULL;history=(BSHistoryItem *)calloc(BS_HISTORY_MAX,sizeof *history);if(!q || !history)goto done;
    if(hist && fread(history,sizeof *history,(size_t)hist,f)!=hist)goto done;
    crc=bs_crc32(crc,(const unsigned char *)history,(size_t)hist*sizeof *history);
    for(i=0;i<hist;i++)if(!state_history_valid(&history[i]))goto done;
    if(count && !(queue=bsp_temp_file()))goto done;
    for(i=0;i<count;i++){
        if(fread(q,sizeof *q,1,f)!=1 || !state_queue_valid(q))goto done;
        crc=bs_crc32(crc,(const unsigned char *)q,sizeof *q);q->report=0;
        if(!state_append(queue,load,q))goto done;
        if(q->group>c->tree_counter)c->tree_counter=q->group;
    }
    if(crc!=get32(header+24) || fgetc(f)!=EOF || ferror(f) || (queue && fflush(queue)))goto done;
    c->queue_file=queue;queue=NULL;c->queue_head=0;c->queue_count=load->count;c->queue_paused=count!=0;
    /* History was stored newest first; rebuild the native ring oldest first. */
    for(i=hist;i>0;i--){c->history[c->history_next]=history[i-1];c->history_next=(c->history_next+1)%BS_HISTORY_MAX;c->history_count++;if(history[i-1].serial>c->transfer_serial)c->transfer_serial=history[i-1].serial;}
    c->history_serial=c->transfer_serial;okay=1;
done:if(f)fclose(f);if(queue)fclose(queue);free(load);free(history);return okay;
}
int bs_state_configure(BSClient *c,const char *path){struct BSState *s;int kind;
    if(!c || c->saved_state || c->status.active || c->queue_count || c->history_count || !path || !*path || strlen(path)>BS_PATH_MAX-8)return 0;
    s=(struct BSState *)calloc(1,sizeof *s);if(!s)return 0;
    copystr(s->path,path,sizeof s->path);strcpy(s->temporary,path);strcat(s->temporary,".new");strcpy(s->backup,path);strcat(s->backup,".old");c->saved_state=s;
    kind=bsp_path_kind(path);
    if(kind==1){if(!state_load(c,path)){copystr(s->error,"Saved transfers are invalid or from another build; original retained",sizeof s->error);return 0;}bsp_remove_file(s->backup);}
    else if(kind==0 && bsp_path_kind(s->backup)==1){if(!state_load(c,s->backup)){copystr(s->error,"Saved transfer recovery failed; original retained",sizeof s->error);return 0;}}
    else if(kind!=0){copystr(s->error,"Saved transfer path is unsafe or unavailable",sizeof s->error);return 0;}
    if(bsp_path_kind(s->temporary)==1)bsp_remove_file(s->temporary);
    s->restored=c->queue_count;if(s->restored)copystr(c->status.message,"Saved outgoing transfers restored paused; review and Continue to send",sizeof c->status.message);
    return 1;
}
static void state_tick(BSClient *c){struct BSState *s;unsigned char header[32];BSQueued q;BSHistoryItem history;int i,budget=4;
    if(!c || !(s=c->saved_state) || !s->dirty || s->error[0])return;
    if(!s->force && !s->job && (long)(bsp_millis()-s->next_save)<0)return;
    if(s->job && s->saving!=s->revision)state_abort(s);
    if(!s->job){
        s->job=bsp_create_file(s->temporary);if(!s->job){state_error(s);return;}s->saving=s->revision;s->index=0;s->count=c->queue_count;s->crc=0;
        memset(header,0,sizeof header);memcpy(header,"BSSTATE2",8);put32(header+8,sizeof q);put32(header+12,sizeof history);put32(header+16,(unsigned long)s->count);put32(header+20,(unsigned long)c->history_count);put32(header+28,sizeof(unsigned long));
        if(fwrite(header,1,sizeof header,s->job)!=sizeof header){state_error(s);return;}
        for(i=0;i<c->history_count;i++){if(!bs_history_item(c,i,&history) || fwrite(&history,sizeof history,1,s->job)!=1){state_error(s);return;}s->crc=bs_crc32(s->crc,(const unsigned char *)&history,sizeof history);}
    }
    while(budget-- && s->index<s->count){if(!queue_read(c,s->index++,&q) || fwrite(&q,sizeof q,1,s->job)!=1){state_error(s);return;}s->crc=bs_crc32(s->crc,(const unsigned char *)&q,sizeof q);}
    if(s->index<s->count)return;
    put32(header,s->crc);
    if(fseek(s->job,24,SEEK_SET) || fwrite(header,1,4,s->job)!=4 || fflush(s->job)){state_error(s);return;}
    if(fclose(s->job)){s->job=NULL;state_error(s);return;}s->job=NULL;
    if(bsp_path_kind(s->path)==1 && bsp_commit(s->path,s->backup)){state_error(s);return;}
    if(bsp_commit(s->temporary,s->path)){if(bsp_path_kind(s->backup)==1)bsp_commit(s->backup,s->path);state_error(s);return;}
    bsp_remove_file(s->backup);s->dirty=0;s->next_save=bsp_millis()+2000UL;
}
int bs_state_flush(BSClient *c){struct BSState *s;int budget=BS_QUEUE_MAX+8;if(!c || !(s=c->saved_state))return 0;
    s->force=1;while(s->dirty && !s->error[0] && budget--)state_tick(c);
    s->force=0;return !s->dirty && !s->error[0];}
static void state_close(BSClient *c){struct BSState *s;if(!(s=c->saved_state))return;
    if(c->status.active && c->status.filename[0]){c->transfer_result=BS_RESULT_FAILED;copystr(c->status.message,"Interrupted on exit; saved outgoing work requires explicit retry",sizeof c->status.message);history_record(c);}
    bs_state_flush(c);state_abort(s);free(s);c->saved_state=NULL;
}
