/* OpenSSL TLS 1.3 external-PSK transport. No application cryptography.
   The BIO retains native BSSocket width; no SOCKET -> int narrowing on Win64. */
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#undef shutdown
#define BS_TLS_SLOTS 24
typedef struct {BSSocket socket;int used,server,mode,want,ready,failed,paired;SSL *ssl;unsigned char key[32];} BSTLS;
static BSTLS tls_slots[BS_TLS_SLOTS];
static BIO_METHOD *tls_bio;
static int tls_blocked,tls_call;
static BSTLS *tls_find(BSSocket socket) {int i;for(i=0;i<BS_TLS_SLOTS;i++)if(tls_slots[i].used && tls_slots[i].socket==socket)return &tls_slots[i];return NULL;}
static int tls_bio_create(BIO *b) {BIO_set_init(b,1);BIO_set_data(b,NULL);return 1;}
static int tls_bio_destroy(BIO *b) {if(b)BIO_set_data(b,NULL);return 1;}
static int tls_bio_read(BIO *b,char *p,int n) {BSTLS *t=(BSTLS *)BIO_get_data(b);int rc;BIO_clear_retry_flags(b);rc=bsp_raw_recv(t->socket,p,n);if(rc<0 && bsp_raw_wouldblock())BIO_set_retry_read(b);return rc;}
static int tls_bio_write(BIO *b,const char *p,int n) {BSTLS *t=(BSTLS *)BIO_get_data(b);int rc;BIO_clear_retry_flags(b);rc=bsp_raw_send(t->socket,p,n);if(rc<0 && bsp_raw_wouldblock())BIO_set_retry_write(b);return rc;}
static long tls_bio_control(BIO *b,int cmd,long number,void *p) {(void)b;(void)number;(void)p;return cmd==BIO_CTRL_FLUSH?1:0;}
static SSL_SESSION *tls_session(SSL *ssl) {
    static const unsigned char cipher_id[2]={0x13,0x01};BSTLS *t=(BSTLS *)SSL_get_app_data(ssl);SSL_SESSION *session=SSL_SESSION_new();
    if(!session)return NULL;
    if(!SSL_SESSION_set1_master_key(session,t->key,32) || !SSL_SESSION_set_cipher(session,SSL_CIPHER_find(ssl,cipher_id)) || !SSL_SESSION_set_protocol_version(session,TLS1_3_VERSION)) {SSL_SESSION_free(session);return NULL;}
    SSL_SESSION_set_max_early_data(session,0);return session;
}
/* Stable PSK protocol identifier: independent of the application version. */
static int tls_client_psk(SSL *ssl,const EVP_MD *md,const unsigned char **id,size_t *length,SSL_SESSION **session) {
    static const unsigned char identity[]="BitSkim-0.1";
    if(md && !EVP_MD_is_a(md,"SHA256")){*session=NULL;return 1;}
    *id=identity;*length=sizeof identity-1;*session=tls_session(ssl);return *session!=NULL;
}
static int tls_server_psk(SSL *ssl,const unsigned char *id,size_t length,SSL_SESSION **session) {
    static const char identity[]="BitSkim-0.1";*session=NULL;
    if(length!=sizeof identity-1 || CRYPTO_memcmp(id,identity,length))return 1;
    *session=tls_session(ssl);return *session!=NULL;
}
/* An unpaired connection encrypts opportunistically using a process-local
   self-signed identity. It does not authenticate a person or installation. */
static EVP_PKEY *tls_identity;
static X509 *tls_certificate;
static char tls_identity_path[BS_PATH_MAX+1];
int bsp_tls_identity(const char *path){if(!path || strlen(path)>BS_PATH_MAX)return 0;if(tls_identity)return !strcmp(path,tls_identity_path);strcpy(tls_identity_path,path);return 1;}
static int tls_accept_certificate(int preverified,X509_STORE_CTX *context){(void)preverified;(void)context;return 1;}

static int tls_no_password(char *buffer,int size,int writing,void *context){(void)buffer;(void)size;(void)writing;(void)context;return 0;}
static int tls_auto_identity(void) {
    EVP_PKEY *key=NULL;X509 *cert=NULL;X509_NAME *name;FILE *file;unsigned long size;int ok;
    if(tls_identity && tls_certificate)return 1;
    if(tls_identity_path[0] && bsp_size(tls_identity_path,&size)){
        if(!size || size>16384UL)return 0;
        file=bsp_open_file(tls_identity_path,"rb");if(!file)return 0;
        key=PEM_read_PrivateKey(file,NULL,tls_no_password,NULL);cert=PEM_read_X509(file,NULL,NULL,NULL);fclose(file);
        if(!key || !cert || !X509_check_private_key(cert,key))goto bad;
        tls_identity=key;tls_certificate=cert;return 1;
    }
    key=EVP_PKEY_Q_keygen(NULL,NULL,"EC","prime256v1");cert=X509_new();
    if(!key || !cert)goto bad;
    if(!X509_set_version(cert,2) || !ASN1_INTEGER_set(X509_get_serialNumber(cert),1) ||
       !X509_gmtime_adj(X509_getm_notBefore(cert),-60) || !X509_gmtime_adj(X509_getm_notAfter(cert),31536000L) || !X509_set_pubkey(cert,key))goto bad;
    name=X509_NAME_new();
    if(!name)goto bad;
    ok=X509_NAME_add_entry_by_txt(name,"CN",MBSTRING_ASC,(const unsigned char *)"BitSkim unpaired transport",-1,-1,0) && X509_set_subject_name(cert,name) && X509_set_issuer_name(cert,name);
    X509_NAME_free(name);if(!ok || !X509_sign(cert,key,EVP_sha256()))goto bad;
    if(tls_identity_path[0]){
        file=bsp_create_file(tls_identity_path);if(!file)goto bad;
        ok=PEM_write_PrivateKey(file,key,NULL,NULL,0,NULL,NULL) && PEM_write_X509(file,cert);if(fclose(file))ok=0;
        if(!ok){bsp_remove_file(tls_identity_path);goto bad;}
    }
    tls_identity=key;tls_certificate=cert;return 1;
bad:if(key)EVP_PKEY_free(key);if(cert)X509_free(cert);return 0;
}
static int tls_start(BSTLS *t) {
    SSL_CTX *ctx;BIO *bio;
    if(!tls_bio) {tls_bio=BIO_meth_new(BIO_TYPE_SOURCE_SINK,"BitSkim native nonblocking socket");if(!tls_bio)return 0;
        BIO_meth_set_create(tls_bio,tls_bio_create);BIO_meth_set_destroy(tls_bio,tls_bio_destroy);BIO_meth_set_read(tls_bio,tls_bio_read);BIO_meth_set_write(tls_bio,tls_bio_write);BIO_meth_set_ctrl(tls_bio,tls_bio_control);}
    ctx=SSL_CTX_new(TLS_method());if(!ctx)return 0;
    if(!SSL_CTX_set_min_proto_version(ctx,TLS1_3_VERSION) || !SSL_CTX_set_max_proto_version(ctx,TLS1_3_VERSION) || !SSL_CTX_set_ciphersuites(ctx,"TLS_AES_128_GCM_SHA256")) {SSL_CTX_free(ctx);return 0;}
    SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_OFF);SSL_CTX_set_num_tickets(ctx,0);
    if(t->paired){SSL_CTX_set_psk_use_session_callback(ctx,tls_client_psk);SSL_CTX_set_psk_find_session_callback(ctx,tls_server_psk);}
    else {
        SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,tls_accept_certificate);
        if(!tls_auto_identity() || !SSL_CTX_use_certificate(ctx,tls_certificate) || !SSL_CTX_use_PrivateKey(ctx,tls_identity)){SSL_CTX_free(ctx);return 0;}
    }
    t->ssl=SSL_new(ctx);SSL_CTX_free(ctx);if(!t->ssl)return 0;
    SSL_set_app_data(t->ssl,t);bio=BIO_new(tls_bio);if(!bio)return 0;BIO_set_data(bio,t);SSL_set_bio(t->ssl,bio,bio);
    SSL_set_mode(t->ssl,SSL_MODE_ENABLE_PARTIAL_WRITE|SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    if(t->server)SSL_set_accept_state(t->ssl);else SSL_set_connect_state(t->ssl);return 1;
}
int bsp_tls_attach(BSSocket socket,int server,int mode,int encrypted,const unsigned char *key) {
    BSTLS *t;int i;for(i=0;i<BS_TLS_SLOTS;i++)if(!tls_slots[i].used)break;if(i==BS_TLS_SLOTS)return 0;
    t=&tls_slots[i];memset(t,0,sizeof *t);t->socket=socket;t->used=1;t->server=server;t->mode=mode;t->want=server?(mode?-1:0):encrypted;if(key){memcpy(t->key,key,32);t->paired=1;}return 1;
}
int bsp_tls_upgrade(BSSocket socket) {BSTLS *t=tls_find(socket);if(!t || !t->mode || t->want || t->ssl || t->failed)return 0;t->want=1;return 1;}
static int tls_error(BSTLS *t,int result) {
    int error=SSL_get_error(t->ssl,result);
    if(error==SSL_ERROR_WANT_READ || error==SSL_ERROR_WANT_WRITE){tls_blocked=1;return -1;}
    if(error==SSL_ERROR_ZERO_RETURN)return 0;
    t->failed=1;return -1;
}
static int tls_ready(BSTLS *t) {
    int rc;unsigned char first;
    if(t->failed)return -1;
    if(t->want==-1) {
        rc=recv(t->socket,(char *)&first,1,MSG_PEEK);if(rc<0){if(bsp_raw_wouldblock())tls_blocked=1;return -1;}if(!rc)return 0;
        t->want=first==0x16;
        if(t->want && !t->mode){t->failed=1;return -1;}
    }
    if(!t->want)return 1;
    if(!t->ssl && !tls_start(t)){t->failed=1;return -1;}
    if(!t->ready) {ERR_clear_error();rc=SSL_do_handshake(t->ssl);if(rc!=1)return tls_error(t,rc);t->ready=1;}
    return 1;
}
int bsp_send(BSSocket socket,const void *p,int n) {
    BSTLS *t=tls_find(socket);int rc;tls_blocked=0;tls_call=t!=NULL;
    if(!t)return bsp_raw_send(socket,p,n);
    rc=tls_ready(t);if(rc!=1)return rc;
    if(!t->want){tls_call=0;return bsp_raw_send(socket,p,n);}
    ERR_clear_error();rc=SSL_write(t->ssl,p,n);return rc>0?rc:tls_error(t,rc);
}
int bsp_recv(BSSocket socket,void *p,int n) {
    BSTLS *t=tls_find(socket);int rc;tls_blocked=0;tls_call=t!=NULL;
    if(!t)return bsp_raw_recv(socket,p,n);
    rc=tls_ready(t);if(rc!=1)return rc;
    if(!t->want){tls_call=0;return bsp_raw_recv(socket,p,n);}
    ERR_clear_error();rc=SSL_read(t->ssl,p,n);return rc>0?rc:tls_error(t,rc);
}
int bsp_wouldblock(void) {return tls_call?tls_blocked:bsp_raw_wouldblock();}
int bsp_shutdown(BSSocket socket,int how) {BSTLS *t=tls_find(socket);if(t && t->ssl && t->ready){SSL_shutdown(t->ssl);return 0;}return shutdown(socket,how);}
int bsp_close(BSSocket socket) {BSTLS *t=tls_find(socket);if(t){if(t->ssl){if(t->ready)SSL_shutdown(t->ssl);SSL_free(t->ssl);}OPENSSL_cleanse(t->key,32);memset(t,0,sizeof *t);}return bsp_raw_close(socket);}
int bsp_tls_encrypted(BSSocket socket) {BSTLS *t=tls_find(socket);return t && t->ready && !t->failed;}
const char *bsp_tls_error(BSSocket socket) {BSTLS *t=tls_find(socket);return t && t->failed?"Encrypted connection failed; check the pairing key and encryption settings":"";}
int bsp_pairing_key(char out[65]) {unsigned char key[32];static const char hex[]="0123456789abcdef";int i;if(RAND_bytes(key,32)!=1)return 0;for(i=0;i<32;i++){out[i*2]=hex[key[i]>>4];out[i*2+1]=hex[key[i]&15];}out[64]=0;OPENSSL_cleanse(key,32);return 1;}

int bsp_network_wouldblock(void) {return bsp_raw_wouldblock();}

int bsp_tls_peer_pin(BSSocket socket,unsigned char out[32]){BSTLS *t=tls_find(socket);X509 *cert;unsigned n=0;int ok;
    if(!t || !t->ready || t->failed || t->paired)return 0;
    cert=SSL_get1_peer_certificate(t->ssl);if(!cert)return 0;
    ok=X509_pubkey_digest(cert,EVP_sha256(),out,&n)==1 && n==32;X509_free(cert);return ok;
}
int bsp_tls_code(BSSocket socket,char out[20]){BSTLS *t=tls_find(socket);unsigned char code[6];static const char hex[]="0123456789ABCDEF";int i,j=0;
    if(!t || !t->ready || t->failed || t->paired)return 0;
    if(SSL_export_keying_material(t->ssl,code,sizeof code,"EXPORTER-BitSkim-Pairing-v1",sizeof("EXPORTER-BitSkim-Pairing-v1")-1,NULL,0,0)!=1)return 0;
    for(i=0;i<6;i++){if(i && !(i%2))out[j++]='-';out[j++]=hex[code[i]>>4];out[j++]=hex[code[i]&15];}out[j]=0;OPENSSL_cleanse(code,sizeof code);return 1;
}
