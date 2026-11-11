/* Less-used FIFO actions stay in the standard Workbench window menu. */
static struct Menu *queue_menu;
static void queue_menu_close(void) {
    if(queue_menu) {if(window)ClearMenuStrip(window);FreeMenus(queue_menu);queue_menu=NULL;}
}
static int queue_menu_open(void) {
    struct NewMenu menus[]={
        {NM_TITLE,(STRPTR)"Tools",NULL,0,0,NULL},
        {NM_ITEM,(STRPTR)"Transfers / Diagnostics",(STRPTR)"I",0,0,NULL},
        {NM_END,NULL,NULL,0,0,NULL}
    };
    queue_menu=CreateMenus(menus,TAG_DONE);
    return queue_menu && LayoutMenus(queue_menu,visual,TAG_DONE) && SetMenuStrip(window,queue_menu);
}
static void queue_menu_command(UWORD code) {
    if(code!=MENUNULL && MENUNUM(code)==0 && ITEMNUM(code)==0)transfer_tools();
}
