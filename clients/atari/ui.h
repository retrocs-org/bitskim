/* Compact modeless preferences and palette mapping; never alter desktop colours. */
static int nearest(int r,int g,int b){int i,best=0;long distance=2147483647L;for(i=0;i<native_count;i++){long x=native_colours[i][0]-r,y=native_colours[i][1]-g,z=native_colours[i][2]-b,d=x*x+y*y+z*z;if(d<distance){distance=d;best=i;}}return best;}
static void palette_read(void){int i;native_count=colour_count<256?colour_count:256;if(native_count<2)native_count=2;palette_ready=0;for(i=0;i<native_count;i++)vq_color(vhandle,i,1,native_colours[i]);}
static void palette_select(void){
 if(palette_ready)return;
 palette_ready=1;
 ui_bg=nearest(800,800,800);
 ui_fg=nearest(0,0,0);
 ui_panel=0;
 ui_accent=nearest(0,0,667);
 if(ui_accent==ui_bg || ui_accent==ui_panel)ui_accent=ui_fg;
 ui_selected_text=0;
}
static void open_preferences(void){draft=prefs;pref_section=0;page=PREFS;focus=2;sprintf(rate_fields[0],"%lu",draft.send_limit/1024UL);sprintf(rate_fields[1],"%lu",draft.receive_limit/1024UL);dirty=1;}
static void pref_field(int row,const char *label,const char *value,int field){
 text(15,row,label);
 input_field(15,row+1,value,field);
}
static void pref_check(int row,const char *label,int value,int field){
 short x=work[0]+16*char_w,y=work[1]+row*char_h+text_inset,side=char_h-text_inset-2;
 frame(x,y,x+side-1,y+side-1,focus==field?accent():ui_fg,ui_panel);
 if(value)fill_box(x+2,y+2,x+side-3,y+side-3,accent());
 text(19,row,label);
}
static int apply_preferences(int persist){unsigned long up,down;char wire[64];if(!draft.name[0] || !bs_to_utf8(draft.name,wire,sizeof wire)){message("Choose a nonempty ASCII client name.");return 0;}if(!bs_limit_parse(rate_fields[0],&up) || !bs_limit_parse(rate_fields[1],&down)){message("Limits: 0..1048575 KiB/s; 0 = Unlimited.");return 0;}draft.send_limit=up;draft.receive_limit=down;prefs=draft;bs_configure(engine,wire,prefs.visible,prefs.receiving);bs_limits_configure(engine,up,down);if(persist && !save()){message("Settings applied, but BITSKIM.CFG was not saved.");return 0;}page=MAIN;message(persist?"Preferences saved.":"Preferences applied for this session.");return 1;}
static void paint_preferences(void){static const char *sections[]={"General","Network","About"};int i;for(i=0;i<3;i++)control(1,2+i*2,12,sections[i],pref_section==i);
 if(pref_section==0){pref_field(2,"Client name",draft.name,2);pref_field(4,"Download folder",draft.folder,3);pref_check(7,"Auto-accept",draft.automatic,7);pref_check(8,"Ask save path",draft.ask,8);pref_check(9,"Completion sound",draft.bell,14);}
 else if(pref_section==1){pref_check(2,"Visible on LAN",draft.visible,11);pref_check(3,"Allow incoming",draft.receiving,12);pref_field(5,"Send KiB/s (0=unlimited)",rate_fields[0],5);pref_field(8,"Receive KiB/s",rate_fields[1],6);text(15,11,"Plaintext transfers");}
 else {text(15,2,"BitSkim 0.2 / " STACK);text(15,4,"github.com/retrocs-org/");text(15,5,"bitskim");text(15,7,"bitskim.retrocs.org");text(15,9,"Native palette; no desktop");text(15,10,"colour changes. Experimental.");}
 button(1,12,"Save");button(11,12,"Use");button(20,12,"Revert");button(32,12,"Cancel");
}
static void preferences_click(int col,int row){if(col<14 && row>=2 && row<=6 && !(row&1)){pref_section=(row-2)/2;focus=pref_section==0?2:pref_section==1?11:13;dirty=1;return;}if(row==12){if(col<11)apply_preferences(1);else if(col<20)apply_preferences(0);else if(col<32){draft=prefs;sprintf(rate_fields[0],"%lu",draft.send_limit/1024UL);sprintf(rate_fields[1],"%lu",draft.receive_limit/1024UL);}else{page=MAIN;}dirty=1;return;}if(col<15)return;
 if(pref_section==0){if(row==2 || row==3)focus=2;else if(row==4 || row==5)focus=3;else if(row==9){focus=14;draft.bell=!draft.bell;}else if(row==7 || row==8){focus=row;if(row==7)draft.automatic=!draft.automatic;else draft.ask=!draft.ask;}}
 else if(pref_section==1){if(row==2){focus=11;draft.visible=!draft.visible;}else if(row==3){focus=12;draft.receiving=!draft.receiving;}else if(row==5 || row==6)focus=5;else if(row==8 || row==9)focus=6;}dirty=1;
}
static void preferences_key(unsigned short code){unsigned char ch=(unsigned char)code;char *field=NULL;size_t cap=0,n;if(ch==27){page=MAIN;dirty=1;return;}if(ch==19){apply_preferences(1);return;}if((code>>8)==0x3b){tools_return=PREFS;page=TOOLS;tools_record=-1;tools_top=0;dirty=1;return;}if(ch==7 || ch==14){pref_section=ch==7?0:1;focus=pref_section==0?2:11;dirty=1;return;}
 if(ch==9){if(pref_section==0)focus=focus==2?3:focus==3?7:focus==8?14:focus==14?2:focus+1;else if(pref_section==1)focus=focus==11?12:focus==12?5:focus==5?6:11;dirty=1;return;}
 if(ch==13){apply_preferences(1);return;}
 if(ch==' ' && focus==14){draft.bell=!draft.bell;dirty=1;return;}
 if(ch==' ' && (focus==7 || focus==8 || focus==11 || focus==12)){if(focus==7)draft.automatic=!draft.automatic;else if(focus==8)draft.ask=!draft.ask;else if(focus==11)draft.visible=!draft.visible;else draft.receiving=!draft.receiving;dirty=1;return;}
 if(pref_section==0 && focus==2){field=draft.name;cap=64;}else if(pref_section==0 && focus==3){field=draft.folder;cap=CAP;}else if(pref_section==1 && (focus==5 || focus==6)){field=rate_fields[focus==5?0:1];cap=8;}if(!field)return;n=strlen(field);if(ch==21)field[0]=0;else if(ch==8 && n)field[n-1]=0;else if(ch>=32 && ch<127 && (cap!=8 || (ch>='0' && ch<='9')) && n+1<cap){field[n]=ch;field[n+1]=0;}dirty=1;
}
