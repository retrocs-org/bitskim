/* Optional connection reuse is scoped to one negotiated immutable collection. */
int bs_session_enable(BSClient *c,int enabled){if(!c || c->status.active || c->queue_count)return 0;c->session_enabled=enabled!=0;bs_refresh(c);return 1;}
static void session_reset(BSClient *c,int incoming){
    BSCapabilities caps=c->remote_caps;BSBatch *batch=c->in_batch;int index=c->in_batch_index,rx=c->rx_count,known=c->cap_ready;unsigned features=c->remote_features;unsigned short port=c->remote_port;
    char id[33],address[48],name[64];copystr(id,c->remote_id,sizeof id);copystr(address,c->status.peer_address,sizeof address);copystr(name,c->status.peer_name,sizeof name);
    c->in_batch=NULL;reset_status(c,incoming,address);c->in_batch=batch;c->in_batch_index=index;c->rx_count=rx;
    c->remote_caps=caps;c->remote_features=features;c->cap_ready=known;c->got_hello=1;c->remote_port=port;copystr(c->remote_id,id,sizeof c->remote_id);copystr(c->status.peer_name,name,sizeof c->status.peer_name);c->state=GREETING;
}
static void session_finish(BSClient *c){int keep=0,rx=c->rx_count;BSSocket socket=c->sock;
    if(c->session_enabled && (c->remote_features&BS_CAP_SESSION) && (c->transfer_result==BS_RESULT_COMPLETE || c->transfer_result==BS_RESULT_SKIPPED)){
        if(c->status.incoming)keep=c->batch_approval && c->batch_approval->next<c->batch_approval->manifest->count;
        else keep=c->out_batch && c->out_batch_index+1<c->out_batch->count && c->queue_count>1 && !c->queue_paused;
    }
    if(keep){if(!c->status.incoming)memcpy(c->session_token,c->out_batch->token,16);c->sock=BS_INVALID_SOCKET;cleanup_transfer(c);c->sock=socket;c->rx_count=rx;c->state=SESSION_IDLE;c->session_since=bsp_seconds();}
    else {if(socket!=BS_INVALID_SOCKET){park_socket(c,socket);c->sock=BS_INVALID_SOCKET;}cleanup_transfer(c);}
}
static int session_matches(BSClient *c,const BSQueued *q){return c->queue_batch && !memcmp(c->session_token,c->queue_batch->token,16) && (!q->id[0] || !strcmp(q->id,c->remote_id)) && (!q->address[0] || (!strcmp(q->address,c->status.peer_address) && q->port==c->remote_port));}
static int session_send(BSClient *c,const BSQueued *q){
    session_reset(c,0);c->status.total=q->total;copystr(c->source,q->path,sizeof c->source);
    if(q->relative[0])copystr(c->status.filename,q->relative,sizeof c->status.filename);else if(!wire_filename(q->path,c->status.filename)){fail(c,BS_ERR_SOURCE,"Queued filename is unavailable");return 0;}
    if(c->metadata_enabled)c->source_mtime_valid=bsp_mtime_get(q->path,&c->source_mtime);
    c->out_batch=c->queue_batch;c->out_batch_index=c->queue_batch_index;begin_offer(c);return c->status.active;
}
