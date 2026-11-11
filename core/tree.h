#define TREE_LEVELS 16
typedef struct {char wire[256],native[BS_PATH_MAX+1];} BSTreeLevel;
struct BSTreePlan {FILE *paths;BSTreeLevel levels[TREE_LEVELS];int depth,cursor,ready;unsigned long heartbeat;char root[BS_PATH_MAX+1];};
static int path_join(char *,size_t,const char *,const char *);
static int batch_destination(BSClient *,const char *,char *);
static void tree_plan_free(BSTreePlan *p){if(p){if(p->paths)fclose(p->paths);free(p);}}
int bs_safe_relative(const char *path){char leaf[256];size_t i=0,n=0;int depth=0;if(!path || !*path || strlen(path)>255 || path[0]=='/')return 0;
    for(;;){char ch=path[i++];if(ch=='/' || !ch){if(!n || ++depth>TREE_LEVELS)return 0;leaf[n]=0;if(!bs_safe_name(leaf))return 0;n=0;if(!ch || !path[i])break;}else {if(n>=255)return 0;leaf[n++]=ch;}}
    return 1;
}
int bs_batch_is_tree(const BSClient *c){return c && c->in_batch && c->in_batch->tree;}
typedef struct {char prefix[TREE_LEVELS][256];int depth;} BSTreeWire;
static int tree_manifest_item(BSBatch *batch,const BSBatchItem *item,int index){BSTreeWire *v=(BSTreeWire *)batch->wire_tree;char path[256],parent[256],*slash;size_t n;int dir,depth;
    if(!v){v=(BSTreeWire *)calloc(1,sizeof *v);if(!v)return 0;batch->wire_tree=v;}
    copystr(path,item->filename,sizeof path);n=strlen(path);dir=n && path[n-1]=='/';if(dir)path[--n]=0;slash=strrchr(path,'/');
    if(index==0){if(!dir || slash)return 0;copystr(v->prefix[0],item->filename,256);v->depth=1;return 1;}
    if(!slash)return 0;
    n=(size_t)(slash-path)+1;memcpy(parent,path,n);parent[n]=0;for(depth=v->depth-1;depth>=0;depth--)if(!strcmp(v->prefix[depth],parent))break;
    if(depth<0)return 0;
    v->depth=depth+1;if(dir){if(v->depth>=TREE_LEVELS)return 0;copystr(v->prefix[v->depth++],item->filename,256);}return 1;
}
static int tree_directory(const BSBatchItem *item){size_t n=strlen(item->filename);return n && item->filename[n-1]=='/';}
static int tree_parent_safe(const BSTreePlan *plan,const char *path){char prefix[BS_PATH_MAX+1];size_t n=strlen(plan->root),i;if(!n || strncmp(path,plan->root,n) || (path[n] && path[n]!='/') || bsp_path_kind(plan->root)!=2)return 0;copystr(prefix,path,sizeof prefix);for(i=n+1;prefix[i];i++)if(prefix[i]=='/'){prefix[i]=0;if(bsp_path_kind(prefix)!=2)return 0;prefix[i]='/';}return 1;}
static int tree_accept_entry(BSClient *c){BSBatchApproval *a=c->batch_approval;BSTreePlan *plan=a?a->plan:NULL;const BSBatchItem *item=batch_item(c->in_batch,c->in_batch_index);char path[BS_PATH_MAX+1];
    if(!item || !plan || !plan->ready || fseek(plan->paths,(long)c->in_batch_index*(long)sizeof path,SEEK_SET) || fread(path,sizeof path,1,plan->paths)!=1 || !memchr(path,0,sizeof path) || !tree_parent_safe(plan,path)){copystr(c->error,"Folder destination changed or cannot be resolved safely",256);return 0;}
    if(!tree_directory(item))return bs_accept_conflict(c,path);
    if(bsp_path_kind(path)!=2){copystr(c->error,"Receiving directory was removed or replaced",256);return 0;}
    copystr(c->dest,path,sizeof c->dest);c->transfer_result=BS_RESULT_COMPLETE;c->status.awaiting_decision=0;c->verified=0;
    a->next=c->in_batch_index+1;a->last_seen=bsp_seconds();frame(c,COMPLETE,NULL,0);c->state=CLOSING;message(c,"Directory received");notify(c,BS_EVENT_COMPLETE);if(a->next>=a->manifest->count)batch_revoke(c);return 1;
}
static int tree_prepare_start(BSClient *c){BSTreePlan *p;if(bsp_path_kind(c->batch_approval->folder)!=2){copystr(c->error,"Choose an existing folder, not a link",256);return 0;}
    p=(BSTreePlan *)calloc(1,sizeof *p);if(!p){copystr(c->error,"Not enough memory for folder preparation",256);return 0;}p->paths=bsp_temp_file();if(!p->paths){tree_plan_free(p);copystr(c->error,"Cannot create destination plan; check temporary disk space",256);return 0;}
    c->batch_approval->plan=p;p->heartbeat=bsp_seconds();c->status.awaiting_decision=0;c->state=TREE_PREPARE;message(c,"Preparing destination directories...");return 1;
}
static void tree_prepare_tick(BSClient *c){BSBatchApproval *a=c->batch_approval;BSTreePlan *p=a?a->plan:NULL;int budget=4;
    if(!p){finish_error(c,FRAME_ERROR,"Folder preparation was cancelled");return;}
    if(bsp_seconds()-p->heartbeat>=2){frame(c,TREE_WAIT,NULL,0);p->heartbeat=bsp_seconds();return;}
    while(budget-- && p->cursor<c->in_batch->count){const BSBatchItem *source=batch_item(c->in_batch,p->cursor);BSBatchItem item;char wire[256],parent[256],path[BS_PATH_MAX+1]={0},leaf[256];char *slash;size_t n;int isdir,depth;
        BSCapabilities caps;bs_local_capabilities(c,&caps);if(!source)goto bad;item=*source;copystr(wire,item.filename,sizeof wire);n=strlen(wire);isdir=tree_directory(&item);if(isdir)wire[--n]=0;
        slash=strrchr(wire,'/');
        if(p->cursor==0){if(!isdir || slash || !bs_suggest_destination(a->folder,wire,0,path,sizeof path))goto bad;
            p->depth=0;}
        else {
            if(!slash)goto bad;
            n=(size_t)(slash-wire)+1;memcpy(parent,wire,n);parent[n]=0;
            for(depth=p->depth-1;depth>=0;depth--)if(!strcmp(p->levels[depth].wire,parent))break;
            if(depth<0 || !tree_parent_safe(p,p->levels[depth].native) || bsp_path_kind(p->levels[depth].native)!=2)goto bad;
            p->depth=depth+1;
            if(!bs_suggest_name(slash+1,p->cursor,leaf,sizeof leaf) || !path_join(path,sizeof path,p->levels[depth].native,leaf))goto bad;
            if(isdir || (c->conflict_policy==BS_CONFLICT_RENAME && bsp_path_kind(path)!=0)){if(!bs_suggest_destination(p->levels[depth].native,slash+1,p->cursor,path,sizeof path))goto bad;}
        }
        if(strlen(path)>caps.path_bytes)goto bad;
        if(isdir){BSStorage space;if(p->depth>=TREE_LEVELS)goto bad;
            {char base[BS_PATH_MAX+1];storage_parent(path,base);memset(&space,0,sizeof space);if(bsp_disk_free(base,&space.available_high,&space.available_low,&space.allocation_unit) && !space.available_high && space.available_low<space.allocation_unit)goto bad;}
            if(!bsp_mkdir(path))goto bad;
            if(p->cursor==0)copystr(p->root,path,sizeof p->root);
            copystr(p->levels[p->depth].wire,item.filename,256);copystr(p->levels[p->depth].native,path,BS_PATH_MAX+1);p->depth++;
        }
        if(fwrite(path,sizeof path,1,p->paths)!=1)goto bad;
        p->cursor++;
    }
    if(p->cursor==c->in_batch->count){if(fflush(p->paths))goto bad;p->ready=1;c->state=DECISION;c->status.awaiting_decision=1;if(!tree_accept_entry(c)){char error[256];copystr(error,c->error,sizeof error);finish_error(c,FRAME_ERROR,error);}return;}
    return;
bad:finish_error(c,FRAME_ERROR,"Cannot prepare folder: check safe depth-first paths, destination name/path limits, permissions and disk space. Created directories are kept.");
}
