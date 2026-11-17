/* Short-lived public invitations: no password, private key or automatic consent. */
static int invitation_address(const char *text){unsigned a,b,c,d;char tail,canonical[48];if(sscanf(text,"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)!=4 || a>255 || b>255 || c>255 || d>255)return 0;
    sprintf(canonical,"%u.%u.%u.%u",a,b,c,d);return !strcmp(text,canonical) && inet_addr(text)!=INADDR_NONE && inet_addr(text)!=0;}
int bs_invitation_parse(const char *text,BSInvitation *out){char data[256],*address,*id,*pin,*expires,*token,*colon,*end;unsigned long value,now=(unsigned long)time(NULL);int i,n;
    if(!text || !out || strlen(text)>=sizeof data || strncmp(text,"bitskim://",10))return 0;
    strcpy(data,text+10);address=data;
    id=strchr(address,'/');if(!id)return 0;*id++=0;pin=strchr(id,'/');if(!pin)return 0;*pin++=0;expires=strchr(pin,'/');if(!expires)return 0;*expires++=0;token=strchr(expires,'/');if(!token)return 0;*token++=0;
    colon=strchr(address,':');if(!colon)return 0;*colon++=0;
    if(!invitation_address(address) || strlen(id)!=32 || strlen(pin)!=64 || strlen(token)!=32 || !*colon || !*expires)return 0;
    for(i=0;colon[i];i++)if(colon[i]<'0'||colon[i]>'9')return 0;
    value=strtoul(colon,&end,10);if(*end || !value || value>65535)return 0;
    memset(out,0,sizeof *out);out->port=(unsigned short)value;
    for(i=0;i<32;i++)if(hexval(id[i])<0)return 0;
    for(i=0;i<32;i++){int a=hexval(pin[i*2]),b=hexval(pin[i*2+1]);if(a<0 || b<0)return 0;
    out->pin[i]=(unsigned char)((a<<4)|b);}
    for(i=0;i<16;i++){int a=hexval(token[i*2]),b=hexval(token[i*2+1]);if(a<0 || b<0)return 0;out->token[i]=(unsigned char)((a<<4)|b);}
    n=(int)strlen(expires);if(n<1 || n>10)return 0;
    for(i=0;i<n;i++)if(expires[i]<'0'||expires[i]>'9')return 0;
    value=strtoul(expires,&end,10);if(*end || value<now || value-now>600UL)return 0;
    copystr(out->address,address,sizeof out->address);copystr(out->id,id,sizeof out->id);out->expires=value;return 1;
}
int bs_invitation_create(BSClient *c,const char *address,char *out,size_t capacity){
#ifdef BS_OPENSSL
    unsigned char pin[32];char encoded[65],token[33],text[256];static const char hex[]="0123456789abcdef";int i;
    if(!c || !out || !c->config.receiving || !c->trust_directory[0] || c->pairing_set || !c->security_mode || !invitation_address(address) || !bsp_tls_local_pin(pin))return 0;
    if(RAND_bytes(c->issued_token,16)!=1)return 0;
    c->issued_expires=(unsigned long)time(NULL)+300UL;
    for(i=0;i<16;i++){token[i*2]=hex[c->issued_token[i]>>4];token[i*2+1]=hex[c->issued_token[i]&15];}token[32]=0;
    for(i=0;i<32;i++){encoded[i*2]=hex[pin[i]>>4];encoded[i*2+1]=hex[pin[i]&15];}encoded[64]=0;
    sprintf(text,"bitskim://%s:%u/%s/%s/%lu/%s",address,(unsigned)c->config.port,c->config.id,encoded,c->issued_expires,token);
    if(strlen(text)>=capacity)return 0;
    strcpy(out,text);return 1;
#else
    (void)c;(void)address;(void)out;(void)capacity;return 0;
#endif
}
int bs_queue_invitation(BSClient *c,const char *text,const char *path){
#ifdef BS_OPENSSL
    BSInvitation invitation;BSQueued q;long offset;char icon[BS_PATH_MAX+1];int before;
    if(!c || !bs_invitation_parse(text,&invitation)){if(c)copystr(c->error,"Invalid or expired invitation; scan a fresh code",256);return 0;}
    if(!c->security_mode || c->pairing_set){copystr(c->error,"Invitations require certificate encryption; change Security first",256);return 0;}before=c->queue_count;
    if(!bs_queue_add(c,invitation.id,invitation.address,invitation.port,path))return 0;
    if(!queue_read(c,c->queue_count-1,&q))return 0;
    q.invitation_expires=invitation.expires;memcpy(q.invitation_pin,invitation.pin,32);memcpy(q.invitation_token,invitation.token,16);
    offset=(long)(c->queue_head+(unsigned long)c->queue_count-1)*(long)sizeof q;
    if(fseek(c->queue_file,offset,SEEK_SET) || fwrite(&q,sizeof q,1,c->queue_file)!=1 || fflush(c->queue_file)){c->queue_count--;state_mark(c);return 0;}state_mark(c);if(queue_companion(c,path,icon) && !bs_queue_invitation(c,text,icon)){c->queue_count=before;state_mark(c);return 0;}return 1;
#else
    (void)c;(void)text;(void)path;return 0;
#endif
}
static int invitation_send(BSClient *c,const BSQueued *q){
#ifdef BS_OPENSSL
    if(q->invitation_expires<(unsigned long)time(NULL)){copystr(c->error,"Invitation expired; scan a fresh code before retrying",256);return 0;}
    if(!bs_send(c,q->address,q->port,q->path))return 0;
    copystr(c->expected_id,q->id,33);c->invitation_expires=q->invitation_expires;memcpy(c->invitation_pin,q->invitation_pin,32);memcpy(c->invitation_token,q->invitation_token,16);return 1;
#else
    (void)c;(void)q;return 0;
#endif
}
static int invitation_check(BSClient *c){if(!c->invitation_expires)return 1;
#ifdef BS_OPENSSL
    {unsigned char pin[32];if(c->invitation_expires>=(unsigned long)time(NULL) && bs_transfer_encrypted(c) && bsp_tls_peer_pin(c->sock,pin) && !CRYPTO_memcmp(pin,c->invitation_pin,32))return 1;}
#endif
    finish_error(c,FRAME_ERROR,"Invitation identity differs, expired or encryption unavailable");return 0;
}

static int invitation_frame(BSClient *c,const unsigned char *p,int n){
#ifdef BS_OPENSSL
    if(!c->status.incoming || c->state!=GREETING || !c->got_hello || c->invitation_received || !bs_transfer_encrypted(c) || n!=20 || !c->issued_expires || c->issued_expires<(unsigned long)time(NULL) || get32(p)!=c->issued_expires || CRYPTO_memcmp(p+4,c->issued_token,16))return 0;
    c->invitation_received=1;return 1;
#else
    (void)c;(void)p;(void)n;return 0;
#endif
}
