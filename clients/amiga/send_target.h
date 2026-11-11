/* A GUI send retains an advertised identity or an explicitly entered address,
   never a catalog index or a fallback from a disappeared peer to another target. */
typedef struct { char id[33],address[48]; unsigned short port; } GuiSendTarget;
static int gui_target(GuiSendTarget *t,const char *id,const char *manual) {
    char *colon,*end; unsigned long port;
    memset(t,0,sizeof(*t)); t->port=BS_PORT;
    if(id && id[0]) { if(strlen(id)!=32) return 0; strcpy(t->id,id); return 1; }
    if(!manual || !manual[0] || strlen(manual)>=sizeof(t->address)) return 0;
    strcpy(t->address,manual); colon=strchr(t->address,':');
    if(colon) { *colon++=0; if(!*colon) return 0; port=strtoul(colon,&end,10); if(*end || port<1 || port>65535) return 0; t->port=(unsigned short)port; }
    return t->address[0]!=0;
}
static int gui_send(BSClient *engine,const GuiSendTarget *t,const char *path) {
    return t->id[0]?bs_queue_peer(engine,t->id,path):bs_queue_address(engine,t->address,t->port,path);
}
