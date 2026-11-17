/* Only native retro attribute domains; no POSIX/Windows execute-bit mapping. */
#if defined(__amigaos__)
int bsp_metadata_get(const char *path,BSMetadata *out){BPTR lock;struct FileInfoBlock *info;int okay=0;if(bsp_path_kind(path)!=1)return 0;lock=Lock((STRPTR)path,ACCESS_READ);if(!lock)return 0;
    info=(struct FileInfoBlock *)AllocDosObject(DOS_FIB,NULL);if(info){if(Examine(lock,info) && info->fib_DirEntryType<0){memset(out,0,sizeof *out);out->kind=1;out->protection=(unsigned long)info->fib_Protection&255UL;okay=bs_to_utf8((const char *)info->fib_Comment,out->comment,sizeof out->comment);}FreeDosObject(DOS_FIB,info);}UnLock(lock);return okay;
}
int bsp_metadata_apply(const char *path,const BSMetadata *in){char comment[80];if(in->kind!=1 || in->protection>255UL || !bs_to_local(in->comment,comment,sizeof comment))return 0;
    return SetComment((STRPTR)path,(STRPTR)comment) && SetProtection((STRPTR)path,(LONG)in->protection);
}
#elif defined(__riscos__) && !defined(__APPLE__) && !defined(__linux__)
int bsp_riscos_metadata_get(const char *,BSMetadata *);
int bsp_riscos_metadata_apply(const char *,const BSMetadata *);
int bsp_metadata_get(const char *p,BSMetadata *m){return bsp_riscos_metadata_get(p,m);}
int bsp_metadata_apply(const char *p,const BSMetadata *m){return bsp_riscos_metadata_apply(p,m);}
#endif
