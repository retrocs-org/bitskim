/* GDI-only theme painting: no UxTheme, DWM or newer control dependencies. */
#include "../windows/themes.h"
static int theme_index=WIN_THEME_DEFAULT,saved_theme;
static HBRUSH base_brush,surface_brush;
static const WinTheme *theme(void) { return &win_themes[theme_index]; }
static BOOL CALLBACK repaint_child(HWND child,LPARAM ignored) { (void)ignored; InvalidateRect(child,NULL,TRUE); return TRUE; }
static void apply_theme(void) {
    if(base_brush) DeleteObject(base_brush); if(surface_brush) DeleteObject(surface_brush);
    base_brush=CreateSolidBrush(theme()->base); surface_brush=CreateSolidBrush(theme()->surface);
    if(window) { InvalidateRect(window,NULL,TRUE); EnumChildWindows(window,repaint_child,0); }
    if(prefs) { InvalidateRect(prefs,NULL,TRUE); EnumChildWindows(prefs,repaint_child,0); }
}
#define CHECK_KIND 0x10000L
#define CHECK_VALUE 0x20000L
static int checked(HWND c) { return (GetWindowLongA(c,GWL_USERDATA)&CHECK_VALUE)!=0; }
static void set_checked(HWND c,int value) {
    LONG data=GetWindowLongA(c,GWL_USERDATA); SetWindowLongA(c,GWL_USERDATA,value?data|CHECK_VALUE:data&~CHECK_VALUE); InvalidateRect(c,NULL,TRUE);
}
static LRESULT paint_theme(HWND w,UINT msg,WPARAM wp,LPARAM lp,int *handled) {
    HDC dc=(HDC)wp; *handled=1;
    if(msg==WM_ERASEBKGND) { RECT r; GetClientRect(w,&r); FillRect(dc,&r,base_brush); return 1; }
    if(msg==WM_CTLCOLORSTATIC || msg==WM_CTLCOLORBTN || msg==WM_CTLCOLOREDIT || msg==WM_CTLCOLORLISTBOX) {
        SetTextColor(dc,theme()->text); SetBkColor(dc,msg==WM_CTLCOLORSTATIC?theme()->base:theme()->surface);
        return (LRESULT)(msg==WM_CTLCOLORSTATIC?base_brush:surface_brush);
    }
    if(msg==WM_DRAWITEM) {
        DRAWITEMSTRUCT *d=(DRAWITEMSTRUCT *)lp; RECT r=d->rcItem; char text[256]; HBRUSH brush; COLORREF background=theme()->surface,foreground=theme()->text;
        int check=(GetWindowLongA(d->hwndItem,GWL_USERDATA)&CHECK_KIND)!=0;
        if(d->CtlType==ODT_LISTBOX) {
            if(d->itemID==(UINT)-1) return TRUE;
            SendMessageA(d->hwndItem,LB_GETTEXT,d->itemID,(LPARAM)text);
            if(d->itemState&ODS_SELECTED) { background=theme()->accent; foreground=theme()->base; }
        } else {
            GetWindowTextA(d->hwndItem,text,sizeof text);
            if(d->itemState&ODS_SELECTED) { background=theme()->accent; foreground=theme()->base; }
        }
        brush=CreateSolidBrush(check?theme()->base:background); FillRect(d->hDC,&r,brush); DeleteObject(brush);
        SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,(d->itemState&ODS_DISABLED)?theme()->muted:(check?theme()->text:foreground));
        if(check) {
            RECT box={r.left+2,r.top+5,r.left+18,r.top+21}; HPEN pen=CreatePen(PS_SOLID,1,theme()->accent); HGDIOBJ old=SelectObject(d->hDC,pen),oldbrush=SelectObject(d->hDC,surface_brush);
            Rectangle(d->hDC,box.left,box.top,box.right,box.bottom);
            if(checked(d->hwndItem)) { MoveToEx(d->hDC,box.left+3,box.top+8,NULL); LineTo(d->hDC,box.left+7,box.top+12); LineTo(d->hDC,box.left+13,box.top+3); }
            SelectObject(d->hDC,old); SelectObject(d->hDC,oldbrush); DeleteObject(pen); r.left+=28;
            DrawTextA(d->hDC,text,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        } else { DrawTextA(d->hDC,text,-1,&r,(d->CtlType==ODT_LISTBOX?DT_LEFT:DT_CENTER)|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); }
        if(d->itemState&ODS_FOCUS) { InflateRect(&r,-3,-3); DrawFocusRect(d->hDC,&r); } return TRUE;
    }
    *handled=0; return 0;
}
