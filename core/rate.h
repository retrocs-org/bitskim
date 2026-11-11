/* Nonblocking token buckets. Credit is payload bytes, not headers/TLS overhead. */
int bs_limit_parse(const char *text,unsigned long *out) {
    unsigned long n=0;const unsigned char *p=(const unsigned char *)text;
    if(!p || !*p || !out)return 0;
    while(*p){if(*p<'0' || *p>'9' || n>(BS_RATE_KIB_MAX-(unsigned long)(*p-'0'))/10UL)return 0;n=n*10UL+(unsigned long)(*p++-'0');}
    *out=n*1024UL;return 1;
}
int bs_limits_configure(BSClient *c,unsigned long send,unsigned long receive) {
    if(!c)return 0;
    if(send>BS_RATE_MAX || receive>BS_RATE_MAX){copystr(c->error,"Speed limits must be 0..1048575 KiB/s; zero is Unlimited",256);return 0;}
    c->send_limit=send;c->receive_limit=receive;return 1;
}
void bs_limits_get(const BSClient *c,unsigned long *send,unsigned long *receive){if(send)*send=c?c->send_limit:0;if(receive)*receive=c?c->receive_limit:0;}
static unsigned long limits_effective(const BSClient *c){unsigned long rate=c->active_send_limit;if(c->peer_receive_limit && (!rate || c->peer_receive_limit<rate))rate=c->peer_receive_limit;return rate;}
static void limits_reset(BSClient *c){unsigned long now=bsp_millis();memset(&c->upload,0,sizeof c->upload);memset(&c->download,0,sizeof c->download);c->upload.rate=limits_effective(c);c->download.rate=c->active_receive_limit;c->upload.stamp=c->download.stamp=now;}
static void limits_refill(BSBucket *b,unsigned long now){unsigned long elapsed=(now-b->stamp)&0xffffffffUL,grant,part,capacity;
    b->stamp=now;if(!b->rate)return;if(elapsed>1000)elapsed=1000;
    part=(b->rate%1000UL)*elapsed+b->fraction;grant=(b->rate/1000UL)*elapsed+part/1000UL;b->fraction=part%1000UL;
    capacity=b->rate/4UL;if(capacity<BS_CHUNK)capacity=BS_CHUNK;
    if(grant>=capacity-b->credit)b->credit=capacity;else b->credit+=grant;
}
static void limits_tick(BSClient *c){unsigned long now;if(!c->upload.rate && !c->download.rate)return;now=bsp_millis();limits_refill(&c->upload,now);limits_refill(&c->download,now);}
static unsigned long limits_piece(const BSClient *c){unsigned long n=c->upload.rate/10UL,left=c->status.total-c->status.transferred;if(!n)n=1;if(n>BS_CHUNK)n=BS_CHUNK;if(n>left)n=left;return n;}
static unsigned long limits_send(BSClient *c,unsigned long want){if(!c->upload.rate)return want;if(c->upload.credit<limits_piece(c))return 0;if(want>c->upload.credit)want=c->upload.credit;c->upload.credit-=want;return want;}
static int limits_receive(BSClient *c,unsigned long bytes){if(!c->download.rate)return 1;if(c->download.credit<bytes)return 0;c->download.credit-=bytes;return 1;}
static int limits_read_blocked(const BSClient *c){return c->state==RECEIVING && c->download.rate && c->rx_count>=HEADER && get16(c->rx+4)==DATA && get32(c->rx+8)>c->download.credit;}
