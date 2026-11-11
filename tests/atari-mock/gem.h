/* Host test double; only startup/teardown control flow is exercised here. */
#define FIS_SOLID 1
#define BEG_UPDATE 1
#define END_UPDATE 0
#define M_OFF 0
#define M_ON 1
#define WF_FIRSTXYWH 1
#define WF_NEXTXYWH 2
#define WF_WORKXYWH 3
#define WF_NAME 4
#define WF_TOP 5
#define WF_CURRXYWH 6
#define ARROW 0
#define NAME 1
#define CLOSER 2
#define MOVER 4
#define MU_MESAG 1
#define MU_TIMER 2
#define MU_KEYBD 4
#define MU_BUTTON 8
#define WM_REDRAW 10
#define WM_CLOSED 11
#define WM_TOPPED 12
#define WM_MOVED 13
#define AP_TERM 14
short appl_init(void);void appl_exit(void);
short graf_handle(short*,short*,short*,short*);
void v_opnvwk(short*,short*,short*);void v_clsvwk(short);
void vst_height(short,short,short*,short*,short*,short*);
void wind_get(short,short,short*,short*,short*,short*);
void wind_set(short,short,...);void wind_set_str(short,short,const char*);
short wind_create(short,short,short,short,short);void wind_open(short,short,short,short,short);void wind_close(short);void wind_delete(short);
void graf_mouse(short,void*);short form_alert(short,const char*);
void vst_color(short,short);void vsf_color(short,short);void vsf_interior(short,short);void v_bar(short,short*);void v_gtext(short,short,short,const char*);void vs_clip(short,short,short*);
void wind_update(short);void fsel_input(char*,char*,short*);
short evnt_multi(short,short,short,short,short,short,short,short,short,short,short,short,short,short,short*,unsigned long,short*,short*,short*,short*,short*,short*);
void vq_color(short,short,short,short*);
#define MD_TRANS 2
void vswr_mode(short,short);
void vst_alignment(short,short,short,short*,short*);
#define G_BUTTON 26
#define OF_LASTOB 32
#define OF_SELECTABLE 1
#define OS_NORMAL 0
#define OS_SELECTED 1
typedef struct {
 short ob_next,ob_head,ob_tail,ob_type,ob_flags,ob_state;
 union {char *free_string;} ob_spec;
 short ob_x,ob_y,ob_width,ob_height;
} OBJECT;
short objc_draw(OBJECT *,short,short,short,short,short,short);
