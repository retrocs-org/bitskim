/* File lists live in temporary storage. Returned item pointers are valid until
   the next item lookup on this client; copy before requesting another entry. */
void bs_batch_enable(BSClient *c,int enabled){if(c){c->batch_enabled=enabled!=0;if(!enabled)batch_revoke(c);bs_refresh(c);}}
int bs_batch_count(const BSClient *c){return c && c->status.incoming && c->in_batch?c->in_batch->count:0;}
int bs_batch_index(const BSClient *c){return bs_batch_count(c)?c->in_batch_index:0;}
const BSBatchItem *bs_batch_item(const BSClient *c,int i){return i>=0 && i<bs_batch_count(c)?batch_item(c->in_batch,i):NULL;}
int bs_suggest_name(const char *wire,int index,char *out,size_t capacity) {
    char name[256];(void)index;
    if(!wire || !bs_safe_name(wire) || !out || !capacity)return 0;
    if(!bs_to_local(wire,name,sizeof name)) {
        {const unsigned char *p=(const unsigned char *)wire;int i=0;
         while(*p && i<255){if(*p<128)name[i++]=(char)*p++;else {name[i++]='_';p++;while((*p&0xc0)==0x80)p++;}}name[i]=0;}
        if(!name[0])sprintf(name,"received-%d",index+1);
    }
#ifdef _WIN32
    {char stem[9];const char *dot=strchr(name,'.');size_t n=dot?(size_t)(dot-name):strlen(name);int j;
     if(n<sizeof stem){for(j=0;j<(int)n;j++)stem[j]=(char)toupper((unsigned char)name[j]);stem[n]=0;
      if(!strcmp(stem,"CON") || !strcmp(stem,"PRN") || !strcmp(stem,"AUX") || !strcmp(stem,"NUL") || !strcmp(stem,"CLOCK$") || !strcmp(stem,"CONIN$") || !strcmp(stem,"CONOUT$") || (n==4 && (!strncmp(stem,"COM",3) || !strncmp(stem,"LPT",3)) && stem[3]>='1' && stem[3]<='9')){size_t len=strlen(name);if(len<255)memmove(name+1,name,len+1);name[0]='_';}}}
#endif
#ifdef __amigaos__
    if(strlen(name)>30) {
        char *dot=strrchr(name,'.');size_t extension=dot?strlen(dot):0;
        if(extension>0 && extension<=12 && dot!=name)memmove(name+30-extension,dot,extension+1);
        else name[30]=0;
    }
#endif
#ifdef __riscos__
    if(strlen(name)==1 && strchr("^$@%&",name[0])){name[2]=0;name[1]=name[0];name[0]='_';}
    if(strlen(name)>10){char *dot=strrchr(name,'.');size_t ext=dot?strlen(dot):0;if(dot!=name && ext>0 && ext<=4)memmove(name+10-ext,dot,ext+1);else name[10]=0;}
#endif
#if defined(BS_DOS) || defined(BS_ATARI_STING)
    {char leaf[13],stem[9],extension[4];const char *dot=strrchr(name,'.');int i=0,j=0;
     while(name[i] && (!dot || name+i<dot) && j<8) {unsigned char ch=(unsigned char)name[i++];stem[j++]=(char)(isalnum(ch) && ch<128?toupper(ch):'_');}stem[j]=0;
     if(!j || !strcmp(stem,"CON") || !strcmp(stem,"PRN") || !strcmp(stem,"AUX") || !strcmp(stem,"NUL") ||
         (j==4 && (!strncmp(stem,"COM",3) || !strncmp(stem,"LPT",3)) && stem[3]>='1' && stem[3]<='9'))sprintf(stem,"F%07d",index+1);
     j=0;if(dot)for(i=1;dot[i] && j<3;i++){unsigned char ch=(unsigned char)dot[i];extension[j++]=(char)(isalnum(ch) && ch<128?toupper(ch):'_');}extension[j]=0;
     if(j)sprintf(leaf,"%s.%s",stem,extension);else strcpy(leaf,stem);strcpy(name,leaf);}
#endif
    if(strlen(name)>=capacity)return 0;
    copystr(out,name,capacity);return 1;
}
int bs_batch_save_name(const BSClient *c,int index,char *out,size_t capacity) {
    const BSBatchItem *item=bs_batch_item(c,index);char wire[256],mapped[256],*part,*slash;size_t used=0;
    if(!item || !out || !capacity)return 0;
    if(!c || !c->in_batch->tree)return bs_suggest_name(item->filename,index,out,capacity);
    copystr(wire,item->filename,sizeof wire);out[0]=0;part=wire;
    while(*part){slash=strchr(part,'/');if(slash)*slash=0;if(!bs_suggest_name(part,index,mapped,sizeof mapped) || used+strlen(mapped)+(slash?1:0)>=capacity)return 0;strcat(out,mapped);used=strlen(out);if(!slash)break;strcat(out,"/");used++;part=slash+1;}return 1;
}
int bs_suggest_destination(const char *folder,const char *wire,int ordinal,char *path,size_t capacity){
    char name[256],candidate[256],suffix[24],destination[BS_PATH_MAX+1];size_t length,prefix,limit;int n,kind;
    if(!folder || !*folder || !wire || !path || strlen(folder)>BS_PATH_MAX-300 || !bs_suggest_name(wire,ordinal,name,sizeof name))return 0;
    for(n=0;n<10000;n++){
        copystr(candidate,name,sizeof candidate);
        if(n){sprintf(suffix,".%d",n);limit=255;
#ifdef __amigaos__
            limit=30;
#endif
#ifdef __riscos__
            limit=10;
#endif
            {const char *dot=strrchr(name,'.');size_t ext=dot && dot!=name?strlen(dot):0;
             /* Leave at least one stem byte; never subtract an oversized extension. */
             if(ext>12 || strlen(suffix)+ext>=limit)ext=0;
             length=strlen(name)-ext;
             if(length+strlen(suffix)+ext>limit)length=limit-strlen(suffix)-ext;
#if !defined(__amigaos__) && !defined(__riscos__)
                while(length && (((unsigned char)candidate[length]&0xc0)==0x80))length--;
#endif
             candidate[length]=0;strcat(candidate,suffix);if(ext)strcat(candidate,dot);}
#if defined(BS_DOS) || defined(BS_ATARI_STING)
            {const char *dot=strrchr(name,'.');size_t base=dot?(size_t)(dot-name):strlen(name);char digits[8];sprintf(digits,"~%d",n);if(base+strlen(digits)>8)base=8-strlen(digits);memcpy(candidate,name,base);candidate[base]=0;strcat(candidate,digits);if(dot)strcat(candidate,dot);}
#endif
        }
        copystr(destination,folder,sizeof destination);prefix=strlen(destination);if(destination[prefix-1]!='/' && destination[prefix-1]!='\\' && destination[prefix-1]!=':')strcat(destination,"/");strcat(destination,candidate);
        kind=bsp_path_kind(destination);if(kind==0){if(strlen(destination)>=capacity)return 0;copystr(path,destination,capacity);return 1;}if(kind<0)return 0;
    }return 0;
}
#include "tree.h"
static int batch_destination(BSClient *c,const char *folder,char *path){
    const BSBatchItem *item=bs_batch_item(c,c->in_batch_index);
    if(item && c->conflict_policy!=BS_CONFLICT_RENAME){char name[256];size_t n=strlen(folder);if(!n || n>BS_PATH_MAX-300 || !bs_suggest_name(item->filename,c->in_batch_index,name,sizeof name))return 0;copystr(path,folder,BS_PATH_MAX+1);if(path[n-1]!='/' && path[n-1]!='\\' && path[n-1]!=':')strcat(path,"/");strcat(path,name);return 1;}
    if(!item || !bs_suggest_destination(folder,item->filename,c->in_batch_index,path,BS_PATH_MAX+1)){copystr(c->error,"Cannot choose an unused destination filename",256);return 0;}return 1;
}
int bs_accept_batch(BSClient *c,const char *folder){BSBatchApproval *a;char path[BS_PATH_MAX+1];
    if(!c || !bs_batch_count(c) || !c->status.awaiting_decision || c->state!=DECISION || c->tx_count)return 0;
    if(!folder || !*folder || strlen(folder)>BS_PATH_MAX-300){copystr(c->error,"Choose a shorter destination folder",256);return 0;}
    if(!storage_check(c,folder,1))return 0;
    a=(BSBatchApproval *)calloc(1,sizeof *a);if(!a){copystr(c->error,"Not enough memory for approval",256);return 0;}
    a->manifest=c->in_batch;a->manifest->refs++;a->next=c->in_batch_index;a->port=c->remote_port;a->last_seen=bsp_seconds();a->encrypted=bs_transfer_encrypted(c);
    copystr(a->folder,folder,sizeof a->folder);copystr(a->peer,c->remote_id,33);copystr(a->address,c->status.peer_address,48);
    batch_revoke(c);c->batch_approval=a;
    if(c->in_batch->tree){if(!tree_prepare_start(c)){batch_revoke(c);return 0;}remember_folder(c,1,folder,1);return 1;}
    if(!batch_destination(c,folder,path) || !bs_accept_conflict(c,path)){batch_revoke(c);return 0;}
    remember_folder(c,1,folder,1);return 1;
}
static int batch_identity(BSClient *c,const unsigned char *token,int count,int index){BSBatchApproval *a=c->batch_approval;return a && a->manifest->count==count && !memcmp(a->manifest->token,token,16) && a->next==index && bsp_seconds()-a->last_seen<=120 && a->port==c->remote_port && !strcmp(a->peer,c->remote_id) && !strcmp(a->address,c->status.peer_address) && a->encrypted==bs_transfer_encrypted(c);}
static void batch_present(BSClient *c,int approved){const BSBatchItem *item=batch_item(c->in_batch,c->in_batch_index);char path[BS_PATH_MAX+1];
    if(!item){fail(c,"Cannot read incoming file list");return;}
    copystr(c->status.filename,item->filename,256);c->status.total=item->total;c->status.awaiting_decision=1;c->state=DECISION;c->decision_start=bsp_seconds();
    if(approved && c->in_batch->tree){if(!tree_accept_entry(c)){char error[256];copystr(error,c->error,sizeof error);finish_error(c,FRAME_ERROR,error);}return;}
    if(approved){if(!batch_destination(c,c->batch_approval->folder,path) || !bs_accept_conflict(c,path)){char error[256];copystr(error,c->error,sizeof error);finish_error(c,FRAME_ERROR,error[0]?error:"Cannot choose destination name");}return;}
    message(c,"Incoming file collection - waiting for your decision");notify(c,BS_EVENT_OFFER);
}
static void batch_send_offer(BSClient *c){unsigned char payload[19+BS_BATCH_MAX*261];const BSBatchItem *item;int at=19,i,n;
    /* Old implementations understand at most eight entries. Larger collections
       safely fall back to individual offers, without broadening old consent. */
    if(c->out_batch->count>BS_BATCH_MAX){unsigned char offer[270];int len=(int)strlen(c->status.filename);memset(offer,0,4);put32(offer+4,c->status.total);put16(offer+8,(unsigned short)len);memcpy(offer+10,c->status.filename,(size_t)len);frame(c,OFFER,offer,10+len);return;}
    memcpy(payload,c->out_batch->token,16);payload[16]=(unsigned char)c->out_batch_index;payload[17]=(unsigned char)c->out_batch->count;payload[18]=0;
    for(i=0;i<c->out_batch->count;i++){item=batch_item(c->out_batch,i);if(!item){fail(c,"Cannot read outgoing list");return;}n=(int)strlen(item->filename);put32(payload+at,item->total);put16(payload+at+4,(unsigned short)n);memcpy(payload+at+6,item->filename,(size_t)n);at+=6+n;}frame(c,OFFER_BATCH,payload,at);
}
static int batch_parse_item_kind(const unsigned char *p,int n,BSBatchItem *item,int tree){int length;if(n<7)return 0;memset(item,0,sizeof *item);item->total=get32(p);length=get16(p+4);if(item->total>BS_FILE_MAX || length<1 || length>255 || n!=6+length || memchr(p+6,0,(size_t)length))return 0;memcpy(item->filename,p+6,(size_t)length);return tree?(bs_safe_relative(item->filename) && (item->filename[strlen(item->filename)-1]!='/' || item->total==0)):bs_safe_name(item->filename);}
static int batch_parse_item(const unsigned char *p,int n,BSBatchItem *item){return batch_parse_item_kind(p,n,item,0);}
static void batch_receive_offer(BSClient *c,const unsigned char *p,int n){BSBatch *b;BSBatchItem item;int at=19,i,length,approved=0;
    if(n<19 || p[17]<2 || p[17]>BS_BATCH_MAX || p[16]>=p[17] || p[18]){fail(c,"Invalid batch offer");return;}
    b=batch_new();if(!b){fail(c,"Cannot create temporary file list");return;}memcpy(b->token,p,16);b->count=p[17];
    for(i=0;i<b->count;i++){if(at+6>n)break;length=get16(p+at+4);if(length>255 || at+6+length>n || !batch_parse_item(p+at,6+length,&item) || (c->cap_ready && (item.total>c->remote_caps.max_send_file || strlen(item.filename)>c->remote_caps.wire_name_bytes)) || !batch_append(b,&item))break;at+=6+length;}
    if(i!=b->count || at!=n){batch_free(b);fail(c,"Invalid batch file list");return;}
    if(batch_identity(c,b->token,b->count,p[16])){approved=1;for(i=0;i<b->count;i++){BSBatchItem copy;const BSBatchItem *old=batch_item(b,i);if(!old){approved=0;break;}copy=*old;old=batch_item(c->batch_approval->manifest,i);if(!old || old->total!=copy.total || strcmp(old->filename,copy.filename)){approved=0;break;}}}
    if(!approved)batch_revoke(c);
    batch_free(c->in_batch);c->in_batch=b;c->in_batch_index=p[16];batch_present(c,approved);
}
static void collection_send(BSClient *c,int reference){unsigned char p[24];memcpy(p,c->out_batch->token,16);put32(p+16,(unsigned long)c->out_batch_index);put32(p+20,(unsigned long)c->out_batch->count);frame(c,c->out_batch->tree?(reference?TREE_REF:TREE_START):(reference?COLLECTION_REF:COLLECTION_START),p,24);c->list_cursor=0;c->state=reference?DECISION:LIST_SENDING;c->decision_start=bsp_seconds();}
static void collection_tick(BSClient *c){const BSBatchItem *item;unsigned char p[261];int n;
    if(c->list_cursor==c->out_batch->count){frame(c,COLLECTION_END,NULL,0);c->state=DECISION;c->decision_start=bsp_seconds();message(c,"Waiting for collection approval...");return;}
    item=batch_item(c->out_batch,c->list_cursor);if(item && c->cap_ready && (item->total>c->remote_caps.max_receive_file || strlen(item->filename)>c->remote_caps.wire_name_bytes)){finish_error(c,FRAME_ERROR,"Collection exceeds receiver file limits");return;}if(!item){fail(c,"Cannot read outgoing file list");return;}n=(int)strlen(item->filename);put32(p,item->total);put16(p+4,(unsigned short)n);memcpy(p+6,item->filename,(size_t)n);frame(c,c->out_batch->tree?TREE_ITEM:COLLECTION_ITEM,p,6+n);c->list_cursor++;
}
static int collection_frame(BSClient *c,int type,const unsigned char *p,int n){unsigned long count,index;BSBatchItem item;int tree=0;
    if(type==TREE_WAIT && n==0 && !c->status.incoming && c->state==DECISION && c->out_batch && c->out_batch->tree && (c->remote_features&BS_CAP_DIRECTORIES)){c->decision_start=bsp_seconds();message(c,"Recipient is preparing destination folders...");return 1;}
    if(type==TREE_START || type==TREE_REF || type==TREE_ITEM){if(!(c->remote_features&BS_CAP_DIRECTORIES) || !(local_features(c)&BS_CAP_DIRECTORIES))goto invalid;tree=1;type=type==TREE_START?COLLECTION_START:type==TREE_REF?COLLECTION_REF:COLLECTION_ITEM;}
    if(type<COLLECTION_START || type>COLLECTION_NEED)return 0;
    if(!((c->status.incoming?local_features(c):c->remote_features)&BS_CAP_COLLECTION) || !c->got_hello)goto invalid;
    if(type==COLLECTION_NEED && !c->status.incoming && c->state==DECISION && c->out_batch && n==0){collection_send(c,0);return 1;}
    if((type==COLLECTION_REF || type==COLLECTION_START) && c->status.incoming && c->state==GREETING && n==24){
        index=get32(p+16);count=get32(p+20);if(count<(tree?1UL:2UL) || count>BS_QUEUE_MAX || index>=count)goto invalid;
        if(type==COLLECTION_REF){
            if(batch_identity(c,p,(int)count,(int)index) && c->batch_approval->manifest->tree==tree){batch_free(c->in_batch);c->in_batch=c->batch_approval->manifest;c->in_batch->refs++;c->in_batch_index=(int)index;batch_present(c,1);}
            else{batch_revoke(c);frame(c,COLLECTION_NEED,NULL,0);}return 1;
        }
        batch_revoke(c);batch_free(c->in_batch);c->in_batch=batch_new();if(!c->in_batch){fail(c,"Cannot store incoming file list: check temporary disk space");return 1;}
        c->in_batch->tree=tree;memcpy(c->in_batch->token,p,16);c->in_batch->count=(int)count;c->in_batch_index=(int)index;c->list_cursor=0;c->state=LIST_RECEIVING;message(c,"Receiving file list...");return 1;
    }
    if(type==COLLECTION_ITEM && c->status.incoming && c->state==LIST_RECEIVING && c->list_cursor<c->in_batch->count){if(tree!=c->in_batch->tree || !batch_parse_item_kind(p,n,&item,tree) || (c->cap_ready && (item.total>c->remote_caps.max_send_file || strlen(item.filename)>c->remote_caps.wire_name_bytes)))goto invalid;if(tree && !tree_manifest_item(c->in_batch,&item,c->list_cursor))goto invalid;if(!batch_append(c->in_batch,&item)){fail(c,"Cannot store file list: check temporary disk space");return 1;}c->list_cursor++;return 1;}
    if(type==COLLECTION_END && n==0 && c->status.incoming && c->state==LIST_RECEIVING && c->list_cursor==c->in_batch->count){if(fflush(c->in_batch->items)){fail(c,"Cannot save incoming file list");return 1;}batch_present(c,0);return 1;}
invalid:fail(c,"Invalid collection sequence");return 1;
}
