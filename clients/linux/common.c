#define _GNU_SOURCE 1
#include "common.h"
#include <openssl/rand.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static gboolean failure(GError **error,const char *text){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_FAILED,text);return FALSE;}
static gboolean private_directory(const char *path,GError **error){struct stat info;
    if(g_mkdir_with_parents(path,0700)<0 || lstat(path,&info)<0)return failure(error,"Cannot create private BitSkim storage");
    if(!S_ISDIR(info.st_mode)||info.st_uid!=getuid()||(info.st_mode&0077))return failure(error,"BitSkim storage must be a user-owned private directory (mode 0700), without a directory symlink");
    return TRUE;
}
static char *xdg(const char *variable,const char *fallback){const char *value=g_getenv(variable);char *base=value&&g_path_is_absolute(value)?g_strdup(value):g_build_filename(g_get_home_dir(),fallback,NULL);char *path=g_build_filename(base,"bitskim",NULL);g_free(base);return path;}
static int pref_int(GKeyFile *key,const char *name,int fallback){GError *error=NULL;int value=g_key_file_get_integer(key,"BitSkim",name,&error);if(error){g_clear_error(&error);return fallback;}return value;}
static char *pref_string(GKeyFile *key,const char *name,const char *fallback){char *value=g_key_file_get_string(key,"BitSkim",name,NULL);return value?value:g_strdup(fallback);}
char *linux_display(const char *text){char *result=g_utf8_make_valid(text?text:"",-1);for(char *p=result;*p;p++)if((unsigned char)*p<32 || *p==127)*p=' ';return result;}
static gboolean store_text(char *dest,size_t capacity,const char *source,GError **error){if(!*source||!g_utf8_validate(source,-1,NULL)||strlen(source)>=capacity)return failure(error,"Name or address is too long or is not valid UTF-8");for(const char *p=source;*p;p++)if((unsigned char)*p<32||*p==127)return failure(error,"Control characters are not allowed in settings");g_strlcpy(dest,source,capacity);return TRUE;}
gboolean linux_save(LinuxClient *app,GError **error){gsize size;char *data;
    g_key_file_set_string(app->preferences,"BitSkim","ID",app->config.id);
    g_key_file_set_string(app->preferences,"BitSkim","Name",app->config.name);
    g_key_file_set_string(app->preferences,"BitSkim","Destination",app->destination);
    g_key_file_set_string(app->preferences,"BitSkim","Theme",app->theme);
    g_key_file_set_integer(app->preferences,"BitSkim","Security",app->security);
    g_key_file_set_integer(app->preferences,"BitSkim","Metadata",app->metadata);
    g_key_file_set_integer(app->preferences,"BitSkim","Visible",app->visible);
    g_key_file_set_integer(app->preferences,"BitSkim","Receiving",app->receiving);
    g_key_file_set_integer(app->preferences,"BitSkim","Notifications",app->notifications);
    data=g_key_file_to_data(app->preferences,&size,error);if(!data)return FALSE;
    gboolean okay=g_file_set_contents_full(app->settings,data,(gssize)size,G_FILE_SET_CONTENTS_CONSISTENT|G_FILE_SET_CONTENTS_DURABLE,0600,error);g_free(data);return okay;
}
gboolean linux_open(LinuxClient *app,const LinuxOptions *options,int receiving,GError **error){char *path=NULL,*value=NULL;struct stat info;gboolean new_settings=FALSE;memset(app,0,sizeof *app);app->lock_fd=-1;
    if(options->profile){if(!g_path_is_absolute(options->profile))return failure(error,"--profile must be an absolute path");if(!private_directory(options->profile,error))return FALSE;app->config_dir=g_build_filename(options->profile,"config",NULL);app->state_dir=g_build_filename(options->profile,"state",NULL);app->data_dir=g_build_filename(options->profile,"data",NULL);}
    else{app->config_dir=xdg("XDG_CONFIG_HOME",".config");app->state_dir=xdg("XDG_STATE_HOME",".local/state");app->data_dir=xdg("XDG_DATA_HOME",".local/share");}
    if(!private_directory(app->config_dir,error)||!private_directory(app->state_dir,error)||!private_directory(app->data_dir,error))goto bad;
    path=g_build_filename(app->state_dir,"client.lock",NULL);app->lock_fd=open(path,O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);g_clear_pointer(&path,g_free);
    if(app->lock_fd<0||fstat(app->lock_fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=getuid()||(info.st_mode&0077)){failure(error,"Cannot open private profile lock");goto bad;}
    if(flock(app->lock_fd,LOCK_EX|LOCK_NB)<0){failure(error,"This BitSkim profile is already open. Close it, or choose another --profile and --port");goto bad;}
    app->preferences=g_key_file_new();app->settings=g_build_filename(app->config_dir,"settings.ini",NULL);
    if(lstat(app->settings,&info)==0){if(!S_ISREG(info.st_mode)||info.st_uid!=getuid()||(info.st_mode&0077)){failure(error,"Unsafe settings file; it must be private and regular");goto bad;}if(!g_key_file_load_from_file(app->preferences,app->settings,G_KEY_FILE_NONE,error))goto bad;}
    else if(errno==ENOENT)new_settings=TRUE;else{failure(error,"Cannot read settings");goto bad;}
    value=pref_string(app->preferences,"ID","");
    if(!*value){unsigned char random[16];if(RAND_bytes(random,sizeof random)!=1){failure(error,"Cannot create a random device identity");goto bad;}for(int i=0;i<16;i++)g_snprintf(app->config.id+i*2,3,"%02x",random[i]);}
    else{if(strlen(value)!=32){failure(error,"Invalid saved device ID; original settings retained");goto bad;}for(int i=0;i<32;i++)if(!g_ascii_isxdigit(value[i])){failure(error,"Invalid saved device ID; original settings retained");goto bad;}g_strlcpy(app->config.id,value,sizeof app->config.id);}g_clear_pointer(&value,g_free);
    value=options->name?g_strdup(options->name):pref_string(app->preferences,"Name",g_get_host_name());if(!store_text(app->config.name,sizeof app->config.name,value,error))goto bad;g_clear_pointer(&value,g_free);
    g_strlcpy(app->config.platform,"Linux",sizeof app->config.platform);if(options->bind&&!store_text(app->config.bind_address,sizeof app->config.bind_address,options->bind,error))goto bad;
    if(options->bind){struct in_addr address;if(inet_pton(AF_INET,options->bind,&address)!=1){failure(error,"--bind requires an IPv4 address");goto bad;}}
    if(options->port<1||options->port>65535){failure(error,"Port must be between 1 and 65535");goto bad;}
    app->config.port=(unsigned short)options->port;app->config.discovery_port=BS_PORT;
    app->visible=pref_int(app->preferences,"Visible",1)!=0;app->receiving=pref_int(app->preferences,"Receiving",1)!=0;
    app->config.visible=options->no_discovery?0:app->visible;app->config.receiving=receiving<0?app->receiving:receiving;
    app->security=pref_int(app->preferences,"Security",1);if(options->security){if(!strcmp(options->security,"off"))app->security=0;else if(!strcmp(options->security,"prefer"))app->security=1;else if(!strcmp(options->security,"require"))app->security=2;else{failure(error,"Security must be off, prefer or require");goto bad;}}
    if(app->security<0||app->security>2){failure(error,"Invalid saved security mode");goto bad;}
    app->notifications=pref_int(app->preferences,"Notifications",1)!=0;
    app->metadata=options->metadata||pref_int(app->preferences,"Metadata",0);app->theme=pref_string(app->preferences,"Theme","mocha");
    const char *downloads=g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);char *fallback=downloads?g_strdup(downloads):g_build_filename(g_get_home_dir(),"Downloads",NULL);
    app->destination=options->destination?g_strdup(options->destination):pref_string(app->preferences,"Destination",fallback);g_free(fallback);
    if(!g_path_is_absolute(app->destination)||strlen(app->destination)>BS_PATH_MAX){failure(error,"Receive folder must be an absolute path within the engine path limit");goto bad;}
    if(new_settings&&!linux_save(app,error))goto bad;
    if(!bs_platform_init()||!(app->engine=(options->no_discovery?bs_create_manual:bs_create)(&app->config,options->callback,options->context))){failure(error,"Cannot listen on the selected TCP/UDP port. Close another BitSkim copy, or use --port");goto bad;}
    path=g_build_filename(app->data_dir,"trust",NULL);if(!private_directory(path,error))goto bad;
    if(!bs_trust_configure(app->engine,path)||!bs_security_configure(app->engine,app->security,"")){failure(error,"Cannot initialize the saved TLS device identity");goto bad;}g_clear_pointer(&path,g_free);
    path=g_build_filename(app->state_dir,"transfers.dat",NULL);bs_state_configure(app->engine,path);g_clear_pointer(&path,g_free);
    path=g_build_filename(app->state_dir,"folders.dat",NULL);bs_folders_configure(app->engine,path);g_clear_pointer(&path,g_free);
    bs_batch_enable(app->engine,1);bs_resume_enable(app->engine,1);bs_metadata_enable(app->engine,app->metadata);bs_conflict_policy(app->engine,BS_CONFLICT_RENAME);
    unsigned long up=(unsigned long)g_key_file_get_uint64(app->preferences,"BitSkim","SendLimit",NULL),down=(unsigned long)g_key_file_get_uint64(app->preferences,"BitSkim","ReceiveLimit",NULL);if(up>BS_RATE_MAX||down>BS_RATE_MAX){failure(error,"Invalid saved speed limits");goto bad;}bs_limits_configure(app->engine,up,down);
    if(options->no_discovery)bs_discovery_search(app->engine,0);return TRUE;
bad:g_free(path);g_free(value);linux_close(app);return FALSE;
}
void linux_close(LinuxClient *app){if(app->engine){bs_destroy(app->engine);app->engine=NULL;bs_platform_cleanup();}if(app->lock_fd>=0){close(app->lock_fd);app->lock_fd=-1;}g_clear_pointer(&app->preferences,g_key_file_unref);g_free(app->config_dir);g_free(app->state_dir);g_free(app->data_dir);g_free(app->settings);g_free(app->destination);g_free(app->theme);memset(app,0,sizeof *app);app->lock_fd=-1;}
gboolean linux_target(const char *target,char address[48],unsigned short *port,GError **error){char **parts=g_strsplit(target,":",-1);struct in_addr parsed;gboolean okay=FALSE;*port=BS_PORT;
    if(g_strv_length(parts)>2||strlen(parts[0])>=48||inet_pton(AF_INET,parts[0],&parsed)!=1||parsed.s_addr==INADDR_ANY||parsed.s_addr==INADDR_BROADCAST){failure(error,"Enter an IPv4 address, optionally followed by :port");goto done;}
    if(parts[1]){for(const char *p=parts[1];*p;p++)if(!g_ascii_isdigit(*p)){failure(error,"Invalid target port");goto done;}char *end;guint64 value=g_ascii_strtoull(parts[1],&end,10);if(!*parts[1]||*end||value<1||value>65535){failure(error,"Invalid target port");goto done;}*port=(unsigned short)value;}
    g_strlcpy(address,parts[0],48);okay=TRUE;
done:g_strfreev(parts);return okay;
}
gboolean linux_enqueue(LinuxClient *app,const char *peer,const char *address,unsigned short port,const char *invitation,GPtrArray *paths,GError **error){int before=bs_queue_count(app->engine);
    if(!paths->len)return failure(error,"Choose files or a folder first");
    if(before||bs_status(app->engine)->active)return failure(error,"Finish, continue or clear the existing queue before starting another send");
    bs_queue_pause(app->engine);
    for(guint i=0;i<paths->len;i++){const char *path=g_ptr_array_index(paths,i);int okay=invitation?bs_queue_invitation(app->engine,invitation,path):peer&&*peer?(address&&*address?bs_queue_peer_address(app->engine,peer,address,port,path):bs_queue_peer(app->engine,peer,path)):bs_queue_address(app->engine,address,port,path);
        if(!okay){bs_queue_clear(app->engine);return failure(error,bs_user_error(app->engine));}}
    bs_queue_continue(app->engine);return TRUE;
}
gboolean linux_accept(LinuxClient *app,const char *folder,gboolean allow_unknown,GError **error){const BSStatus *status=bs_status(app->engine);char path[BS_PATH_MAX+1];BSStorage storage;
    if(!status->awaiting_decision)return failure(error,"This offer has expired");
    if(!g_file_test(folder,G_FILE_TEST_IS_DIR))return failure(error,"Choose an existing destination folder");
    if(bs_batch_count(app->engine)>0){if(!bs_storage_info(app->engine,folder,1,&storage))return failure(error,"Cannot check destination storage");if((storage.known&&!storage.enough)||(!storage.known&&!allow_unknown))return failure(error,storage.known?"Insufficient destination storage":"Destination storage is unknown; review before accepting");return bs_accept_batch(app->engine,folder)?TRUE:failure(error,bs_user_error(app->engine));}
    if(!bs_suggest_destination(folder,status->filename,0,path,sizeof path))return failure(error,"Cannot choose a safe destination filename");
    if(!bs_destination_space_info(app->engine,path,0,&storage))return failure(error,"Cannot check destination storage");if((storage.known&&!storage.enough)||(!storage.known&&!allow_unknown))return failure(error,storage.known?"Insufficient destination storage":"Destination storage is unknown; review before accepting");
    return bs_accept_resumable(app->engine,path,0)?TRUE:failure(error,bs_user_error(app->engine));
}
void linux_json_string(const char *value){char *valid=g_utf8_make_valid(value?value:"",-1);putchar('"');for(const unsigned char *p=(const unsigned char *)valid;*p;p++){if(*p=='"'||*p=='\\'){putchar('\\');putchar(*p);}else if(*p<32||*p==127)printf("\\u%04x",*p);else putchar(*p);}putchar('"');g_free(valid);}

gboolean linux_resume_folder(LinuxClient *app,const char *folder,GError **error){return bs_accept_batch_resume(app->engine,folder)?TRUE:failure(error,bs_user_error(app->engine));}
