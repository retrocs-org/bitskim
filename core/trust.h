#ifdef BS_OPENSSL
static int trust_path(const BSClient *c,char *out){int i;size_t n;if(!c->trust_directory[0] || strlen(c->remote_id)!=32)return 0;for(i=0;i<32;i++)if(!isxdigit((unsigned char)c->remote_id[i]))return 0;n=strlen(c->trust_directory);strcpy(out,c->trust_directory);if(out[n-1]!='/' && out[n-1]!='\\')strcat(out,"/");strcat(out,c->remote_id);strcat(out,".pin");return 1;}
int bs_trust_configure(BSClient *c,const char *directory){char path[BS_PATH_MAX+1];size_t n;if(!c || !directory || !*directory || c->status.active || c->queue_count || strlen(directory)>BS_PATH_MAX-50)return 0;
    n=strlen(directory);strcpy(path,directory);if(path[n-1]!='/' && path[n-1]!='\\')strcat(path,"/");strcat(path,"identity.pem");if(!bsp_tls_identity(path))return 0;copystr(c->trust_directory,directory,sizeof c->trust_directory);bs_refresh(c);return 1;
}
int bs_pairing_code(const BSClient *c,char out[20]){return c && out && (c->remote_features&BS_CAP_DEVICE_TRUST) && (local_features(c)&BS_CAP_DEVICE_TRUST)?bsp_tls_code(c->sock,out):0;}
int bs_trust_status(const BSClient *c){char path[BS_PATH_MAX+1];unsigned char saved[32],pin[32];unsigned long size;FILE *f;int okay;
    if(!c || !trust_path(c,path))return 0;
    if(!bsp_size(path,&size))return 0;
    if(size!=32 || !bs_transfer_encrypted(c) || !bsp_tls_peer_pin(c->sock,pin))return -1;
    f=bsp_open_file(path,"rb");if(!f)return -1;okay=fread(saved,1,32,f)==32;fclose(f);return okay && !CRYPTO_memcmp(saved,pin,32)?1:-1;
}
int bs_trust_current(BSClient *c,const char *code){char expected[20],path[BS_PATH_MAX+1];unsigned char pin[32];FILE *f;int okay;
    if(!c || !code || !c->status.active || !bs_pairing_code(c,expected) || strlen(code)!=strlen(expected) || CRYPTO_memcmp(code,expected,strlen(expected)) || !trust_path(c,path) || !bsp_tls_peer_pin(c->sock,pin)){if(c)copystr(c->error,"Compare the full code on both screens during the encrypted connection",256);return 0;}
    if(bs_trust_status(c)==1)return 1;
    f=bsp_create_file(path);if(!f){copystr(c->error,"Cannot save trust pin; existing pins are never overwritten",256);return 0;}
    okay=fwrite(pin,1,32,f)==32;if(fclose(f))okay=0;if(!okay){bsp_remove_file(path);copystr(c->error,"Could not save device trust",256);}return okay;
}
int bs_forget_current(BSClient *c){char path[BS_PATH_MAX+1];return c && trust_path(c,path)?bsp_remove_file(path)==0:0;}
static int trust_check(BSClient *c){if(bs_trust_status(c)<0){finish_error(c,FRAME_ERROR,"Trusted device identity changed or encryption is unavailable; verify the device before forgetting its saved identity");return 0;}return 1;}
#else
int bs_trust_configure(BSClient *c,const char *d){(void)c;(void)d;return 0;}
int bs_pairing_code(const BSClient *c,char out[20]){(void)c;(void)out;return 0;}
int bs_trust_current(BSClient *c,const char *code){(void)c;(void)code;return 0;}
int bs_trust_status(const BSClient *c){(void)c;return 0;}
int bs_forget_current(BSClient *c){(void)c;return 0;}
static int trust_check(BSClient *c){(void)c;return 1;}
#endif
