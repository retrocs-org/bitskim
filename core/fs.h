/* Bounded filesystem boundary. Directory iterators never follow links. */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
static void fs_product(unsigned long a,unsigned long b,unsigned long *hi,unsigned long *lo){
    unsigned long x=a&65535UL,y=b&65535UL,w=x*y,t=(a>>16&65535UL)*y+(w>>16),z=t>>16;
    t=(t&65535UL)+x*(b>>16&65535UL);*hi=((a>>16&65535UL)*(b>>16&65535UL)+z+(t>>16))&0xffffffffUL;*lo=((t<<16)+(w&65535UL))&0xffffffffUL;
#if ULONG_MAX > 0xffffffffUL
    *hi=(*hi+((a>>32)*b))&0xffffffffUL;
#endif
}
#if defined(_WIN32)
#ifdef BS_WINDOWS_LEGACY
typedef char BSFChar;
#define FS_DATA WIN32_FIND_DATAA
#define FS_FIND FindFirstFileA
#define FS_NEXT FindNextFileA
#define FS_ATTR GetFileAttributesA
#define FS_MKDIR CreateDirectoryA
#define FS_DISK GetDiskFreeSpaceA
#define FS_EX "GetDiskFreeSpaceExA"
#define FS_FULL GetFullPathNameA
static int fs_native(const char *p,BSFChar *out){return bs_to_local(p,out,BS_PATH_MAX+1);}
static int fs_name(const char *p,char *out,size_t n){return bs_to_utf8(p,out,n);}
#else
typedef wchar_t BSFChar;
#define FS_DATA WIN32_FIND_DATAW
#define FS_FIND FindFirstFileW
#define FS_NEXT FindNextFileW
#define FS_ATTR GetFileAttributesW
#define FS_MKDIR CreateDirectoryW
#define FS_DISK GetDiskFreeSpaceW
#define FS_EX "GetDiskFreeSpaceExW"
#define FS_FULL GetFullPathNameW
static int fs_native(const char *p,BSFChar *out){return MultiByteToWideChar(CP_UTF8,0,p,-1,out,BS_PATH_MAX+1)!=0;}
static int fs_name(const wchar_t *p,char *out,size_t n){return WideCharToMultiByte(CP_UTF8,0,p,-1,out,(int)n,NULL,NULL)!=0;}
#endif
typedef struct {HANDLE handle;FS_DATA data;int first;} BSFDir;
int bsp_path_kind(const char *p){BSFChar native[BS_PATH_MAX+1];DWORD attr;if(!fs_native(p,native))return -1;attr=FS_ATTR(native);if(attr==INVALID_FILE_ATTRIBUTES)return GetLastError()==ERROR_FILE_NOT_FOUND || GetLastError()==ERROR_PATH_NOT_FOUND?0:-1;if(attr&FILE_ATTRIBUTE_REPARSE_POINT)return -1;return attr&FILE_ATTRIBUTE_DIRECTORY?2:1;}
int bsp_mkdir(const char *p){BSFChar native[BS_PATH_MAX+1];return fs_native(p,native) && FS_MKDIR(native,NULL)?1:0;}
void *bsp_dir_open(const char *p){BSFDir *d;BSFChar native[BS_PATH_MAX+1];size_t n;if(bsp_path_kind(p)!=2 || !fs_native(p,native))return NULL;
#ifdef BS_WINDOWS_LEGACY
    n=strlen(native);if(n>BS_PATH_MAX-3)return NULL;strcat(native,"\\*");
#else
    n=wcslen(native);if(n>BS_PATH_MAX-3)return NULL;wcscat(native,L"\\*");
#endif
    d=(BSFDir *)calloc(1,sizeof *d);if(!d)return NULL;d->handle=FS_FIND(native,&d->data);if(d->handle==INVALID_HANDLE_VALUE){free(d);return NULL;}d->first=1;return d;
}
int bsp_dir_next(void *handle,char *name,size_t cap){BSFDir *d=(BSFDir *)handle;for(;;){if(!d->first && !FS_NEXT(d->handle,&d->data))return GetLastError()==ERROR_NO_MORE_FILES?0:-1;d->first=0;if(!fs_name(d->data.cFileName,name,cap))return -1;if(strcmp(name,".") && strcmp(name,".."))return 1;}}
void bsp_dir_close(void *handle){BSFDir *d=(BSFDir *)handle;if(d){FindClose(d->handle);free(d);}}
int bsp_disk_free(const char *p,unsigned long *hi,unsigned long *lo,unsigned long *unit){BSFChar native[BS_PATH_MAX+1],full[BS_PATH_MAX+1],root[BS_PATH_MAX+1];size_t i=0;DWORD sectors,bytes,free_clusters,total;ULARGE_INTEGER available;typedef BOOL(WINAPI *FreeEx)(const BSFChar *,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER);FreeEx extended=(FreeEx)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"),FS_EX);
    if(!fs_native(p,native) || !FS_FULL(native,BS_PATH_MAX+1,full,NULL))return 0;
    if(full[0]=='\\' && full[1]=='\\'){int slashes=0;for(i=2;full[i];i++)if(full[i]=='\\' && ++slashes==2){i++;break;}}
    else if(full[1]==':')i=3;else return 0;
    if(!i || i>BS_PATH_MAX)return 0;
    memcpy(root,full,i*sizeof *root);root[i]=0;
    *unit=4096;if(FS_DISK(root,&sectors,&bytes,&free_clusters,&total) && sectors && bytes && sectors<=0xffffffffUL/bytes)*unit=(unsigned long)sectors*bytes;
    if(extended && extended(native,&available,NULL,NULL)){*hi=available.HighPart;*lo=available.LowPart;return 1;}
    if(!FS_DISK(root,&sectors,&bytes,&free_clusters,&total) || !sectors || !bytes || sectors>0xffffffffUL/bytes)return 0;
    *unit=(unsigned long)sectors*bytes;fs_product(free_clusters,*unit,hi,lo);return 1;
}
#elif defined(__amigaos__)
#include <dos/dosextens.h>
#include <exec/memory.h>
typedef struct {BPTR lock;struct FileInfoBlock *info;} BSFDir;
int bsp_path_kind(const char *p){BPTR lock=Lock((STRPTR)p,ACCESS_READ);struct FileInfoBlock *info;int kind=0;if(!lock)return IoErr()==ERROR_OBJECT_NOT_FOUND?0:-1;info=(struct FileInfoBlock *)AllocDosObject(DOS_FIB,NULL);if(info){if(Examine(lock,info)){LONG type=info->fib_DirEntryType;kind=type==ST_FILE?1:(type==ST_ROOT || type==ST_USERDIR)?2:-1;}else kind=-1;FreeDosObject(DOS_FIB,info);}else kind=-1;UnLock(lock);return kind;}
int bsp_mkdir(const char *p){BPTR lock=CreateDir((STRPTR)p);if(!lock)return 0;UnLock(lock);return 1;}
void *bsp_dir_open(const char *p){BSFDir *d;if(bsp_path_kind(p)!=2)return NULL;d=(BSFDir *)calloc(1,sizeof *d);if(!d)return NULL;d->lock=Lock((STRPTR)p,ACCESS_READ);d->info=(struct FileInfoBlock *)AllocDosObject(DOS_FIB,NULL);if(!d->lock || !d->info || !Examine(d->lock,d->info)){if(d->lock)UnLock(d->lock);if(d->info)FreeDosObject(DOS_FIB,d->info);free(d);return NULL;}return d;}
int bsp_dir_next(void *handle,char *name,size_t cap){BSFDir *d=(BSFDir *)handle;if(!ExNext(d->lock,d->info))return IoErr()==ERROR_NO_MORE_ENTRIES?0:-1;if(d->info->fib_DirEntryType!=ST_FILE && d->info->fib_DirEntryType!=ST_USERDIR)return -1;if(strlen(d->info->fib_FileName)>=cap)return -1;strcpy(name,d->info->fib_FileName);return 1;}
void bsp_dir_close(void *handle){BSFDir *d=(BSFDir *)handle;if(d){UnLock(d->lock);FreeDosObject(DOS_FIB,d->info);free(d);}}
int bsp_disk_free(const char *p,unsigned long *hi,unsigned long *lo,unsigned long *unit){BPTR lock=Lock((STRPTR)p,ACCESS_READ);struct InfoData *info;int ok=0;if(!lock)return 0;info=(struct InfoData *)AllocVec(sizeof *info,MEMF_PUBLIC|MEMF_CLEAR);if(info){if(Info(lock,info) && info->id_NumBlocks>=info->id_NumBlocksUsed && info->id_BytesPerBlock>0){*unit=(unsigned long)info->id_BytesPerBlock;fs_product((unsigned long)(info->id_NumBlocks-info->id_NumBlocksUsed),*unit,hi,lo);ok=1;}FreeVec(info);}UnLock(lock);return ok;}
#elif defined(BS_DOS)
#include <direct.h>
typedef struct {struct find_t entry;int first,ended;} BSFDir;
int bsp_path_kind(const char *p){unsigned attr;if(_dos_getfileattr(p,&attr))return errno==ENOENT?0:-1;return attr&_A_SUBDIR?2:1;}
int bsp_mkdir(const char *p){return mkdir(p)==0;}
void *bsp_dir_open(const char *p){BSFDir *d;char pattern[128];size_t n=strlen(p);if(n>123 || bsp_path_kind(p)!=2)return NULL;strcpy(pattern,p);if(pattern[n-1]!='/' && pattern[n-1]!='\\' && pattern[n-1]!=':')strcat(pattern,"/");strcat(pattern,"*.*");d=(BSFDir *)calloc(1,sizeof *d);if(!d)return NULL;{unsigned rc=_dos_findfirst(pattern,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&d->entry);if(rc && rc!=2 && rc!=18){free(d);return NULL;}d->ended=rc!=0;}d->first=1;return d;}
int bsp_dir_next(void *handle,char *name,size_t cap){BSFDir *d=(BSFDir *)handle;for(;;){if(d->ended)return 0;if(!d->first){unsigned rc=_dos_findnext(&d->entry);if(rc){d->ended=1;return rc==2 || rc==18?0:-1;}}d->first=0;if(!strcmp(d->entry.name,".") || !strcmp(d->entry.name,".."))continue;if(strlen(d->entry.name)>=cap)return -1;strcpy(name,d->entry.name);return 1;}}
void bsp_dir_close(void *d){free(d);}
int bsp_disk_free(const char *p,unsigned long *hi,unsigned long *lo,unsigned long *unit){struct diskfree_t info;unsigned drive=0;if(isalpha((unsigned char)p[0]) && p[1]==':')drive=(unsigned)(toupper((unsigned char)p[0])-'A'+1);if(_dos_getdiskfree(drive,&info) || !info.sectors_per_cluster || !info.bytes_per_sector)return 0;*unit=(unsigned long)info.sectors_per_cluster*info.bytes_per_sector;fs_product(info.avail_clusters,*unit,hi,lo);return 1;}
#elif defined(BS_ATARI_STING) && !defined(__APPLE__) && !defined(__linux__)
typedef struct {_DTA entry;int first,ended;} BSFDir;
static int fs_tos_native(const char *p,char *out){size_t i;if(!p || strlen(p)>BS_PATH_MAX)return 0;for(i=0;p[i];i++)out[i]=p[i]=='/'?'\\':p[i];out[i]=0;return 1;}
int bsp_path_kind(const char *p){char native[BS_PATH_MAX+1];long attr;if(!fs_tos_native(p,native))return -1;attr=Fattrib(native,0,0);if(attr<0)return attr==-33 || attr==-34?0:-1;return attr&16?2:1;}
int bsp_mkdir(const char *p){char native[BS_PATH_MAX+1];return fs_tos_native(p,native) && Dcreate(native)==0;}
void *bsp_dir_open(const char *p){BSFDir *d;_DTA *saved;char pattern[BS_PATH_MAX+1];size_t n=strlen(p);if(n>BS_PATH_MAX-5 || bsp_path_kind(p)!=2)return NULL;if(!fs_tos_native(p,pattern))return NULL;if(pattern[n-1]!='\\' && pattern[n-1]!='/' && pattern[n-1]!=':')strcat(pattern,"\\");strcat(pattern,"*.*");d=(BSFDir *)calloc(1,sizeof *d);if(!d)return NULL;saved=Fgetdta();Fsetdta(&d->entry);{long rc=Fsfirst(pattern,55);Fsetdta(saved);if(rc<0 && rc!=-33 && rc!=-49){free(d);return NULL;}d->ended=rc<0;}d->first=1;return d;}
int bsp_dir_next(void *handle,char *name,size_t cap){BSFDir *d=(BSFDir *)handle;_DTA *saved;for(;;){if(d->ended)return 0;if(!d->first){saved=Fgetdta();Fsetdta(&d->entry);{long rc=Fsnext();Fsetdta(saved);if(rc<0){d->ended=1;return rc==-33 || rc==-49?0:-1;}}}d->first=0;if(!strcmp(d->entry.dta_name,".") || !strcmp(d->entry.dta_name,".."))continue;if(strlen(d->entry.dta_name)>=cap)return -1;strcpy(name,d->entry.dta_name);return 1;}}
void bsp_dir_close(void *p){free(p);}
int bsp_disk_free(const char *p,unsigned long *hi,unsigned long *lo,unsigned long *unit){long info[4];int drive=0;if(isalpha((unsigned char)p[0]) && p[1]==':')drive=toupper((unsigned char)p[0])-'A'+1;if(Dfree(info,drive)<0 || info[0]<0 || info[2]<=0 || info[3]<=0 || (unsigned long)info[2]>0xffffffffUL/(unsigned long)info[3])return 0;*unit=(unsigned long)info[2]*info[3];fs_product((unsigned long)info[0],*unit,hi,lo);return 1;}
#else
#include <dirent.h>
#include <sys/stat.h>
#if defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__)
#include <kernel.h>
#include <swis.h>
#include <unixlib/local.h>
#else
#include <sys/statvfs.h>
#endif
int bsp_path_kind(const char *p){struct stat st;if(lstat(p,&st))return errno==ENOENT?0:-1;return S_ISREG(st.st_mode)?1:S_ISDIR(st.st_mode)?2:-1;}
int bsp_mkdir(const char *p){return mkdir(p,0700)==0;}
void *bsp_dir_open(const char *p){return bsp_path_kind(p)==2?opendir(p):NULL;}
int bsp_dir_next(void *handle,char *name,size_t cap){struct dirent *entry;for(;;){errno=0;entry=readdir((DIR *)handle);if(!entry)return errno?-1:0;if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;if(strlen(entry->d_name)>=cap)return -1;strcpy(name,entry->d_name);return 1;}}
void bsp_dir_close(void *d){if(d)closedir((DIR *)d);}
int bsp_disk_free(const char *p,unsigned long *hi,unsigned long *lo,unsigned long *unit){
#if defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__)
    char native[BS_PATH_MAX+1];_kernel_swi_regs r;if(!__riscosify_std(p,0,native,sizeof native,NULL) || !strncmp(native,"Net:",4) || !strncmp(native,"NetFS:",6))return 0;
    *unit=4096;memset(&r,0,sizeof r);r.r[0]=55;r.r[1]=(int)native;
    if(!_kernel_swi(OS_FSControl,&r,&r)){*lo=(unsigned long)r.r[0];*hi=(unsigned long)r.r[1];return 1;}
    memset(&r,0,sizeof r);r.r[0]=49;r.r[1]=(int)native;if(_kernel_swi(OS_FSControl,&r,&r))return 0;fs_product((unsigned long)r.r[0],1,hi,lo);return 1;
#else
    struct statvfs info;if(statvfs(p,&info))return 0;*unit=(unsigned long)(info.f_frsize?info.f_frsize:info.f_bsize);if(!*unit)return 0;fs_product((unsigned long)info.f_bavail,*unit,hi,lo);return 1;
#endif
}
#endif
