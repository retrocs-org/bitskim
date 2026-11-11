/* Disk-backed FIFO. Only a path record and one manifest entry need be resident. */
static int queue_read(const BSClient *c,int index,BSQueued *q) {
    long offset=(long)(c->queue_head+(unsigned long)index)*(long)sizeof *q;
    return c->queue_file && index>=0 && index<c->queue_count && !fseek(c->queue_file,offset,SEEK_SET) && fread(q,sizeof *q,1,c->queue_file)==1;
}
#include "run.h"
#include "treeq.h"
static int bs_queue_add(BSClient *c,const char *id,const char *address,unsigned short port,const char *path) {
    unsigned long size=0;BSQueued q;char normal[BS_PATH_MAX+1];size_t length;int kind;
    if(!c || c->retry || !path || !*path || strlen(path)>BS_PATH_MAX)return 0;
    copystr(normal,path,sizeof normal);length=strlen(normal);while(length>1 && (normal[length-1]=='/' || normal[length-1]=='\\'))normal[--length]=0;path=normal;
    kind=bsp_path_kind(path);if((kind!=1 && kind!=2) || (kind==1 && !bsp_size(path,&size))){copystr(c->error,"Choose a regular file or readable folder; links and special files are not sent",256);return 0;}
    if(c->queue_count>=BS_QUEUE_MAX){copystr(c->error,"Queue count exceeds the portable count range",256);return 0;}
    if((id && strlen(id)!=32) || (!id && (!address || strlen(address)>=48 || !inet_addr(address) || inet_addr(address)==INADDR_NONE))){copystr(c->error,"Invalid queue recipient",256);return 0;}
    memset(&q,0,sizeof q);if(id)copystr(q.id,id,33);else copystr(q.address,address,48);
    q.kind=kind==2?2:0;if(kind==2){if(++c->tree_counter==0)++c->tree_counter;q.group=c->tree_counter;}
    q.total=size;q.port=port?port:BS_PORT;copystr(q.path,path,sizeof q.path);
    if(!c->queue_file)c->queue_file=bsp_temp_file();
    if(!c->queue_file || c->queue_head+(unsigned long)c->queue_count>=2147483647UL/sizeof q ||
       fseek(c->queue_file,(long)(c->queue_head+(unsigned long)c->queue_count)*(long)sizeof q,SEEK_SET) ||
       fwrite(&q,sizeof q,1,c->queue_file)!=1 || fflush(c->queue_file)) {copystr(c->error,"Cannot store queue: check temporary disk space",256);return 0;}
    if(!c->queue_count){c->queue_processed=0;if(c->run_file)fclose(c->run_file);c->run_file=NULL;c->run_count=0;memset(&c->outgoing_run,0,sizeof c->outgoing_run);}
    c->queue_count++;remember_folder(c,0,path,kind==2);return 1;
}
int bs_queue_peer(BSClient *c,const char *id,const char *path){return bs_queue_add(c,id,NULL,0,path);}
int bs_queue_address(BSClient *c,const char *ip,unsigned short port,const char *path){return bs_queue_add(c,NULL,ip,port,path);}
int bs_queue_item(const BSClient *c,int index,BSQueueItem *out){BSQueued q;if(!out)return 0;memset(out,0,sizeof *out);if(!c || !queue_read(c,index,&q))return 0;copystr(out->path,q.path,sizeof out->path);out->total=q.total;out->active=index==0 && c->queue_active && c->status.active && !c->status.incoming;out->paused=c->queue_paused;return 1;}
void bs_queue_progress(const BSClient *c,unsigned long *position,unsigned long *total){if(position)*position=c?c->queue_processed+(c->queue_count?1:0):0;if(total)*total=c?c->queue_processed+(unsigned long)c->queue_count:0;}
int bs_queue_waiting(const BSClient *c){return c?c->queue_count-((c->queue_active && c->status.active && !c->status.incoming)?1:0):0;}
int bs_queue_limit(const BSClient *c){return c?BS_QUEUE_MAX:0;}
int bs_queue_count(const BSClient *c){return c?c->queue_count:0;}
int bs_queue_paused(const BSClient *c){return c?c->queue_paused:0;}
void bs_queue_pause(BSClient *c){if(c)c->queue_paused=1;}
void bs_queue_continue(BSClient *c){if(c)c->queue_paused=0;}
static void queue_pop(BSClient *c){if(!c->queue_count)return;c->queue_processed++;c->queue_head++;c->queue_count--;c->queue_batch_index++;
    if(c->queue_batch && c->queue_batch_index>=c->queue_batch->count){batch_free(c->queue_batch);c->queue_batch=NULL;c->queue_batch_index=0;}
    if(!c->queue_count){if(c->run_count>1 || c->outgoing_run.folders){bs_run_summary(c,0,c->status.message,sizeof c->status.message);notify(c,BS_EVENT_STATUS);}if(c->queue_file)fclose(c->queue_file);c->queue_file=NULL;c->queue_head=0;batch_free(c->queue_batch);c->queue_batch=NULL;c->queue_batch_index=0;}}
void bs_queue_skip(BSClient *c){if(c && !c->status.active){tree_scan_free(c);{BSQueued q;if(queue_read(c,0,&q) && !q.report)run_record(c,BS_RESULT_SKIPPED);}/* Retain a tree manifest so remaining references keep their root. */if(!c->queue_batch || !c->queue_batch->tree){batch_free(c->queue_batch);c->queue_batch=NULL;c->queue_batch_index=0;}queue_pop(c);c->queue_active=0;}}
void bs_queue_clear(BSClient *c){if(!c)return;retry_free(c);tree_scan_free(c);c->queue_count=c->queue_active?1:0;c->queue_paused=c->queue_active!=0;if(!c->queue_active){if(c->queue_file)fclose(c->queue_file);c->queue_file=NULL;c->queue_head=0;batch_free(c->queue_batch);c->queue_batch=NULL;c->queue_batch_index=0;}}
static int queue_batch_prepare(BSClient *c){
    BSQueued head,q;BSBatchItem item;BSBatch *b=c->queue_batch;int budget=16;unsigned long size;
    if(!b){if(!queue_read(c,0,&head))return -1;b=batch_new();if(!b)return -1;c->queue_batch=b;c->queue_batch_index=0;b->count=c->queue_count;
        c->batch_counter++;put32(b->token,bsp_seconds());put32(b->token+4,c->batch_counter);put32(b->token+8,(unsigned long)clock());put32(b->token+12,bs_crc32(c->config.port,(const unsigned char *)c->config.id,32));}
    if(b->streamed)return 1;
    if(!queue_read(c,0,&head))return -1;
    while(budget-- && c->queue_batch_index<b->count){int i=c->queue_batch_index;
        if(!queue_read(c,i,&q))return -1;
        if(strcmp(head.id,q.id) || strcmp(head.address,q.address) || head.port!=q.port || head.group!=q.group || q.kind==2){b->count=i;break;}
        memset(&item,0,sizeof item);if(q.relative[0]){if(!bs_safe_relative(q.relative) || (q.kind==1?bsp_path_kind(q.path)!=2:(!bsp_size(q.path,&size) || bsp_path_kind(q.path)!=1)))return -1;copystr(item.filename,q.relative,256);item.total=q.kind==1?0:size;b->tree=1;}else{if(!bsp_size(q.path,&size) || !wire_filename(q.path,item.filename))return -1;item.total=size;}if(!batch_append(b,&item))return -1;c->queue_batch_index++;
    }
    if(c->queue_batch_index<b->count)return 0;
    if(fflush(b->items))return -1;
    b->streamed=1;c->queue_batch_index=0;return 1;
}
static void queue_tick(BSClient *c){BSQueued q;const BSBatchItem *item;char name[256];unsigned long size;int ok;
    if(c->retry){retry_tick(c);return;}
    if(c->status.active)return;
    if(c->queue_active){c->queue_active=0;if((c->transfer_result==BS_RESULT_COMPLETE || c->transfer_result==BS_RESULT_SKIPPED))queue_pop(c);else{c->queue_paused=1;return;}}
    if(c->queue_paused || !c->queue_count)return;
    if(!queue_read(c,0,&q))goto failed;
    if(q.kind==2){if(tree_scan_tick(c,&q)<0)run_record(c,BS_RESULT_FAILED);return;}
    ok=queue_batch_prepare(c);if(!ok)return;if(ok<0){copystr(c->error,"Cannot prepare file list: check source files and temporary disk space",256);goto failed;}
    if(!queue_read(c,0,&q)){copystr(c->error,"Cannot read transfer queue",256);goto failed;}
    item=batch_item(c->queue_batch,c->queue_batch_index);
    if(!item || (q.kind==1?bsp_path_kind(q.path)!=2:(!bsp_size(q.path,&size) || bsp_path_kind(q.path)!=1)) || (q.relative[0]?(copystr(name,q.relative,256),0):!wire_filename(q.path,name)) || (q.kind!=1 && size!=item->total) || strcmp(name,item->filename)){copystr(c->error,"A queued file changed; clear and queue it again",256);goto failed;}
    c->queue_dir_request=q.kind==1;
    ok=q.id[0]?bs_send_peer(c,q.id,q.path):bs_send(c,q.address,q.port,q.path);c->queue_dir_request=0;
    if(ok){c->queue_active=1;c->out_batch=c->queue_batch;c->out_batch_index=c->queue_batch_index;if(q.relative[0])copystr(c->status.filename,q.relative,256);return;}
failed:run_record(c,BS_RESULT_FAILED);c->queue_paused=1;copystr(c->status.message,c->error,256);notify(c,BS_EVENT_ERROR);
}
