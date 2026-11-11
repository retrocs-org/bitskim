/* Production Atari UI state against host-only AES/VDI/network doubles. */
#define main atari_program_main
#include "../clients/atari/main.c"
#undef main
#include <assert.h>
int test_socketpair(int fd[2]);
static void test_drop(void){int fd[2];unsigned char hello[33],header[12]={0,10,'A','R','G','S',0,0,0,0,0,0};char path[CAP];const char args[]="'C:\\ONE.TXT' 'D:\\TWO.TXT'";
 assert(test_socketpair(fd)==0);assert(drop_begin(fd[0]));drop_tick();assert(read(fd[1],hello,33)==33 && hello[0]==0 && !memcmp(hello+1,"ARGS",4));
 header[9]=(unsigned char)sizeof args;assert(write(fd[1],header,1)==1);drop_tick();assert(drop.state==1 && drop.used==1);assert(write(fd[1],header+1,11)==11);drop_tick();drop_tick();drop_tick();assert(read(fd[1],hello,1)==1 && hello[0]==0);
 assert(write(fd[1],args,4)==4);drop_tick();assert(!drop_paths);assert(write(fd[1],args+4,sizeof args-4)==sizeof args-4);drop_tick();assert(drop_paths && drop_count==2);
 assert(fread(path,sizeof path,1,drop_paths)==1 && !strcmp(path,"C:\\ONE.TXT"));assert(fread(path,sizeof path,1,drop_paths)==1 && !strcmp(path,"D:\\TWO.TXT"));drop_discard();close(fd[1]);
}
int main(void){FILE *f;char saved[1024];size_t n;short clip[]={0,0,319,199};
 assert(bs_platform_init());strcpy(config_path,"UI-PREFS.CFG");remove(config_path);load();assert(prefs.ask && prefs.visible && prefs.receiving);
 engine=bs_create_app("Atari UI test",PLATFORM,prefs.id,1,1);assert(engine);window=vhandle=1;desktop[2]=work[2]=320;desktop[3]=work[3]=200;colour_count=2;palette_read();
 open_preferences();assert(page==PREFS && pref_section==0);preferences_click(16,9);assert(draft.bell && !prefs.bell);
 preferences_click(2,4);assert(pref_section==1);preferences_click(16,2);assert(!draft.visible);strcpy(rate_fields[0],"32");strcpy(rate_fields[1],"16");assert(apply_preferences(1));assert(prefs.bell && !prefs.visible && prefs.send_limit==32768 && prefs.receive_limit==16384);
 f=fopen(config_path,"r");assert(f);n=fread(saved,1,sizeof saved-1,f);saved[n]=0;fclose(f);assert(strstr(saved,"bell=1") && !strstr(saved,"theme="));
 load();assert(prefs.bell && prefs.send_limit==32768);
 open_preferences();draft.automatic=1;assert(apply_preferences(0));assert(prefs.automatic);load();assert(!prefs.automatic);
 open_preferences();draft.ask=0;preferences_click(22,12);assert(draft.ask==prefs.ask);draft.ask=0;preferences_key(27);assert(prefs.ask && page==MAIN);
 open_preferences();palette_select();assert(ui_bg!=ui_fg && ui_accent!=ui_selected_text);paint(clip);
 preferences_key(14);assert(pref_section==1);preferences_key(9);assert(focus==12);
 preferences_key(' ');assert(draft.receiving!=prefs.receiving);
 preferences_key(27);click(21,12);assert(!running && page==MAIN);
 running=1;drop_adding=1;key(27);assert(running && page==QUIT);
 key(27);assert(page==MAIN);drop_adding=0;
 test_drop();
 bs_destroy(engine);bs_platform_cleanup();remove(config_path);puts("PASS Atari UI state: preferences sections, persisted defaults/settings, Use/Save/Revert/Cancel, GEM contrast, quit confirmation and fragmented GEM ARGS handshake");return 0;
}
