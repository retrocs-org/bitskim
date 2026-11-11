/* Fixed-size, versioned profiles. Discovery hints never replace live checks. */
void bs_local_capabilities(const BSClient *c,BSCapabilities *out) {
    if(!out)return;
    memset(out,0,sizeof *out);if(!c)return;
    out->send_features=BS_CAP_DIRECTORIES|(BS_CAP_RATE_LIMITS|BS_CAP_SKIP|BS_CAP_DISK_VERIFY)|BS_CAP_DETAILS|BS_CAP_BATCH|BS_CAP_COLLECTION;
#ifdef BS_OPENSSL
    out->send_features|=BS_CAP_TLS;
    if(c->resume_enabled)out->send_features|=BS_CAP_RESUME;
    if(c->security_mode){out->send_features|=BS_CAP_TLS;if(!c->pairing_set){out->send_features|=BS_CAP_AUTO_TLS;if(c->trust_directory[0])out->send_features|=BS_CAP_DEVICE_TRUST;}}
#endif
    out->receive_features=c->config.receiving?local_features(c):0;
#ifdef BS_OPENSSL
    if(c->config.receiving)out->receive_features|=BS_CAP_TLS;
#endif
    out->max_send_file=out->max_receive_file=BS_FILE_MAX;
    out->wire_name_bytes=255;out->local_name_bytes=255;out->path_bytes=BS_PATH_MAX;
    out->security_mode=(unsigned char)c->security_mode;out->paired=(unsigned char)c->pairing_set;
    out->collection_files=c->batch_enabled?BS_QUEUE_MAX:1;
#if defined(__amigaos__)
    out->name_policy=1;out->local_name_bytes=30;
#elif defined(__riscos__)
    out->name_policy=1;out->local_name_bytes=10;
#elif defined(BS_DOS) || defined(BS_ATARI_STING)
    out->name_policy=2;out->local_name_bytes=12;out->path_bytes=127;
#elif defined(BS_ATARI)
    out->name_policy=3;
#elif defined(BS_WINDOWS_LEGACY)
    out->name_policy=4;out->path_bytes=259;out->code_page=(unsigned short)GetACP();
#endif
}
static void cap_encode(const BSClient *c,unsigned char p[36]) {
    BSCapabilities caps;bs_local_capabilities(c,&caps);memset(p,0,36);memcpy(p,"BXC1",4);
    p[4]=1;p[5]=caps.security_mode;p[6]=caps.name_policy;p[7]=caps.paired;
    put32(p+8,caps.send_features);put32(p+12,caps.receive_features);
    put32(p+16,caps.max_send_file);put32(p+20,caps.max_receive_file);
    put16(p+24,caps.wire_name_bytes);put16(p+26,caps.local_name_bytes);put16(p+28,caps.path_bytes);put16(p+30,caps.code_page);
    put32(p+32,caps.collection_files);
}
static int cap_decode(const unsigned char *p,int n,BSCapabilities *out) {
    if(n!=36 || memcmp(p,"BXC1",4) || p[4]!=1 || p[5]>2 || p[6]>4 || p[7]>1 || !get32(p+16) || !get32(p+20) || !get16(p+24) || get16(p+24)>255 || !get16(p+26) || !get16(p+28) || !get32(p+32))return 0;
    memset(out,0,sizeof *out);out->security_mode=p[5];out->name_policy=p[6];out->paired=p[7];
    out->send_features=get32(p+8);out->receive_features=get32(p+12);
    out->max_send_file=get32(p+16);out->max_receive_file=get32(p+20);
    out->wire_name_bytes=get16(p+24);out->local_name_bytes=get16(p+26);out->path_bytes=get16(p+28);out->code_page=get16(p+30);out->collection_files=get32(p+32);
    /* Policy is separate from built-in TLS support. Unknown
       optional bits are retained for future display, never used as commands. */
    if((out->name_policy==4 && !out->code_page) || (out->name_policy!=4 && out->code_page))return 0;
    if(!(out->send_features&BS_CAP_DETAILS) || (out->receive_features && !(out->receive_features&BS_CAP_DETAILS)))return 0;
    if(out->security_mode && (!(out->send_features&BS_CAP_TLS) || (out->receive_features && !(out->receive_features&BS_CAP_TLS))))return 0;
    if(!out->security_mode && ((out->send_features|out->receive_features)&BS_CAP_AUTO_TLS))return 0;
    if(((out->send_features&BS_CAP_AUTO_TLS) && (!(out->send_features&BS_CAP_TLS) || out->paired)) || ((out->receive_features&BS_CAP_AUTO_TLS) && (!(out->receive_features&BS_CAP_TLS) || out->paired)))return 0;
    return 1;
}
int bs_peer_capabilities(const BSClient *c,const char *id,BSCapabilities *out) {
    int i,found=-1;if(!out)return 0;memset(out,0,sizeof *out);if(!c || !id)return 0;
    for(i=0;i<c->peer_count;i++)if(!strcmp(c->peers[i].id,id)){if(found>=0)return 0;found=i;}
    if(found<0 || !c->peer_caps_known[found])return 0;
    *out=c->peer_caps[found];return 1;
}
int bs_transfer_capabilities(const BSClient *c,BSCapabilities *out) {
    if(!out)return 0;
    memset(out,0,sizeof *out);if(!c || !c->cap_ready)return 0;
    *out=c->remote_caps;return 1;
}
