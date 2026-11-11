#define TREE_DEPTH 16
typedef struct {void *handle;char path[BS_PATH_MAX+1],relative[256];} BSScanFrame;
struct BSTreeScan {FILE *file;BSQueued target;BSScanFrame frames[TREE_DEPTH];int depth,count,copying,cursor;};
static int path_join(char *out,size_t cap,const char *folder,const char *leaf){size_t n=strlen(folder),m=strlen(leaf);if(n+m+2>cap)return 0;strcpy(out,folder);if(n && out[n-1]!='/' && out[n-1]!='\\' && out[n-1]!=':')strcat(out,"/");strcat(out,leaf);return 1;}
static void tree_scan_free(BSClient *c){BSTreeScan *scan=c->tree_scan;int i;if(!scan)return;for(i=0;i<scan->depth;i++)if(scan->frames[i].handle)bsp_dir_close(scan->frames[i].handle);if(scan->file)fclose(scan->file);free(scan);c->tree_scan=NULL;}
int bs_queue_scanning(const BSClient *c){return c && (c->tree_scan!=NULL || c->retry!=NULL);}
static int tree_scan_append(BSClient *c,const char *path,const char *relative,int kind){BSTreeScan *s=c->tree_scan;BSQueued q=s->target;if(s->count+c->queue_count>BS_QUEUE_MAX || !bs_safe_relative(relative) || strlen(relative)>255 || strlen(path)>BS_PATH_MAX)return 0;q.kind=kind;if(s->count)q.report=0;copystr(q.path,path,sizeof q.path);copystr(q.relative,relative,sizeof q.relative);q.total=0;if(kind==0 && !bsp_size(path,&q.total))return 0;if(fwrite(&q,sizeof q,1,s->file)!=1)return 0;s->count++;return 1;}
static int tree_scan_start(BSClient *c,const BSQueued *target){BSTreeScan *s;char wire[256];size_t n;if(!wire_filename(target->path,wire) || strlen(wire)>253 || bsp_path_kind(target->path)!=2)return 0;
    s=(BSTreeScan *)calloc(1,sizeof *s);if(!s)return 0;c->tree_scan=s;s->target=*target;s->file=bsp_temp_file();if(!s->file)return 0;
    copystr(s->frames[0].path,target->path,sizeof s->frames[0].path);copystr(s->frames[0].relative,wire,256);n=strlen(wire);s->frames[0].relative[n]='/';s->frames[0].relative[n+1]=0;
    s->frames[0].handle=bsp_dir_open(target->path);if(!s->frames[0].handle)return 0;s->depth=1;
    return tree_scan_append(c,target->path,s->frames[0].relative,1);
}
static int tree_scan_tick(BSClient *c,const BSQueued *target){BSTreeScan *s;int budget=8;char name[256],wire[256],path[BS_PATH_MAX+1],relative[256];path[0]=0;
    if(!c->tree_scan && !tree_scan_start(c,target))goto failed;
    s=c->tree_scan;
    while(budget--){
        if(s->copying){BSQueued q;
            if(s->cursor<c->queue_count){if(s->count+c->queue_count-1>BS_QUEUE_MAX || !queue_read(c,s->cursor++,&q) || fwrite(&q,sizeof q,1,s->file)!=1)goto failed;continue;}
            if(fflush(s->file))goto failed;
            fclose(c->queue_file);c->queue_file=s->file;s->file=NULL;c->queue_head=0;c->queue_count=s->count+c->queue_count-1;
            batch_free(c->queue_batch);c->queue_batch=NULL;c->queue_batch_index=0;tree_scan_free(c);message(c,"Folder listed; preparing complete preview...");return 1;
        }
        if(!s->depth){s->copying=1;s->cursor=1;continue;}
        {BSScanFrame *f=&s->frames[s->depth-1];int next=bsp_dir_next(f->handle,name,sizeof name),kind;size_t len;
            if(next<0)goto failed;
            if(!next){bsp_dir_close(f->handle);f->handle=NULL;s->depth--;continue;}
            if(!bs_to_utf8(name,wire,sizeof wire) || !bs_safe_name(wire) || !path_join(path,sizeof path,f->path,name))goto failed;
            len=strlen(f->relative);if(len+strlen(wire)+2>sizeof relative)goto failed;strcpy(relative,f->relative);strcat(relative,wire);
            kind=bsp_path_kind(path);if(kind!=1 && kind!=2)goto failed;
            if(kind==2){if(s->depth>=TREE_DEPTH)goto failed;strcat(relative,"/");if(!tree_scan_append(c,path,relative,1))goto failed;
                f=&s->frames[s->depth];copystr(f->path,path,sizeof f->path);copystr(f->relative,relative,sizeof f->relative);f->handle=bsp_dir_open(path);if(!f->handle)goto failed;s->depth++;
            }else if(!tree_scan_append(c,path,relative,0))goto failed;
        }
    }
    message(c,"Listing folder contents (bounded scan)...");return 0;
failed:tree_scan_free(c);sprintf(c->error,"Cannot list folder near %.100s: check links/special files, read permissions, 16 levels, 255-byte relative paths, queue count and temporary disk space",path[0]?path:target->path);c->queue_paused=1;copystr(c->status.message,c->error,256);notify(c,BS_EVENT_ERROR);return -1;
}
