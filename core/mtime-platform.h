/* UTC seconds on the wire; preserve only regular-file modification dates. */
#if defined(_WIN32)
static HANDLE mtime_handle(const char *path,int write){BSFChar native[BS_PATH_MAX+1];HANDLE file;BY_HANDLE_FILE_INFORMATION info;
    if(!fs_native(path,native))return INVALID_HANDLE_VALUE;
#ifdef BS_WINDOWS_LEGACY
    file=CreateFileA(native,FILE_READ_ATTRIBUTES|(write?FILE_WRITE_ATTRIBUTES:0),FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,LOBYTE(LOWORD(GetVersion()))>=5?FILE_FLAG_OPEN_REPARSE_POINT:0,NULL);
#else
    file=CreateFileW(native,FILE_READ_ATTRIBUTES|(write?FILE_WRITE_ATTRIBUTES:0),FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);
#endif
    if(file!=INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(file,&info) || (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))){CloseHandle(file);return INVALID_HANDLE_VALUE;}
    return file;
}
int bsp_mtime_get(const char *path,unsigned long *seconds){HANDLE file;FILETIME modified;ULARGE_INTEGER ticks;int okay;
    file=mtime_handle(path,0);if(file==INVALID_HANDLE_VALUE)return 0;okay=GetFileTime(file,NULL,NULL,&modified)!=0;CloseHandle(file);if(!okay)return 0;
    ticks.LowPart=modified.dwLowDateTime;ticks.HighPart=modified.dwHighDateTime;
    if(ticks.QuadPart<116444736000000000ULL)return 0;
    ticks.QuadPart=(ticks.QuadPart-116444736000000000ULL)/10000000ULL;
    if(ticks.QuadPart>0xffffffffUL)return 0;
    *seconds=(unsigned long)ticks.QuadPart;return 1;
}
int bsp_mtime_set(const char *path,unsigned long seconds){HANDLE file;FILETIME modified;ULARGE_INTEGER ticks;int okay;
    if(seconds>0xffffffffUL)return 0;
    file=mtime_handle(path,1);if(file==INVALID_HANDLE_VALUE)return 0;
    ticks.QuadPart=116444736000000000ULL+(ULONGLONG)seconds*10000000ULL;modified.dwLowDateTime=ticks.LowPart;modified.dwHighDateTime=ticks.HighPart;
    okay=SetFileTime(file,NULL,NULL,&modified)!=0;CloseHandle(file);return okay;
}
#else
#include <utime.h>
int bsp_mtime_get(const char *path,unsigned long *seconds){struct stat st;
    if(bsp_path_kind(path)!=1 || stat(path,&st))return 0;
#if !(defined(BS_DOS) || defined(BS_WIN16))
    if(st.st_mtime<0)return 0;
#endif
#if ULONG_MAX > 0xffffffffUL
    if((unsigned long)st.st_mtime>0xffffffffUL)return 0;
#endif
    *seconds=(unsigned long)st.st_mtime;return 1;
}
int bsp_mtime_set(const char *path,unsigned long seconds){struct stat st;time_t converted=(time_t)seconds;
    if((unsigned long)converted!=seconds || bsp_path_kind(path)!=1 || stat(path,&st))return 0;
#if !(defined(BS_DOS) || defined(BS_WIN16))
    if(converted<0)return 0;
#endif
#if ULONG_MAX > 0xffffffffUL
    if(seconds>0xffffffffUL)return 0;
#endif
#if defined(__amigaos__)
    if(seconds<252460800UL)return 0;
#elif (defined(BS_DOS) || defined(BS_WIN16)) || defined(BS_ATARI)
    if(seconds<315532800UL)return 0;
#endif
#if !defined(__amigaos__) && !(defined(BS_DOS) || defined(BS_WIN16)) && !defined(BS_ATARI) && (!defined(__riscos__) || defined(__linux__))
    {int fd,okay;struct timespec times[2];fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return 0;
     if(fstat(fd,&st) || !S_ISREG(st.st_mode)){close(fd);return 0;}times[0].tv_sec=0;times[0].tv_nsec=UTIME_OMIT;times[1].tv_sec=converted;times[1].tv_nsec=0;
     okay=futimens(fd,times)==0;close(fd);return okay;}
#else
    {struct utimbuf times;times.actime=st.st_atime;times.modtime=converted;return utime(path,&times)==0;}
#endif
}
#endif
