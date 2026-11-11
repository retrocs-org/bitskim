#ifndef BITSKIM_TLS_API_H
#define BITSKIM_TLS_API_H
#ifdef BS_OPENSSL
int bsp_network_wouldblock(void);
int bsp_tls_attach(BSSocket,int server,int mode,int encrypted,const unsigned char *key);
int bsp_tls_encrypted(BSSocket);
int bsp_tls_upgrade(BSSocket);
const char *bsp_tls_error(BSSocket);
int bsp_tls_identity(const char *path);
int bsp_tls_peer_pin(BSSocket,unsigned char out[32]);
int bsp_tls_code(BSSocket,char out[20]);
int bsp_pairing_key(char out[65]);
int bsp_shutdown(BSSocket,int);
#define shutdown bsp_shutdown
#endif
#endif
