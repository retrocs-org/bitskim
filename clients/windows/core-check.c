/* Native Windows loopback/platform check. No GUI or external peer required. */
#include "bitskim.h"
#include "platform.h"
#include <bcrypt.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>
static int failures;
static BSClient *a,*b;
static void check(int ok,const char *name) { printf("%s %s\n",ok?"PASS":"FAIL",name); if(!ok) failures++; }
static void check_app_startup(void) {
    wchar_t path[BS_PATH_MAX+1],command[(BS_PATH_MAX+1)*2],*slash;
    STARTUPINFOW startup; PROCESS_INFORMATION process; DWORD result=1; int ok=0;
    if(!GetModuleFileNameW(NULL,path,BS_PATH_MAX+1)) { check(0,"GUI image startup"); return; }
    slash=wcsrchr(path,L'\\'); if(!slash) { check(0,"GUI image startup"); return; } *slash=0;
    /* Release packages keep this tool in Tools; build outputs keep it beside the app. */
    slash=wcsrchr(path,L'\\'); if(slash && !_wcsicmp(slash+1,L"Tools")) *slash=0;
    if(wcslen(path)+16>=BS_PATH_MAX) { check(0,"GUI image startup"); return; }
    wcscat(path,L"\\BitSkim.exe"); swprintf(command,(BS_PATH_MAX+1)*2,L"\"%ls\" --check-startup",path);
    ZeroMemory(&startup,sizeof startup); startup.cb=sizeof startup;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    if(CreateProcessW(path,command,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) {
        if(WaitForSingleObject(process.hProcess,10000)==WAIT_OBJECT_0) ok=GetExitCodeProcess(process.hProcess,&result) && result==0;
        else TerminateProcess(process.hProcess,1);
        CloseHandle(process.hThread); CloseHandle(process.hProcess);
    }
    check(ok,"GUI executable initializes controls, themed tools, stable scrolling and networking");
}
static BSClient *test_client(char id,unsigned short *port) {
    BSConfig config; int attempt;
    memset(&config,0,sizeof config); memset(config.id,id,32); strcpy(config.name,"Windows self-check"); strcpy(config.platform,"Windows"); strcpy(config.bind_address,"127.0.0.1"); config.receiving=1;
    for(attempt=0;attempt<20;attempt++) {
        BSSocket s=socket(AF_INET,SOCK_STREAM,0); struct sockaddr_in addr; int size=sizeof addr; BSClient *c;
        if(s==INVALID_SOCKET) return NULL;
        memset(&addr,0,sizeof addr); addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(s,(struct sockaddr *)&addr,sizeof addr) || getsockname(s,(struct sockaddr *)&addr,&size)) { closesocket(s); continue; }
        *port=ntohs(addr.sin_port); closesocket(s); config.port=config.discovery_port=*port;
        c=bs_create(&config,NULL,NULL); if(c) return c;
    }
    return NULL;
}
static int pump(int decision,BSClient *receiver) {
    ULONGLONG start=GetTickCount64();
    while(GetTickCount64()-start<15000) {
        bs_tick(a); bs_tick(b);
        if(decision && bs_status(receiver)->awaiting_decision) return 1;
        if(!decision && !bs_status(a)->active && !bs_status(b)->active) return 1;
        Sleep(1);
    }
    return 0;
}
static void transfer(BSClient *sender,BSClient *receiver,unsigned short port,const char *source,const char *destination,int size,const char *label) {
    int okay=bs_send(sender,"127.0.0.1",port,source) && pump(1,receiver) && bs_accept(receiver,destination) && pump(0,receiver);
    FILE *f=okay?bsp_open_file(destination,"rb"):NULL; int i;
    if(f) {
        for(i=0;i<size;i++) if(fgetc(f)!=(i%251)) okay=0;
        if(fgetc(f)!=EOF) okay=0;
        fclose(f);
    } else okay=0;
    if(!strstr(bs_status(sender)->message,"delivered successfully")) okay=0;
    check(okay,label);
}
static void collection_check(unsigned short port,const char *folder,const char *source) {
    char target[BS_PATH_MAX+1];int i,okay=1;unsigned long size;ULONGLONG start;
    bs_batch_enable(b,1);
    for(i=0;i<24;i++)if(!bs_queue_address(a,"127.0.0.1",port,source))okay=0;
    okay=okay && pump(1,b) && bs_batch_count(b)==24 && bs_accept_batch(b,folder);
    start=GetTickCount64();
    while(okay && (bs_queue_count(a) || bs_status(b)->active) && GetTickCount64()-start<20000) {
        bs_tick(a);bs_tick(b);if(bs_status(b)->awaiting_decision || bs_queue_paused(a))okay=0;Sleep(1);
    }
    okay=okay && !bs_queue_count(a) && !bs_status(b)->active;
    for(i=0;i<24;i++) {
        /* The original source already exists, so receiving uses suffixes 1..24. */
        {char stem[BS_PATH_MAX+1];strcpy(stem,source);stem[strlen(stem)-4]=0;snprintf(target,sizeof target,"%.1000s.%d.bin",stem,i+1);}
        if(!bsp_size(target,&size) || size!=65553)okay=0;
        bsp_remove_file(target);
    }
    check(okay,"disk-backed Unicode collection: 24 files, one approval and folder, collision-safe sequential delivery");
}
static void remove_directory(const char *path){wchar_t native[BS_PATH_MAX+1];if(MultiByteToWideChar(CP_UTF8,0,path,-1,native,BS_PATH_MAX+1))RemoveDirectoryW(native);}
static void folder_check(unsigned short port,const char *folder){char root[BS_PATH_MAX+1],sub[BS_PATH_MAX+1],empty_dir[BS_PATH_MAX+1],file[BS_PATH_MAX+1],target[BS_PATH_MAX+1],old_root[BS_PATH_MAX+1],new_root[BS_PATH_MAX+1],new_sub[BS_PATH_MAX+1],new_empty[BS_PATH_MAX+1],received[BS_PATH_MAX+1];FILE *f;int okay=1;unsigned long size;ULONGLONG start;BSStorage info;
    snprintf(root,sizeof root,"%.900s/Tree",folder);snprintf(sub,sizeof sub,"%.900s/Tree/Sub",folder);snprintf(empty_dir,sizeof empty_dir,"%.900s/Tree/Sub/Empty",folder);snprintf(file,sizeof file,"%.900s/Tree/Sub/unicode-\xc3\xa5-\xce\xbb.bin",folder);
    snprintf(target,sizeof target,"%.900s/FolderReceive",folder);snprintf(old_root,sizeof old_root,"%.900s/FolderReceive/Tree",folder);snprintf(new_root,sizeof new_root,"%.900s/FolderReceive/Tree.1",folder);snprintf(new_sub,sizeof new_sub,"%.900s/FolderReceive/Tree.1/Sub",folder);snprintf(new_empty,sizeof new_empty,"%.900s/FolderReceive/Tree.1/Sub/Empty",folder);snprintf(received,sizeof received,"%.900s/FolderReceive/Tree.1/Sub/unicode-\xc3\xa5-\xce\xbb.bin",folder);
    okay=bsp_mkdir(root) && bsp_mkdir(sub) && bsp_mkdir(empty_dir) && bsp_mkdir(target) && bsp_mkdir(old_root);f=bsp_create_file(file);if(f){fputs("native folder contents",f);fclose(f);}else okay=0;
    check(bs_storage_info(b,target,0,&info) && info.known,"native Windows destination disk-space query");
    okay=okay && bs_queue_address(a,"127.0.0.1",port,root) && pump(1,b) && bs_batch_is_tree(b) && bs_accept_batch(b,target);
    start=GetTickCount64();while(okay && (bs_queue_count(a) || bs_status(b)->active) && GetTickCount64()-start<20000){bs_tick(a);bs_tick(b);if(bs_queue_paused(a))okay=0;Sleep(1);}
    okay=okay && !bs_queue_count(a) && bsp_path_kind(new_empty)==2 && bsp_path_kind(old_root)==2 && bsp_size(received,&size) && size==22;
    check(okay,"folder transfer: native iterator, nested Unicode file, empty directory and preserved existing root");
    bsp_remove_file(file);bsp_remove_file(received);remove_directory(new_empty);remove_directory(new_sub);remove_directory(new_root);remove_directory(old_root);remove_directory(target);remove_directory(empty_dir);remove_directory(sub);remove_directory(root);
}
int main(void) {
    wchar_t temp[BS_PATH_MAX+1],directory[BS_PATH_MAX+1]; char folder[BS_PATH_MAX-80],source[BS_PATH_MAX+1],dest[BS_PATH_MAX+1],empty[BS_PATH_MAX+1],reverse[BS_PATH_MAX+1],part[BS_PATH_MAX+1],reserved[BS_PATH_MAX+1];
    unsigned short pa,pb; FILE *f; int i; unsigned char random[8];
    check_app_startup();
    check(sizeof(BSSocket)==sizeof(SOCKET),"socket handles retain full native width");
    if(!bs_platform_init()) return 1;
    GetTempPathW(BS_PATH_MAX+1,temp);
    if(wcslen(temp)>BS_PATH_MAX-80 || BCryptGenRandom(NULL,random,sizeof random,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) return 1;
    swprintf(directory,BS_PATH_MAX+1,L"%lsBitSkim-check-%02x%02x%02x%02x%02x%02x%02x%02x",temp,random[0],random[1],random[2],random[3],random[4],random[5],random[6],random[7]);
    if(!CreateDirectoryW(directory,NULL) || !WideCharToMultiByte(CP_UTF8,0,directory,-1,folder,sizeof folder,NULL,NULL) || strlen(folder)>BS_PATH_MAX-80) return 1;
    snprintf(source,sizeof source,"%s\\unicode-\xc3\xa5-\xce\xbb.bin",folder); snprintf(dest,sizeof dest,"%s\\received-\xc3\xa5-\xce\xbb.bin",folder);
    snprintf(empty,sizeof empty,"%s\\empty.bin",folder); snprintf(reverse,sizeof reverse,"%s\\reverse.bin",folder); snprintf(part,sizeof part,"%s\\collision.part",folder); snprintf(reserved,sizeof reserved,"%s\\CON.txt",folder);
    f=bsp_create_file(source); if(!f) return 1; for(i=0;i<65553;i++) fputc(i%251,f); fclose(f);
    f=bsp_create_file(empty); if(!f) return 1; fclose(f);
    check(bsp_create_file(source)==NULL,"exclusive creation refuses existing files");
    check(bsp_create_file(reserved)==NULL,"DOS device filenames are rejected");
    a=test_client('a',&pa); b=test_client('b',&pb); if(!a || !b) { check(0,"loopback listeners"); return 1; }
    transfer(a,b,pb,source,dest,65553,"UTF-8 paths and streamed binary transfer over Winsock");
    check(bs_transfer_encrypted(a) && bs_transfer_encrypted(b),"Prefer encrypts automatically without a pairing key over a manual connection");
    transfer(b,a,pa,empty,reverse,0,"reverse zero-byte transfer");
    f=bsp_create_file(part); if(f) { fputs("temporary",f); fclose(f); }
    check(f && bsp_commit(part,dest)<0,"finalization refuses an existing destination");
    { unsigned long size=0; check(bsp_size(dest,&size) && size==65553,"collision preserves destination content size"); }
    collection_check(pb,folder,source);
    folder_check(pb,folder);
    bs_destroy(a); bs_destroy(b); bsp_remove_file(source); bsp_remove_file(dest); bsp_remove_file(empty); bsp_remove_file(reverse); bsp_remove_file(part); RemoveDirectoryW(directory); bs_platform_cleanup();
    printf("%s: %d failures\n",failures?"FAILED":"PASSED",failures); return failures?1:0;
}
