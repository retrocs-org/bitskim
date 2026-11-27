/* Clang libFuzzer exercises production decoders without accepting file writes. */
#include <stdint.h>
#include "../core/bitskim.c"
int LLVMFuzzerTestOneInput(const uint8_t *data,size_t size){
    BSClient *c;BSPeer peer;BSCapabilities caps;BSInvitation invitation;char name[257];size_t at=0,n;int i,type;
    if(size>65536)return 0;
    n=size<256?size:256;memcpy(name,data,n);name[n]=0;bs_safe_name(name);bs_safe_relative(name);bs_invitation_parse(name,&invitation);
    if(size<=512){parse_greeting(data,(int)size,&peer);cap_decode(data,(int)size,&caps);}
    c=(BSClient *)calloc(1,sizeof *c);if(!c)return 0;c->listener=c->udp=c->sock=BS_INVALID_SOCKET;
    for(i=0;i<LINGER_MAX;i++)c->linger[i]=BS_INVALID_SOCKET;
    c->config.receiving=1;c->batch_enabled=1;c->session_enabled=1;reset_status(c,1,"127.0.0.1");c->state=GREETING;
    /* Inputs are sequences of u8 type, u16 big-endian length, payload. */
    while(size-at>=3 && c->status.active){type=data[at];n=((size_t)data[at+1]<<8)|data[at+2];at+=3;if(n>size-at || n>BS_CHUNK)break;handle_frame(c,type,data+at,(int)n);at+=n;}
    bs_destroy(c);return 0;
}
