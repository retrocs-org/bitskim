#include <jni.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "bitskim.h"
#include "platform.h"

typedef struct { BSClient *client;char received[BS_PATH_MAX+1];int discovery; } AndroidClient;
typedef struct { char data[65536];size_t used;int failed; } Json;
static void put(Json *j,const char *format,...) {int n;va_list args;if(j->failed)return;va_start(args,format);n=vsnprintf(j->data+j->used,sizeof(j->data)-j->used,format,args);va_end(args);if(n<0 || (size_t)n>=sizeof(j->data)-j->used){j->failed=1;return;}j->used+=(size_t)n;}
static void quote(Json *j,const char *text) {const unsigned char *p=(const unsigned char *)text;put(j,"\"");for(;*p;p++){if(*p=='"' || *p=='\\')put(j,"\\%c",*p);else if(*p<32)put(j,"\\u%04x",*p);else put(j,"%c",*p);}put(j,"\"");}
static int java_text(JNIEnv *env,jstring value,char *out,size_t capacity) {
 jclass cls;jmethodID method;jbyteArray bytes;jsize n;jstring encoding;
 if(!value || !capacity)return 0;cls=(*env)->GetObjectClass(env,value);method=(*env)->GetMethodID(env,cls,"getBytes","(Ljava/lang/String;)[B");encoding=(*env)->NewStringUTF(env,"UTF-8");bytes=(jbyteArray)(*env)->CallObjectMethod(env,value,method,encoding);(*env)->DeleteLocalRef(env,encoding);(*env)->DeleteLocalRef(env,cls);if((*env)->ExceptionCheck(env) || !bytes)return 0;
 n=(*env)->GetArrayLength(env,bytes);if(n<0 || (size_t)n>=capacity){(*env)->DeleteLocalRef(env,bytes);return 0;}(*env)->GetByteArrayRegion(env,bytes,0,n,(jbyte *)out);(*env)->DeleteLocalRef(env,bytes);if(memchr(out,0,(size_t)n))return 0;out[n]=0;return 1;
}
static jstring string(JNIEnv *env,const char *text) {
 jclass cls=(*env)->FindClass(env,"java/lang/String");jmethodID ctor=(*env)->GetMethodID(env,cls,"<init>","([BLjava/lang/String;)V");jsize n=(jsize)strlen(text);jbyteArray bytes=(*env)->NewByteArray(env,n);jstring encoding,result;(*env)->SetByteArrayRegion(env,bytes,0,n,(const jbyte *)text);encoding=(*env)->NewStringUTF(env,"UTF-8");result=(jstring)(*env)->NewObject(env,cls,ctor,bytes,encoding);(*env)->DeleteLocalRef(env,encoding);(*env)->DeleteLocalRef(env,bytes);(*env)->DeleteLocalRef(env,cls);return result;
}
static void error(JNIEnv *env,const char *message) {jclass type=(*env)->FindClass(env,"java/lang/IllegalStateException");if(type)(*env)->ThrowNew(env,type,message);}
static AndroidClient *handle(jlong h) {return (AndroidClient *)(uintptr_t)h;}
JNIEXPORT jlong JNICALL Java_org_bitskim_android_Native_create(JNIEnv *env,jclass cls,jstring name,jstring id,jstring trust,jstring received,jint port,jboolean discovery,jint security) {
 AndroidClient *a;BSConfig config;char directory[BS_PATH_MAX+1];(void)cls;memset(&config,0,sizeof(config));a=(AndroidClient *)calloc(1,sizeof(*a));if(!a){error(env,"Out of memory");return 0;}
 if(port<1 || port>65535 || !java_text(env,name,config.name,sizeof(config.name)) || !java_text(env,id,config.id,sizeof(config.id)) || !java_text(env,trust,directory,sizeof(directory)) || !java_text(env,received,a->received,sizeof(a->received))){free(a);error(env,"Invalid Android client configuration");return 0;}
 strcpy(config.platform,"Android");config.port=(unsigned short)port;config.discovery_port=BS_PORT;config.visible=discovery?1:0;config.receiving=1;a->discovery=discovery?1:0;
 if(!bs_platform_init() || !(a->client=bs_create(&config,NULL,NULL))){free(a);error(env,"Cannot open the local network listener");return 0;}
 if(!bs_trust_configure(a->client,directory) || !bs_security_configure(a->client,security,"")){bs_destroy(a->client);free(a);error(env,"Cannot initialize encryption or device identity storage");return 0;}
 bs_batch_enable(a->client,1);bs_resume_enable(a->client,0);bs_conflict_policy(a->client,BS_CONFLICT_RENAME);return (jlong)(uintptr_t)a;
}
JNIEXPORT void JNICALL Java_org_bitskim_android_Native_destroy(JNIEnv *env,jclass cls,jlong h) {AndroidClient *a=handle(h);(void)env;(void)cls;if(a){bs_cancel(a->client);bs_destroy(a->client);free(a);bs_platform_cleanup();}}
JNIEXPORT jboolean JNICALL Java_org_bitskim_android_Native_action(JNIEnv *env,jclass cls,jlong h,jint cmd,jlong serial,jstring argument) {
 AndroidClient *a=handle(h);char text[BS_PATH_MAX+1],dest[BS_PATH_MAX+1];const BSStatus *s;(void)cls;if(!a || !java_text(env,argument,text,sizeof(text)))return JNI_FALSE;s=bs_status(a->client);
 switch(cmd){
 case 1:bs_refresh(a->client);return JNI_TRUE;
 case 2:bs_cancel(a->client);return JNI_TRUE;
 case 3:if(!s->awaiting_decision || bs_transfer_serial(a->client)!=(unsigned long)serial)return JNI_FALSE;if(bs_batch_count(a->client)>0)return bs_accept_batch(a->client,a->received)?JNI_TRUE:JNI_FALSE;if(!bs_suggest_destination(a->received,s->filename,0,dest,sizeof(dest)))return JNI_FALSE;return bs_accept(a->client,dest)?JNI_TRUE:JNI_FALSE;
 case 4:if(s->awaiting_decision && bs_transfer_serial(a->client)==(unsigned long)serial){bs_decline(a->client);return JNI_TRUE;}return JNI_FALSE;
 case 5:return bs_trust_current(a->client,text)?JNI_TRUE:JNI_FALSE;
 case 6:return bs_security_configure(a->client,(int)serial,"")?JNI_TRUE:JNI_FALSE;
 case 7:if(s->active)return JNI_FALSE;bs_configure(a->client,NULL,a->discovery,(int)serial);return JNI_TRUE;
 case 8:bs_queue_continue(a->client);return JNI_TRUE;
 case 9:bs_service(a->client,4);return JNI_TRUE;
 }return JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_org_bitskim_android_Native_queue(JNIEnv *env,jclass cls,jlong h,jstring peer,jstring ip,jint port,jstring path) {
 AndroidClient *a=handle(h);char id[33],address[48],file[BS_PATH_MAX+1];(void)cls;if(!a || port<1 || port>65535 || !java_text(env,peer,id,sizeof(id)) || !java_text(env,ip,address,sizeof(address)) || !java_text(env,path,file,sizeof(file)))return JNI_FALSE;
 return (*id?bs_queue_peer_address(a->client,id,address,(unsigned short)port,file):bs_queue_address(a->client,address,(unsigned short)port,file))?JNI_TRUE:JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_org_bitskim_android_Native_tick(JNIEnv *env,jclass cls,jlong h) {
 AndroidClient *a=handle(h);Json *j;const BSStatus *s;char report[8192],code[20];unsigned long position=0,count=0;int i,n; jstring result;(void)cls;if(!a)return string(env,"{}");bs_service(a->client,4);s=bs_status(a->client);j=(Json *)calloc(1,sizeof(*j));if(!j){error(env,"Out of memory");return NULL;}
 bs_queue_progress(a->client,&position,&count);put(j,"{\"active\":%s,\"incoming\":%s,\"awaiting\":%s,\"serial\":%lu,\"total\":%lu,\"done\":%lu,\"rate\":%lu,\"position\":%lu,\"count\":%lu,\"queued\":%d,\"paused\":%s,\"encrypted\":%s,\"trusted\":%s,\"filename\":",s->active?"true":"false",s->incoming?"true":"false",s->awaiting_decision?"true":"false",bs_transfer_serial(a->client),s->total,s->transferred,bs_transfer_rate(a->client),position,count,bs_queue_count(a->client),bs_queue_paused(a->client)?"true":"false",bs_transfer_encrypted(a->client)?"true":"false",bs_trust_status(a->client)==1?"true":"false");quote(j,s->filename);put(j,",\"status\":");quote(j,s->message);put(j,",\"peer\":");quote(j,s->peer_name);put(j,",\"error\":");quote(j,bs_last_error(a->client));code[0]=0;bs_pairing_code(a->client,code);put(j,",\"code\":");quote(j,code);
 bs_progress_summary(a->client,report,sizeof(report));put(j,",\"summary\":");quote(j,report);bs_diagnostics(a->client,report,sizeof(report));put(j,",\"diagnostics\":");quote(j,report);
 put(j,",\"peers\":[");n=bs_peer_count(a->client);for(i=0;i<n;i++){const BSPeer *p=bs_peer(a->client,i);if(i)put(j,",");put(j,"{\"id\":");quote(j,p->id);put(j,",\"name\":");quote(j,p->name);put(j,",\"platform\":");quote(j,p->platform);put(j,",\"address\":");quote(j,p->address);put(j,",\"port\":%u}",(unsigned)p->port);}put(j,"],\"history\":[");n=bs_history_count(a->client);for(i=0;i<n;i++){if(i)put(j,",");bs_history_text(a->client,i,report,sizeof(report));quote(j,report);}put(j,"],\"manifestCount\":%d,\"manifest\":[",bs_batch_count(a->client));n=bs_batch_count(a->client);for(i=0;i<n && i<20;i++){const BSBatchItem *item=bs_batch_item(a->client,i);if(i)put(j,",");quote(j,item?item->filename:"");}put(j,"]}");result=string(env,j->failed?"{\"error\":\"Report exceeds display capacity\"}":j->data);free(j);return result;
}
static unsigned short free_port(void) {int fd=socket(AF_INET,SOCK_STREAM,0);struct sockaddr_in addr;socklen_t size=sizeof(addr);memset(&addr,0,sizeof(addr));addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);if(fd<0 || bind(fd,(struct sockaddr *)&addr,sizeof(addr))){if(fd>=0)close(fd);return 0;}if(getsockname(fd,(struct sockaddr *)&addr,&size)){close(fd);return 0;}close(fd);return ntohs(addr.sin_port);}
JNIEXPORT jstring JNICALL Java_org_bitskim_android_Native_selfTest(JNIEnv *env,jclass cls,jstring folder) {
 char root[BS_PATH_MAX+1],source[BS_PATH_MAX+1],dest[BS_PATH_MAX+1],readback[64];BSConfig ac,bc;BSClient *a=NULL,*b=NULL;FILE *f;unsigned long start;int accepted=0,ok=0;const char bytes[]="Android JNI Unicode and CRC32 check\n";const char *failure="FAIL native setup";(void)cls;
 if(!java_text(env,folder,root,sizeof(root)) || strlen(root)>BS_PATH_MAX-40)return string(env,failure);
 snprintf(source,sizeof(source),"%s/Möte.txt",root);snprintf(dest,sizeof(dest),"%s/received.txt",root);f=fopen(source,"wb");if(!f)return string(env,failure);fwrite(bytes,1,sizeof(bytes)-1,f);fclose(f);
 memset(&ac,0,sizeof(ac));memset(&bc,0,sizeof(bc));strcpy(ac.id,"11111111111111111111111111111111");strcpy(bc.id,"22222222222222222222222222222222");strcpy(ac.name,"Android test sender");strcpy(bc.name,"Android test receiver");ac.port=free_port();bc.port=free_port();ac.receiving=bc.receiving=1;ac.discovery_port=bc.discovery_port=BS_PORT;
 if(!bs_platform_init() || !ac.port || !bc.port || !(a=bs_create(&ac,NULL,NULL)) || !(b=bs_create(&bc,NULL,NULL)))goto done;
 failure="FAIL queue send";if(!bs_send(a,"127.0.0.1",bc.port,source))goto done;start=bsp_millis();failure="FAIL transfer timeout";
 while((bsp_millis()-start)<10000UL){bs_service(a,4);bs_service(b,4);if(bs_status(b)->awaiting_decision && !accepted){accepted=bs_accept(b,dest);if(!accepted){failure="FAIL acceptance";goto done;}}if(accepted && !bs_status(a)->active && !bs_status(b)->active)break;usleep(1000);}
 failure="FAIL receipt verification";if(bs_transfer_result(a)!=BS_RESULT_COMPLETE || bs_transfer_result(b)!=BS_RESULT_COMPLETE)goto done;f=fopen(dest,"rb");if(!f)goto done;memset(readback,0,sizeof(readback));ok=fread(readback,1,sizeof(readback),f)==sizeof(bytes)-1 && !memcmp(readback,bytes,sizeof(bytes)-1);fclose(f);if(ok){BSHistoryItem item;ok=bs_history_item(b,0,&item) && item.verified && item.encrypted;}
 done:if(a)bs_destroy(a);if(b)bs_destroy(b);return string(env,ok?"PASS Android native TLS transfer, Unicode filename and written-file CRC32 verification":failure);
}
