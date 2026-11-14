/* Portable 64-bit byte accounting without a 64-bit compiler requirement. */
static void wide_decimal(unsigned long hi,unsigned long lo,char out[24]){char reverse[24];int n=0,i;do{unsigned long parts[4],remain=0;parts[0]=hi>>16;parts[1]=hi&65535;parts[2]=lo>>16;parts[3]=lo&65535;for(i=0;i<4;i++){unsigned long value=remain*65536UL+parts[i];parts[i]=value/10;remain=value%10;}reverse[n++]=(char)('0'+remain);hi=(parts[0]<<16)|parts[1];lo=(parts[2]<<16)|parts[3];}while((hi || lo) && n<23);for(i=0;i<n;i++)out[i]=reverse[n-1-i];out[n]=0;}
static void storage_parent(const char *path,char *folder){const char *slash=strrchr(path,'/'),*other;
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI)
    other=strrchr(path,'\\');if(other && (!slash || other>slash))slash=other;
#else
    other=NULL;(void)other;
#endif
#if defined(_WIN32) || defined(BS_DOS) || defined(BS_ATARI) || defined(__amigaos__)
    other=strrchr(path,':');if(other && (!slash || other>slash))slash=other;
#endif
    if(slash){size_t n=(size_t)(slash-path)+1;if(n>BS_PATH_MAX)n=BS_PATH_MAX;memcpy(folder,path,n);folder[n]=0;}else {
#ifdef __amigaos__
        strcpy(folder,"");
#else
        strcpy(folder,".");
#endif
    }
}
int bs_storage_info(const BSClient *c,const char *folder,int collection,BSStorage *out){unsigned long files=1,dirs=0,h,l,extra;BSBatchRecord prefix;
    if(!c || !folder || !out)return 0;
    memset(out,0,sizeof *out);out->allocation_unit=4096;
    out->known=bsp_disk_free(folder,&out->available_high,&out->available_low,&out->allocation_unit);if(!out->allocation_unit)out->allocation_unit=4096;
    if(collection && c->in_batch){out->required_high=c->in_batch->hi;out->required_low=c->in_batch->lo;files=c->in_batch->files;dirs=c->in_batch->dirs;
        if(c->in_batch_index>0){if(!batch_record(c->in_batch,c->in_batch_index-1,&prefix))return 0;wide_sub(&out->required_high,&out->required_low,prefix.hi,prefix.lo);files-=prefix.files;if(!c->in_batch->tree)dirs-=prefix.dirs;}
    }else out->required_low=c->status.total;
    /* One allocation slack per file plus directory metadata and 64 KiB reserve.
       Originals/backups stay allocated already; they are never counted as free. */
    extra=out->allocation_unit>0x7fffffffUL?0x7fffffffUL:out->allocation_unit;
    wide_product(files,extra,&h,&l);wide_add(&out->required_high,&out->required_low,h,l);wide_product(dirs,extra,&h,&l);wide_add(&out->required_high,&out->required_low,h,l);
    wide_product(files+dirs,4096,&h,&l);wide_add(&out->required_high,&out->required_low,h,l);wide_add(&out->required_high,&out->required_low,0,65536);
    out->enough=!out->known || out->available_high>out->required_high || (out->available_high==out->required_high && out->available_low>=out->required_low);return 1;
}
int bs_storage_report(const BSClient *c,const char *folder,int collection,char *out,size_t capacity){BSStorage info;char required[24],available[24],text[200];if(!out || !capacity || !bs_storage_info(c,folder,collection,&info))return 0;wide_decimal(info.required_high,info.required_low,required);wide_decimal(info.available_high,info.available_low,available);
    if(info.known){if(info.enough)sprintf(text,"Need about %s B including overhead; available %s B.",required,available);else sprintf(text,"Not enough disk space: need about %s B; available %s B.",required,available);}else sprintf(text,"Need about %s B including overhead. Free space unavailable.",required);copystr(out,text,capacity);return info.enough?1:-1;
}
static int storage_check(BSClient *c,const char *folder,int collection){BSStorage info;if(!bs_storage_info(c,folder,collection,&info)){copystr(c->error,"Cannot calculate required disk space",256);return 0;}if(!info.enough){bs_storage_report(c,folder,collection,c->error,sizeof c->error);return 0;}return 1;}

int bs_destination_space_info(const BSClient *c,const char *path,int collection,BSStorage *out){char folder[BS_PATH_MAX+1];if(!out)return 0;memset(out,0,sizeof *out);if(!path || strlen(path)>BS_PATH_MAX)return 0;if(collection)copystr(folder,path,sizeof folder);else storage_parent(path,folder);return bs_storage_info(c,folder,collection,out);}
int bs_destination_space_report(const BSClient *c,const char *path,int collection,char *out,size_t capacity){char folder[BS_PATH_MAX+1];if(!path || strlen(path)>BS_PATH_MAX)return 0;if(collection)copystr(folder,path,sizeof folder);else storage_parent(path,folder);return bs_storage_report(c,folder,collection,out,capacity);}

#ifdef BS_OPENSSL
static int storage_resume(BSClient *c,const char *path,unsigned long retained){BSStorage info;char folder[BS_PATH_MAX+1],required[24],available[24];storage_parent(path,folder);if(!bs_storage_info(c,folder,0,&info))return 0;if(retained>c->status.total)retained=c->status.total;wide_sub(&info.required_high,&info.required_low,0,retained);if(!info.known || info.available_high>info.required_high || (info.available_high==info.required_high && info.available_low>=info.required_low))return 1;wide_decimal(info.required_high,info.required_low,required);wide_decimal(info.available_high,info.available_low,available);sprintf(c->error,"Not enough disk space for remaining data: need about %s B; available %s B",required,available);return 0;}
#endif
