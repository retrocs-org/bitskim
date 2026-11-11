/* Portable local ARexx protocol. Included after the native UI declarations. */
#define BS_EVENT_HISTORY 32
#define BS_REXX_RESULT_MAX 4096
typedef struct {
    BSStatus status; int available,result;
    char session[33],id[42];
} RexxTransfer;
static struct { unsigned long seq; int type,result; char id[42]; } event_history[BS_EVENT_HISTORY];
static unsigned long event_sequence,transfer_number,observed_serial,session_epoch;
static char session_base[25],session_id[33],command_error[256],command_code[24]="NONE";
static RexxTransfer current_transfer,last_transfer;
static unsigned long command_error_generation;
static void rexx_session_rotate(void) {
    session_epoch=(session_epoch+1)&0xffffffffUL;
    snprintf(session_id,sizeof(session_id),"%s%08lx",session_base,session_epoch);
    event_sequence=0; memset(event_history,0,sizeof(event_history));
}
static void rexx_session_init(const char *base) {
    snprintf(session_base,sizeof(session_base),"%.24s",base); session_epoch=0;
    transfer_number=observed_serial=0; memset(&current_transfer,0,sizeof(current_transfer)); memset(&last_transfer,0,sizeof(last_transfer));
    command_error[0]=0; strcpy(command_code,"NONE"); rexx_session_rotate();
}
static void rexx_observe(void) {
    unsigned long serial; const BSStatus *s;
    if(!client) return;
    serial=bs_transfer_serial(client); if(!serial) return; s=bs_status(client);
    if(serial!=observed_serial) {
        observed_serial=serial;
        if(transfer_number==0xffffffffUL) { transfer_number=0; rexx_session_rotate(); }
        transfer_number++;
        memset(&current_transfer,0,sizeof(current_transfer)); current_transfer.available=1;
        strcpy(current_transfer.session,session_id);
        snprintf(current_transfer.id,sizeof(current_transfer.id),"%s-%08lx",session_id,transfer_number);
    }
    current_transfer.status=*s; current_transfer.result=(int)bs_transfer_result(client);
    /* A terminal result is immutable, including while a receive COMPLETE drains. */
    if(current_transfer.result && strcmp(last_transfer.id,current_transfer.id)) {
        last_transfer=current_transfer; last_transfer.status.active=0; last_transfer.status.awaiting_decision=0;
    }
}
static void record_event(BSEvent event) {
    unsigned long slot;
    rexx_observe();
    if(event_sequence==0xffffffffUL) rexx_session_rotate();
    slot=event_sequence++%BS_EVENT_HISTORY;
    event_history[slot].seq=event_sequence; event_history[slot].type=(int)event;
    event_history[slot].id[0]=0; event_history[slot].result=0;
    if(event!=BS_EVENT_PEERS && current_transfer.available) {
        strcpy(event_history[slot].id,current_transfer.id); event_history[slot].result=current_transfer.result;
    }
}
/* Quotes use doubled matching delimiters. Backslashes and other bytes are literal. */
static int rexx_token(char **cursor,char *out,size_t capacity) {
    char *p=*cursor,quote=0; size_t n=0;
    while(*p && isspace((unsigned char)*p)) p++;
    if(*p=='"' || *p=='\'') quote=*p++;
    while(*p) {
        if(quote && *p==quote) { if(p[1]==quote) p++; else break; }
        else if(!quote && isspace((unsigned char)*p)) break;
        if(n+1>=capacity) return 0;
        out[n++]=*p++;
    }
    if(quote) { if(*p!=quote) return 0; p++; if(*p && !isspace((unsigned char)*p)) return 0; }
    out[n]=0; while(*p && isspace((unsigned char)*p)) p++; *cursor=p;
    return n!=0;
}
static int rexx_error(char *out,size_t cap,int rc,const char *code,const char *text) {
    command_error_generation++;
    snprintf(command_code,sizeof(command_code),"%s",code); snprintf(command_error,sizeof(command_error),"%s",text);
    snprintf(out,cap,"%s",text); return rc;
}
static int rexx_uint(const char *s,unsigned long *out) {
    unsigned long n=0; if(!*s) return 0;
    while(*s) { unsigned int digit=(unsigned int)(*s++-'0'); if(digit>9 || n>(0xffffffffUL-digit)/10) return 0; n=n*10+digit; }
    *out=n; return 1;
}
static int rexx_field(char *out,size_t cap,const char *key,const char *value) {
    static const char hex[]="0123456789ABCDEF"; size_t n=strlen(out),k=strlen(key); const unsigned char *p=(const unsigned char *)value;
    if(n+k+3>=cap) return 0;
    out[n++]=' '; memcpy(out+n,key,k); n+=k; out[n++]='=';
    while(*p) {
        unsigned char v=*p++;
        if((v>='a'&&v<='z') || (v>='A'&&v<='Z') || (v>='0'&&v<='9') || strchr("-._~",v)) { if(n+1>=cap) return 0; out[n++]=(char)v; }
        else { if(n+3>=cap) return 0; out[n++]='%'; out[n++]=hex[v>>4]; out[n++]=hex[v&15]; }
    }
    out[n]=0; return 1;
}
static int rexx_transfer_details(const RexxTransfer *t,char *out,size_t cap) {
    if(!t->available) { snprintf(out,cap,"AVAILABLE=0"); return 0; }
    snprintf(out,cap,"AVAILABLE=1 SESSION=%s ID=%s ACTIVE=%d INCOMING=%d BYTES=%lu TOTAL=%lu RESULT=%d",t->session,t->id,t->status.active,t->status.incoming,t->status.transferred,t->status.total,t->result);
    if(!rexx_field(out,cap,"FILENAME",t->status.filename) || !rexx_field(out,cap,"PEER",t->status.peer_name) || !rexx_field(out,cap,"ADDRESS",t->status.peer_address) || !rexx_field(out,cap,"ERROR",t->result>1?t->status.message:"")) return 20;
    return 0;
}
static int rexx_dispatch(char *line,char *result,size_t capacity) {
    char cmd[32],a[1024],b[1024],*cursor=line,*colon; int i;
    const BSStatus *status=bs_status(client); const BSPeer *peer;
    if(!rexx_token(&cursor,cmd,sizeof(cmd))) return 10;
    for(i=0;cmd[i];i++) cmd[i]=(char)toupper((unsigned char)cmd[i]);
    strcpy(result,"OK"); rexx_observe();
    if(!strcmp(cmd,"VISIBLE") || !strcmp(cmd,"RECEIVING")) {
        int value;
        if(!rexx_token(&cursor,a,sizeof(a)) || *cursor) return 10;
        for(i=0;a[i];i++) a[i]=(char)toupper((unsigned char)a[i]);
        if(!strcmp(a,"ON")) value=1; else if(!strcmp(a,"OFF")) value=0; else return 10;
        if(!strcmp(cmd,"VISIBLE")) visible=value; else receiving=value;
        if(!bs_to_utf8(client_name,wire_name,sizeof(wire_name))) return 20;
        bs_configure(client,wire_name,visible,receiving); return 0;
    }
    if(!strcmp(cmd,"SEND") || !strcmp(cmd,"SENDID")) {
        unsigned short port=BS_PORT; unsigned long value; char address[48];
        if(!rexx_token(&cursor,a,sizeof(a)) || !rexx_token(&cursor,b,sizeof(b)) || *cursor) return 10;
        if(status->active) return rexx_error(result,capacity,20,"BUSY","A transfer is already active");
        if(!strcmp(cmd,"SENDID")) {
            if(strlen(a)!=32) return 10;
            for(i=0;i<32;i++) { if(!isxdigit((unsigned char)a[i])) return 10; a[i]=(char)tolower((unsigned char)a[i]); }
            peer=NULL;
            for(i=0;i<bs_peer_count(client);i++) if(!strcmp(bs_peer(client,i)->id,a)) { peer=bs_peer(client,i); break; }
            if(!peer) return rexx_error(result,capacity,20,"UNAVAILABLE","Peer ID is not in the current catalog; show BitSkim to resume discovery if iconified");
            snprintf(address,sizeof(address),"%s",peer->address); port=peer->port;
        } else {
            colon=strchr(a,':'); if(colon) { *colon++=0; if(!rexx_uint(colon,&value) || value<1 || value>65535) return 10; port=(unsigned short)value; }
            if(strlen(a)>=sizeof(address)) return 10;
            strcpy(address,a);
        }
        if(!(!strcmp(cmd,"SENDID")?bs_send_peer(client,a,b):bs_send(client,address,port,b)))
            return rexx_error(result,capacity,20,"SEND_FAILED",bs_last_error(client));
        rexx_observe();
        if(!strcmp(cmd,"SENDID")) snprintf(result,capacity,"ID=%s",current_transfer.id);
        return 0;
    }
    if(!strcmp(cmd,"QUEUE")) {
        if(!rexx_token(&cursor,a,sizeof a) || *cursor) return 10;
        for(i=0;a[i];i++)a[i]=(char)toupper((unsigned char)a[i]);
        if(!strcmp(a,"STATUS"))snprintf(result,capacity,"COUNT=%d PAUSED=%d",bs_queue_count(client),bs_queue_paused(client));
        else if(!strcmp(a,"CONTINUE"))bs_queue_continue(client);
        else if(!strcmp(a,"PAUSE"))bs_queue_pause(client);
        else if(!strcmp(a,"SKIP")) {if(status->active)return rexx_error(result,capacity,20,"BUSY","Cannot skip an active transfer");bs_queue_skip(client);}
        else if(!strcmp(a,"CLEAR"))bs_queue_clear(client);
        else return 10;
        return 0;
    }
    if(!strcmp(cmd,"ENQUEUEID")) {
        if(!rexx_token(&cursor,a,sizeof a) || !rexx_token(&cursor,b,sizeof b) || *cursor)return 10;
        if(!bs_queue_peer(client,a,b))return rexx_error(result,capacity,20,"QUEUE_FAILED",bs_last_error(client));
        return 0;
    }
    if(!strcmp(cmd,"EVENTS") || !strcmp(cmd,"EVENTDETAILS")) {
        unsigned long since=0,oldest=event_sequence>BS_EVENT_HISTORY?event_sequence-BS_EVENT_HISTORY:0; size_t used; int detailed=!strcmp(cmd,"EVENTDETAILS");
        if(*cursor && !rexx_uint(cursor,&since)) return 10;
        snprintf(result,capacity,"LATEST=%lu LOST=%d",event_sequence,since<oldest || since>event_sequence);
        if(detailed) { used=strlen(result); snprintf(result+used,capacity-used," SESSION=%s",session_id); }
        used=strlen(result);
        for(i=0;i<BS_EVENT_HISTORY;i++) {
            unsigned long seq=oldest+(unsigned long)i+1,slot; char entry[100];
            if(seq>event_sequence) break;
            if(seq>since) {
                slot=(seq-1)%BS_EVENT_HISTORY;
                if(detailed) snprintf(entry,sizeof(entry)," %lu:%d:%s:%d",seq,event_history[slot].type,event_history[slot].id[0]?event_history[slot].id:"-",event_history[slot].result);
                else snprintf(entry,sizeof(entry)," %lu:%d",seq,event_history[slot].type);
                if(used+strlen(entry)+1>=capacity) return 20;
                strcpy(result+used,entry); used+=strlen(entry);
            }
        }
        return 0;
    }
    if(!strcmp(cmd,"PEER") || !strcmp(cmd,"PEERINFO")) {
        unsigned long index;
        if(!strcmp(cmd,"PEER")) {
            if(!rexx_uint(cursor,&index) || index>=(unsigned long)bs_peer_count(client)) return 10;
            peer=bs_peer(client,(int)index); snprintf(result,capacity,"%s:%u %s",peer->address,peer->port,peer->name); return 0;
        }
        if(!rexx_token(&cursor,a,sizeof(a)) || *cursor || strlen(a)!=32) return 10;
        for(i=0;i<32;i++) { if(!isxdigit((unsigned char)a[i])) return 10; a[i]=(char)tolower((unsigned char)a[i]); }
        peer=NULL; for(i=0;i<bs_peer_count(client);i++) if(!strcmp(bs_peer(client,i)->id,a)) { peer=bs_peer(client,i); break; }
        if(!peer) return rexx_error(result,capacity,20,"UNAVAILABLE","Peer ID is not in the current catalog");
        snprintf(result,capacity,"ID=%s PORT=%u",peer->id,peer->port);
        if(!rexx_field(result,capacity,"NAME",peer->name) || !rexx_field(result,capacity,"PLATFORM",peer->platform) || !rexx_field(result,capacity,"ADDRESS",peer->address)) return 20;
        return 0;
    }
    if(*cursor) return 10;
    if(!strcmp(cmd,"VERSION")) strcpy(result,"0.1");
    else if(!strcmp(cmd,"CAPABILITIES")) snprintf(result,capacity,"AREXX=2 STATUS=1 BPS=1 SESSION=1 DETAILS=1 LAST=1 EVENTDETAILS=1 PEERS=1 PEERINFO=1 SENDID=1 ENQUEUEID=1 QUEUE=1 NETWORKCAPS=1 ERROR=1 FORMAT=PCTBYTES MAXCOMMAND=1199 MAXRESULT=4095 HISTORY=32");
    else if(!strcmp(cmd,"NETWORKCAPS")) {BSCapabilities caps;bs_local_capabilities(client,&caps);snprintf(result,capacity,"VERSION=1 TX=%lu RX=%lu MAXSEND=%lu MAXRECEIVE=%lu WIRENAME=%u LOCALNAME=%u PATH=%u NAMEPOLICY=%u CODEPAGE=%u COLLECTION=%lu SECURITY=%u PAIRED=%u",caps.send_features,caps.receive_features,caps.max_send_file,caps.max_receive_file,caps.wire_name_bytes,caps.local_name_bytes,caps.path_bytes,caps.name_policy,caps.code_page,caps.collection_files,caps.security_mode,caps.paired);}
    else if(!strcmp(cmd,"SESSION")) snprintf(result,capacity,"SESSION=%s",session_id);
    else if(!strcmp(cmd,"DETAILS")) return rexx_transfer_details(&current_transfer,result,capacity);
    else if(!strcmp(cmd,"LAST")) return rexx_transfer_details(&last_transfer,result,capacity);
    else if(!strcmp(cmd,"ERROR")) { snprintf(result,capacity,"CODE=%s",command_code); return rexx_field(result,capacity,"TEXT",command_error)?0:20; }
    else if(!strcmp(cmd,"PEERS")) {
        size_t used; snprintf(result,capacity,"COUNT=%d",bs_peer_count(client));
        for(i=0;i<bs_peer_count(client);i++) { used=strlen(result); if(used+48>=capacity) return 20; snprintf(result+used,capacity-used," ID%d=%s",i,bs_peer(client,i)->id); }
    }
    else if(!strcmp(cmd,"STATUS")) snprintf(result,capacity,"ACTIVE=%ld INCOMING=%ld AWAITING=%ld ICONIFIED=%ld RECEIVING=%ld VISIBLE=%ld PORT=%u PEERS=%ld BYTES=%lu TOTAL=%lu RELOAD=%ld BPS=%lu",(long)status->active,(long)status->incoming,(long)status->awaiting_decision,(long)iconified,(long)receiving,(long)visible,listen_port,(long)bs_peer_count(client),status->transferred,status->total,(long)reload_pending,bs_transfer_rate(client));
    else if(!strcmp(cmd,"SHOW")) { if(!restore_ui()) return 20; }
    else if(!strcmp(cmd,"HIDE")) { if(!iconify_ui()) return 20; }
    else if(!strcmp(cmd,"PREFS")) launch_preferences();
    else if(!strcmp(cmd,"RELOAD")) reload_pending=1;
    else if(!strcmp(cmd,"REFRESH")) bs_refresh(client);
    else if(!strcmp(cmd,"CANCEL")) bs_cancel(client);
    else if(!strcmp(cmd,"QUIT")) quit_pending=1;
    else return 10;
    return 0;
}
static int rexx_command(char *line,char *result,size_t capacity) {
    int rc; unsigned long generation=command_error_generation;
    if(capacity<BS_REXX_RESULT_MAX) return 20;
    result[0]=0;
    rc=rexx_dispatch(line,result,capacity);
    if(rc && generation==command_error_generation)
        return rexx_error(result,capacity,rc,rc==10?"BAD_COMMAND":"FAILED",rc==10?"Unknown command or invalid arguments":"Operation failed or response exceeds capacity");
    return rc;
}
