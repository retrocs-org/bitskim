/* Compact summary; fixed storage, no allocations or file I/O while repainting. */
static BSStatus summary_last_transfer;
static unsigned long summary_last_transfer_serial;
static int summary_last_transfer_valid,peer_slots=5,main_shift;
static void transfers_observe(void) {
    BSResult result=bs_transfer_result(client);unsigned long serial=bs_transfer_serial(client);
    if(result==BS_RESULT_NONE || !serial || serial==summary_last_transfer_serial)return;
    summary_last_transfer_serial=serial;summary_last_transfer=*bs_status(client);summary_last_transfer_valid=1;
}
/* Compact main summary; incoming queues have no advertised batch total yet. */
static void transfer_summary(char *title,char *detail,unsigned long *done,unsigned long *total) {
    const BSStatus *s=bs_status(client);char name[256];unsigned long position=0,count=0;
    *done=0;*total=0;detail[0]=0;
    if(s->active) {
        position=count=1;
        if(!s->incoming && bs_queue_count(client))bs_queue_progress(client,&position,&count);
        if(!bs_to_local(s->filename,name,sizeof name))copy(name,"Unicode filename",sizeof name);
        if(s->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0))snprintf(title,100,"Receiving file %d/%d: %.40s",bs_batch_index(client)+1,bs_batch_count(client),name);
        else if(s->incoming)snprintf(title,100,"Receiving: %.54s",name);
        else snprintf(title,100,"Sending file %lu/%lu: %.40s",position,count,name);
        transfer_details(s->transferred,s->total,bs_transfer_rate(client),detail);
        *done=s->transferred;*total=s->total;
    } else {
        snprintf(title,100,"%.69s",s->message);
        if(bs_queue_count(client))snprintf(detail,100,"%d queued%s | Use Queue menu",bs_queue_count(client),bs_queue_paused(client)?" - paused":"");
        else if(summary_last_transfer_valid) {
            transfer_details(summary_last_transfer.transferred,summary_last_transfer.total,0,detail);
            *done=summary_last_transfer.transferred;*total=summary_last_transfer.total;
        }
    }
}
static void summary_draw(int force) {
    char title[100],detail[100];unsigned long done,total;static char old_title[100],old_detail[100];
    static int old_waiting=-1,old_paused=-1;int waiting_count=bs_queue_count(client),paused=bs_queue_paused(client);
    static ULONG last_seconds,last_micros;ULONG seconds,micros;int width=0;struct RastPort *rp;
    if(!window)return;
    transfer_summary(title,detail,&done,&total);
    if(!force && !strcmp(title,old_title) && !strcmp(detail,old_detail) && old_waiting==waiting_count && old_paused==paused)return;
    CurrentTime(&seconds,&micros);
    if(!force && seconds==last_seconds && micros>=last_micros && micros-last_micros<200000UL)return;
    last_seconds=seconds;last_micros=micros;old_waiting=waiting_count;old_paused=paused;copy(old_title,title,sizeof old_title);copy(old_detail,detail,sizeof old_detail);
    rp=window->RPort;SetAPen(rp,main_pens[0]);RectFill(rp,14,190-main_shift,584,244-main_shift);
    SetDrMd(rp,JAM1);SetAPen(rp,main_pens[2]);Move(rp,16,200-main_shift);Text(rp,(STRPTR)title,strlen(title)>69?69:strlen(title));
    SetAPen(rp,main_pens[3]);Move(rp,16,214-main_shift);Text(rp,(STRPTR)detail,strlen(detail)>69?69:strlen(detail));
    SetAPen(rp,main_pens[1]);RectFill(rp,16,220-main_shift,580,226-main_shift);
    if(total)width=(int)((done>total?total:done)/((total+563)/564));
    if(width>564)width=564;
    if(width>0) {SetAPen(rp,main_pens[4]);RectFill(rp,16,220-main_shift,15+width,226-main_shift);}
    Move(rp,16,240-main_shift);SetAPen(rp,main_pens[3]);
    {char hint[100];int waiting=bs_queue_count(client);BSQueueItem item;
     if(bs_queue_item(client,0,&item) && item.active)waiting--;
     if(bs_status(client)->active && bs_status(client)->incoming && (bs_batch_count(client)>1 || bs_batch_is_tree(client)!=0))waiting=bs_batch_count(client)-bs_batch_index(client)-1;
     if(waiting<0)waiting=0;
     if(waiting || !bs_status(client)->active || !bs_status(client)->incoming)
         snprintf(hint,sizeof hint,"%d file%s waiting%s",waiting,waiting==1?"":"s",bs_queue_paused(client)?(bs_status(client)->active?" | Stops after this file":" | Queue paused"):"");
     else copy(hint,"Receiving files one at a time",sizeof hint);
     bs_progress_summary(client,hint,sizeof hint);
     Text(rp,(STRPTR)hint,strlen(hint)>69?69:strlen(hint));}
}
