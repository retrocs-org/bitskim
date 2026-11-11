/* Session result ledger; complete source paths stay on disk, not in RAM. */
struct BSRetry {FILE *stage;unsigned long cursor,group;int count,folder_errors;char previous[16][256];};
static void run_adjust(BSRun *r,int result,int directory,int add){unsigned long *value=NULL;if(result==BS_RESULT_COMPLETE)value=directory?&r->folders:&r->complete;else if(result==BS_RESULT_SKIPPED)value=&r->skipped;else if(result==BS_RESULT_FAILED)value=&r->failed;else if(result==BS_RESULT_CANCELLED || result==BS_RESULT_DECLINED)value=&r->cancelled;if(value){if(add)(*value)++;else if(*value)(*value)--;}}
static void run_record(BSClient *c,int result){BSQueued q;BSRunRecord r;unsigned long slot;int old=0;if(!queue_read(c,0,&q))return;
 if(!c->run_file)c->run_file=bsp_temp_file();
 if(!c->run_file){c->outgoing_run.lost=1;return;}slot=q.report;
 if(slot){if(fseek(c->run_file,(long)(slot-1)*sizeof r,SEEK_SET) || fread(&r,sizeof r,1,c->run_file)!=1){c->outgoing_run.lost=1;return;}old=r.result;if(old==result)return;}
 else{if(c->run_count>=BS_QUEUE_MAX){c->outgoing_run.lost=1;return;}slot=c->run_count+1;}
 memset(&r,0,sizeof r);q.report=slot;r.q=q;r.result=result;if(fseek(c->run_file,(long)(slot-1)*sizeof r,SEEK_SET) || fwrite(&r,sizeof r,1,c->run_file)!=1 || fflush(c->run_file)){c->outgoing_run.lost=1;return;}
 if(!old){if(fseek(c->queue_file,(long)c->queue_head*sizeof q,SEEK_SET) || fwrite(&q,sizeof q,1,c->queue_file)!=1 || fflush(c->queue_file)){c->outgoing_run.lost=1;return;}if(slot>c->run_count)c->run_count=slot;}
 run_adjust(&c->outgoing_run,old,q.kind!=0,0);run_adjust(&c->outgoing_run,result,q.kind!=0,1);
}
static void incoming_record(BSClient *c){int old=0,result=c->transfer_result;unsigned long slot;size_t n;BSBatch *b=c->in_batch;if(!result || !c->transfer_serial || c->receive_serial==c->transfer_serial)return;c->receive_serial=c->transfer_serial;
 if(!b){memset(&c->incoming_run,0,sizeof c->incoming_run);c->run_token_set=0;}else{
 if(!c->run_token_set || memcmp(c->run_token,b->token,16)){if(c->incoming_results)fclose(c->incoming_results);c->incoming_results=bsp_temp_file();memset(&c->incoming_run,0,sizeof c->incoming_run);memcpy(c->run_token,b->token,16);c->run_token_set=1;}
 slot=(unsigned long)c->in_batch_index;if(c->incoming_results){if(!fseek(c->incoming_results,(long)slot*sizeof old,SEEK_SET)){if(fread(&old,sizeof old,1,c->incoming_results)!=1)old=0;clearerr(c->incoming_results);if(fseek(c->incoming_results,(long)slot*sizeof result,SEEK_SET) || fwrite(&result,sizeof result,1,c->incoming_results)!=1 || fflush(c->incoming_results))c->incoming_run.lost=1;}else c->incoming_run.lost=1;}else c->incoming_run.lost=1;}
 n=strlen(c->status.filename);run_adjust(&c->incoming_run,old,n && c->status.filename[n-1]=='/',0);run_adjust(&c->incoming_run,result,n && c->status.filename[n-1]=='/',1);
}
int bs_batch_summary(const BSClient *c,char *out,size_t capacity){char text[180],size[32];BSBatch *b=c?c->in_batch:NULL;if(!b || !out || !capacity)return 0;if(b->hi || b->lo>=1048576UL)sprintf(size,"%lu.%lu MiB",b->hi*4096UL+b->lo/1048576UL,(b->lo%1048576UL)*10/1048576UL);else if(b->lo>=1024)sprintf(size,"%lu.%lu KiB",b->lo/1024,(b->lo%1024)*10/1024);else sprintf(size,"%lu B",b->lo);sprintf(text,"%lu file%s, %lu folder%s, %s",b->files,b->files==1?"":"s",b->dirs,b->dirs==1?"":"s",size);copystr(out,text,capacity);return 1;}
int bs_run_summary(const BSClient *c,int incoming,char *out,size_t capacity){char text[220];const BSRun *r;if(!c || !out || !capacity)return 0;r=incoming?&c->incoming_run:&c->outgoing_run;sprintf(text,"%s: %lu completed files, %lu folders; %lu skipped; %lu failed; %lu cancelled%s",incoming?"Received":"Sent",r->complete,r->folders,r->skipped,r->failed,r->cancelled,r->lost?" (report incomplete)":"");copystr(out,text,capacity);return 1;}
static void retry_free(BSClient *c){if(c->retry){if(c->retry->stage)fclose(c->retry->stage);free(c->retry);c->retry=NULL;}}
int bs_retry_failed(BSClient *c){if(!c)return 0;if(c->status.active || c->queue_count || c->retry || !c->outgoing_run.failed || !c->run_file){copystr(c->error,"Retry failed only requires an idle, empty queue with recorded failures. Continue retries the current item; clear waiting items explicitly first.",256);return 0;}c->error[0]=0;c->retry=(BSRetry *)calloc(1,sizeof *c->retry);if(!c->retry || !(c->retry->stage=bsp_temp_file())){retry_free(c);copystr(c->error,"Cannot create retry list; check memory and temporary disk space",256);return 0;}message(c,"Preparing failed-only retry list...");return 1;}
static int retry_append(BSClient *c,const BSQueued *q){if(c->retry->count>=BS_QUEUE_MAX || fwrite(q,sizeof *q,1,c->retry->stage)!=1)return 0;c->retry->count++;return 1;}
static void retry_tick(BSClient *c){BSRetry *s=c->retry;int budget=8;
 while(budget-- && s->cursor<c->run_count){BSRunRecord r;BSQueued q;int i,depth=0;char path[BS_PATH_MAX+1],relative[256];size_t j;
 if(fseek(c->run_file,(long)s->cursor*sizeof r,SEEK_SET) || fread(&r,sizeof r,1,c->run_file)!=1)goto failed;
 s->cursor++;if(r.result!=BS_RESULT_FAILED)continue;q=r.q;
 if(q.kind!=0){s->folder_errors++;continue;}
 if(q.relative[0]){unsigned long group=q.group;for(j=0;q.relative[j];j++)if(q.relative[j]=='/')depth++;if(depth<1 || depth>16)goto failed;
 if(s->group!=group){s->group=group;memset(s->previous,0,sizeof s->previous);}
 for(i=0;i<depth;i++){char *slash;int k;BSQueued dir=q;copystr(relative,q.relative,sizeof relative);for(k=0,j=0;relative[j];j++)if(relative[j]=='/' && k++==i){relative[j+1]=0;break;}
 if(!strcmp(relative,s->previous[i]))continue;
 copystr(path,q.path,sizeof path);for(k=depth;k>i;k--){slash=strrchr(path,'/');if(!slash)goto failed;*slash=0;}
 copystr(dir.path,path,sizeof dir.path);copystr(dir.relative,relative,sizeof dir.relative);dir.kind=1;dir.total=0;dir.report=0;if(!retry_append(c,&dir))goto failed;copystr(s->previous[i],relative,256);for(k=i+1;k<16;k++)s->previous[k][0]=0;
 }}else{s->group=0;memset(s->previous,0,sizeof s->previous);}if(!retry_append(c,&q))goto failed;
 }
 if(s->cursor==c->run_count){int folder_errors=s->folder_errors;if(!s->count){copystr(c->error,"Only folder entries failed. Queue those folders again explicitly.",256);goto failed;}if(fflush(s->stage))goto failed;c->queue_file=s->stage;s->stage=NULL;c->queue_head=0;c->queue_count=s->count;c->queue_processed=0;c->queue_paused=0;retry_free(c);message(c,folder_errors?"Failed files queued; failed folder entries need manual requeue":"Failed-only files queued; completed and skipped files excluded");}return;
failed:if(!c->error[0])copystr(c->error,"Cannot build failed-only list; check temporary disk space",256);retry_free(c);copystr(c->status.message,c->error,256);notify(c,BS_EVENT_ERROR);
}
/* Directory totals come from the disk manifest, including skipped entries. */
static void progress_size(unsigned long hi,unsigned long lo,char out[40]) {
    if(hi || lo>=1048576UL)sprintf(out,"%lu.%lu MiB",hi*4096UL+lo/1048576UL,(lo%1048576UL)*10/1048576UL);
    else if(lo>=1024)sprintf(out,"%lu.%lu KiB",lo/1024,(lo%1024)*10/1024);
    else sprintf(out,"%lu B",lo);
}
int bs_progress_summary(const BSClient *c,char *out,size_t capacity) {
    BSBatch *batch;BSBatchRecord previous;unsigned long hi=0,lo=0,index=1,count=1;
    char done[40],total[40],rate[40],text[256];int item;const char *kind="File";
    if(!c || !out || !capacity)return 0;
    if(!c->status.active)return bs_run_summary(c,c->status.incoming,out,capacity);
    batch=c->status.incoming?c->in_batch:c->out_batch;
    item=c->status.incoming?c->in_batch_index:c->out_batch_index;
    if(batch){
        count=(unsigned long)batch->count;index=(unsigned long)item+1;
        if(item && batch_record(batch,item-1,&previous)){hi=previous.hi;lo=previous.lo;}
        if(batch->tree){
            size_t n=strlen(c->status.filename);
            if(n && c->status.filename[n-1]=='/')kind="Folder item";
            else{
                count=batch->files;index=1;
                if(item && batch_record(batch,item-1,&previous))index=previous.files+1;
            }
        }
        wide_add(&hi,&lo,0,c->status.transferred);
        progress_size(batch->hi,batch->lo,total);
    }else{lo=c->status.transferred;progress_size(0,c->status.total,total);}
    progress_size(hi,lo,done);progress_size(0,bs_transfer_rate(c),rate);
    sprintf(text,"%s %lu of %lu | Overall %s / %s | %s/s",kind,index,count,done,total,rate);
    copystr(out,text,capacity);return strlen(text)<capacity;
}
