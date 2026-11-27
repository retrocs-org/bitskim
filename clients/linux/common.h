#ifndef BITSKIM_LINUX_COMMON_H
#define BITSKIM_LINUX_COMMON_H
#include <gio/gio.h>
#include "bitskim.h"
typedef struct {
    char *profile,*name,*bind,*security,*destination;
    int port;
    BSCallback callback;
    void *context;
    gboolean metadata,no_discovery;
} LinuxOptions;
typedef struct {
    BSClient *engine;
    BSConfig config;
    GKeyFile *preferences;
    char *config_dir,*state_dir,*data_dir,*settings,*destination,*theme;
    int lock_fd,security;
    gboolean metadata,visible,receiving,notifications;
} LinuxClient;
gboolean linux_open(LinuxClient *,const LinuxOptions *,int receiving,GError **);
void linux_close(LinuxClient *);
gboolean linux_save(LinuxClient *,GError **);
gboolean linux_target(const char *,char address[48],unsigned short *,GError **);
gboolean linux_enqueue(LinuxClient *,const char *peer,const char *address,unsigned short port,const char *invitation,GPtrArray *paths,GError **);
gboolean linux_accept(LinuxClient *,const char *folder,gboolean allow_unknown,GError **);
char *linux_display(const char *);
void linux_json_string(const char *);
gboolean linux_resume_folder(LinuxClient *,const char *,GError **);
#endif
