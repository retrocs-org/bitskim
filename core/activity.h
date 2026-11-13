/* Bounded local history. Not a wire response; never contains pairing secrets. */
static void history_record(BSClient *c) {
    BSHistoryItem *h;
    if(!c->status.filename[0] || !c->transfer_serial || c->history_serial==c->transfer_serial || c->transfer_result==BS_RESULT_NONE)return;
    c->history_serial=c->transfer_serial;h=&c->history[c->history_next];memset(h,0,sizeof *h);
    h->serial=c->transfer_serial;h->total=c->status.total;h->transferred=c->status.transferred;h->finished=bsp_seconds();
    h->incoming=c->status.incoming;h->result=c->transfer_result;h->encrypted=bs_transfer_encrypted(c);h->verified=c->verified;
    copystr(h->filename,c->status.filename,sizeof h->filename);copystr(h->peer,c->status.peer_name,sizeof h->peer);
    copystr(h->path,h->incoming?c->dest:c->source,sizeof h->path);copystr(h->message,c->status.message,sizeof h->message);
    copystr(h->peer_id,c->remote_id,sizeof h->peer_id);copystr(h->address,c->status.peer_address,sizeof h->address);h->port=c->remote_port;
    c->history_next=(c->history_next+1)%BS_HISTORY_MAX;if(c->history_count<BS_HISTORY_MAX)c->history_count++;
}
int bs_history_count(const BSClient *c){return c?c->history_count:0;}
int bs_history_item(const BSClient *c,int i,BSHistoryItem *out){int slot;if(!c || !out || i<0 || i>=c->history_count)return 0;slot=(c->history_next+BS_HISTORY_MAX-1-i)%BS_HISTORY_MAX;*out=c->history[slot];return 1;}
int bs_history_find(const BSClient *c,unsigned long serial){int i;BSHistoryItem h;for(i=0;i<bs_history_count(c);i++)if(bs_history_item(c,i,&h) && h.serial==serial)return i;return -1;}
void bs_history_clear(BSClient *c){if(c){memset(c->history,0,sizeof c->history);c->history_count=c->history_next=0;}}
int bs_history_text(const BSClient *c,int i,char *out,size_t capacity){const BSHistoryItem *h;char text[2048];if(!out || !capacity || !c || i<0 || i>=c->history_count)return 0;h=&c->history[(c->history_next+BS_HISTORY_MAX-1-i)%BS_HISTORY_MAX];
    sprintf(text,"%s: %.255s\nPeer: %.63s\n%lu / %lu bytes\nResult: %.255s\nPath: %.1023s\nIntegrity: %s",h->incoming?"Received":"Sent",h->filename,h->peer,h->transferred,h->total,h->message,h->path,h->result!=BS_RESULT_COMPLETE?"not verified (transfer incomplete)":h->verified?"written file verified (CRC32)":"receiver disk verification not confirmed");copystr(out,text,capacity);return strlen(text)<capacity;}
int bs_retry_history(BSClient *c,int i){BSHistoryItem h;if(!bs_history_item(c,i,&h) || h.incoming || h.result==BS_RESULT_COMPLETE){if(c)copystr(c->error,"Retry is available for unsuccessful outgoing files only",256);return 0;}
    if(h.peer_id[0])return bs_queue_peer(c,h.peer_id,h.path);
    return bs_queue_address(c,h.address,h.port,h.path);
}
int bs_diagnostics(const BSClient *c,char *out,size_t capacity){char text[2048],report[220],local[48]="not connected";BSCapabilities caps;const char *stack;
#if !defined(BS_DOS) && !defined(BS_ATARI)
    struct sockaddr_in address;BSSockLen address_size=sizeof address;
#endif
    if(!c || !out || !capacity)return 0;
#if defined(__amigaos__)
    stack="Amiga bsdsocket.library (provided by your running TCP/IP stack)";
#elif defined(BS_DOS)
    stack="mTCP / packet driver";
#elif defined(BS_ATARI_STING)
    stack="STiNG / configured NetUSBee driver";
#elif defined(BS_ATARI)
    stack="MiNTNet sockets";
#elif defined(__riscos__)
    stack="RISC OS Internet sockets";
#elif defined(_WIN32)
    stack="Winsock";
#else
    stack="System BSD sockets";
#endif

#if !defined(BS_DOS) && !defined(BS_ATARI)
    if(c->sock!=BS_INVALID_SOCKET && !getsockname(c->sock,(struct sockaddr *)&address,&address_size))copystr(local,inet_ntoa(address.sin_addr),sizeof local);
#else
    copystr(local,"see TCP/IP stack settings",sizeof local);
#endif
    if(!strcmp(local,"not connected") && c->check_local[0])copystr(local,c->check_local,sizeof local);
    bs_local_capabilities(c,&caps);
    sprintf(text,"BitSkim 0.2 / %.31s\nConnection check: %.159s\nStack: %s\nName: %.63s\nBind: %.47s (all interfaces if empty)\nLocal connection address: %.47s\nTCP %u / discovery UDP %u\nVisible %s / incoming %s / peers %d\nSecurity: %s; transport %s\nSend / receive cap: %lu / %lu bytes/s (0 unlimited)\nLocal feature masks: %lu / %lu (rate, skip, disk verification negotiated)\nReceive file limit %lu B; local filename %u bytes\nPeer: %.63s at %.47s:%u\nNegotiation: %s / peer masks %lu / %lu\nLast status: %.255s\nLast error: %.255s\nNo peers? Check same LAN, UDP broadcasts, firewall and NAT/router settings. Use the host LAN address, not 127.0.0.1, from another computer.",c->config.platform,c->check_result[0]?c->check_result:"Use Test connection (no file is sent)",stack,c->config.name,c->config.bind_address,local,c->config.port,c->config.discovery_port?c->config.discovery_port:c->config.port,c->config.visible?"yes":"no",c->config.receiving?"yes":"no",c->peer_count,c->security_mode==2?"Force":c->security_mode==1?"Prefer":"Disabled",bs_transfer_encrypted(c)?"encrypted":"plaintext",c->send_limit,c->receive_limit,caps.send_features,caps.receive_features,caps.max_receive_file,caps.local_name_bytes,c->status.peer_name,c->status.peer_address,c->remote_port,c->cap_ready?"profile received":"legacy/none",c->remote_caps.send_features,c->remote_caps.receive_features,c->status.message,c->error);if(bs_run_summary(c,0,report,sizeof report) && strlen(text)+strlen(report)+2<sizeof text){strcat(text,"\n");strcat(text,report);}if(bs_run_summary(c,1,report,sizeof report) && strlen(text)+strlen(report)+2<sizeof text){strcat(text,"\n");strcat(text,report);}sprintf(report,"\nDiscovery: %s; %lu probes sent; %lu valid peer packets received",c->discovery_paused?"paused":"searching",c->discovery_sent,c->discovery_received);
    copystr(text+strlen(text),report,sizeof text-strlen(text));
    if(c->discovery_received){sprintf(report,"\nLast peer packet: %lu seconds ago",bsp_seconds()-c->discovery_last);copystr(text+strlen(text),report,sizeof text-strlen(text));}
    if(!c->peer_count){copystr(text+strlen(text),"\nNo peers: check guest Wi-Fi/client isolation and VPN routing; try a direct IPv4 connection.",sizeof text-strlen(text));}
    if(c->cap_ready){sprintf(report,"\nPeer compatibility: %s; %s; %s",c->remote_caps.receive_features&BS_CAP_DIRECTORIES?"folders supported":"files only",c->remote_caps.receive_features&BS_CAP_DISK_VERIFY?"disk verification":"disk verification unconfirmed",c->remote_caps.receive_features&BS_CAP_TLS?"TLS supported":"plaintext only");copystr(text+strlen(text),report,sizeof text-strlen(text));}
    copystr(out,text,capacity);return strlen(text)<capacity;
}

/* history_index -1 selects diagnostics; all paging stays bounded. */
int bs_tools_line(const BSClient *c,int index,int line,char *out,size_t capacity){char text[2048];size_t at=0,n,start,width;int row=0;
    if(!out || capacity<2 || line<0)return 0;
    out[0]=0;
    if(index==-2)copystr(text,"This history entry has expired.",sizeof text);else if(index<0)bs_diagnostics(c,text,sizeof text);else if(!bs_history_text(c,index,text,sizeof text)){copystr(text,"No transfer history.",sizeof text);}
    width=capacity-1;
    while(text[at]){start=at;n=0;while(text[at] && text[at]!='\n' && n<width){at++;n++;}if(text[at]=='\n')at++;
        if(row++==line){memcpy(out,text+start,n);out[n]=0;return 1;}}
    return 0;
}

/* A UI polls after bs_tick; announce only the last successfully delivered item. */
int bs_completion_notice(const BSClient *c,unsigned long *cursor){const BSHistoryItem *h;int slot;if(!c || !cursor || !c->history_count)return 0;slot=(c->history_next+BS_HISTORY_MAX-1)%BS_HISTORY_MAX;h=&c->history[slot];if(h->serial==*cursor)return 0;*cursor=h->serial;if(h->result!=BS_RESULT_COMPLETE)return 0;if(h->incoming)return !bs_batch_count(c) || bs_batch_index(c)+1==bs_batch_count(c);return !c->queue_count;}
