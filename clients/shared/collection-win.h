/* Lazy, paged native approval dialog: no list-sized allocation or new DLLs. */
#ifdef BS_WINDOWS_LEGACY
#define BS_C_CREATE CreateWindowExA
#define BS_C_TEXT SetWindowTextA
#define BS_C_SEND SendMessageA
#define BS_C_DIALOG DialogBoxIndirectParamA
#define BS_C_LITERAL(x) x
typedef char BSCChar;
#else
#define BS_C_CREATE CreateWindowExW
#define BS_C_TEXT SetWindowTextW
#define BS_C_SEND SendMessageW
#define BS_C_DIALOG DialogBoxIndirectParamW
#define BS_C_LITERAL(x) L##x
typedef wchar_t BSCChar;
#endif
typedef struct {BSClient *client;unsigned long serial;int first,top,count;} BSCollectionDialog;
static void collection_page(HWND w,BSCollectionDialog *d){int i;BSCChar line[600],name[256],mapped[256],title[120];HWND list=GetDlgItem(w,100);
    BS_C_SEND(list,LB_RESETCONTENT,0,0);
    for(i=d->top;i<d->count && i<d->top+8;i++){const BSBatchItem *item=bs_batch_item(d->client,i);if(!item)break;
#ifdef BS_WINDOWS_LEGACY
        if(!bs_to_local(item->filename,name,sizeof name))strcpy(name,"[Unicode name]");if(!bs_batch_save_name(d->client,i,mapped,sizeof mapped))strcpy(mapped,"[unsupported]");wsprintfA(line,"%d. %.240s -> %.240s (%lu bytes)",i+1,name,mapped,item->total);
#else
        {char local_name[256];MultiByteToWideChar(CP_UTF8,0,item->filename,-1,name,256);if(!bs_batch_save_name(d->client,i,local_name,sizeof local_name))strcpy(local_name,"[unsupported]");MultiByteToWideChar(CP_UTF8,0,local_name,-1,mapped,256);swprintf(line,600,L"%d. %.240ls -> %.240ls (%lu bytes)",i+1,name,mapped,item->total);}
#endif
        BS_C_SEND(list,LB_ADDSTRING,0,(LPARAM)line);
    }
    {char summary[120];bs_batch_summary(d->client,summary,sizeof summary);
#ifdef BS_WINDOWS_LEGACY
     lstrcpynA(title,summary,120);
#else
     MultiByteToWideChar(CP_UTF8,0,summary,-1,title,120);
#endif
    }
    BS_C_TEXT(w,title);EnableWindow(GetDlgItem(w,101),d->top>d->first);EnableWindow(GetDlgItem(w,102),d->top+8<d->count);
}
static INT_PTR CALLBACK collection_proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){BSCollectionDialog *d=(BSCollectionDialog *)GetWindowLongPtr(w,DWLP_USER);
    if(msg==WM_INITDIALOG){d=(BSCollectionDialog *)lp;SetWindowLongPtr(w,DWLP_USER,lp);
        BS_C_CREATE(WS_EX_CLIENTEDGE,BS_C_LITERAL("LISTBOX"),BS_C_LITERAL(""),WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|LBS_NOINTEGRALHEIGHT,12,12,480,200,w,(HMENU)100,NULL,NULL);
        BS_C_CREATE(0,BS_C_LITERAL("STATIC"),BS_C_LITERAL("One folder for this list. Names shown as original -> proposed. Sender names are unverified."),WS_CHILD|WS_VISIBLE,12,218,480,36,w,NULL,NULL,NULL);
        {HWND policy=BS_C_CREATE(0,BS_C_LITERAL("COMBOBOX"),BS_C_LITERAL(""),WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST,12,256,480,110,w,(HMENU)103,NULL,NULL);BS_C_SEND(policy,CB_ADDSTRING,0,(LPARAM)BS_C_LITERAL("Existing files: Rename safely"));BS_C_SEND(policy,CB_ADDSTRING,0,(LPARAM)BS_C_LITERAL("Existing files: Skip"));BS_C_SEND(policy,CB_ADDSTRING,0,(LPARAM)BS_C_LITERAL("Existing files: Replace"));BS_C_SEND(policy,CB_SETCURSEL,bs_conflict_get(d->client),0);}
        BS_C_CREATE(0,BS_C_LITERAL("BUTTON"),BS_C_LITERAL("Previous"),WS_CHILD|WS_VISIBLE|WS_TABSTOP,12,300,92,28,w,(HMENU)101,NULL,NULL);
        BS_C_CREATE(0,BS_C_LITERAL("BUTTON"),BS_C_LITERAL("Next"),WS_CHILD|WS_VISIBLE|WS_TABSTOP,112,300,92,28,w,(HMENU)102,NULL,NULL);
        BS_C_CREATE(0,BS_C_LITERAL("BUTTON"),BS_C_LITERAL("Decline"),WS_CHILD|WS_VISIBLE|WS_TABSTOP,286,300,92,28,w,(HMENU)IDCANCEL,NULL,NULL);
        BS_C_CREATE(0,BS_C_LITERAL("BUTTON"),BS_C_LITERAL("Accept all"),WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,386,300,106,28,w,(HMENU)IDOK,NULL,NULL);
        SetWindowPos(w,NULL,0,0,520,380,SWP_NOMOVE|SWP_NOZORDER);SetTimer(w,1,100,NULL);collection_page(w,d);return TRUE;}
    if(!d)return FALSE;
    if(msg==WM_TIMER && (bs_transfer_serial(d->client)!=d->serial || !bs_status(d->client)->awaiting_decision)){EndDialog(w,IDCANCEL);return TRUE;}
    if(msg==WM_CLOSE){EndDialog(w,IDCANCEL);return TRUE;}
    if(msg==WM_COMMAND){int id=LOWORD(wp);if(id==IDOK || id==IDCANCEL){if(id==IDOK)bs_conflict_policy(d->client,(int)BS_C_SEND(GetDlgItem(w,103),CB_GETCURSEL,0,0));EndDialog(w,id);return TRUE;}if(id==101 && d->top>d->first)d->top-=8;if(id==102 && d->top+8<d->count)d->top+=8;collection_page(w,d);return TRUE;}return FALSE;
}
static int collection_confirm(HWND owner,BSClient *client){struct {DLGTEMPLATE dialog;WORD menu,klass,title;} t;BSCollectionDialog d;
    memset(&t,0,sizeof t);t.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;t.dialog.cx=260;t.dialog.cy=170;
    d.client=client;d.serial=bs_transfer_serial(client);d.first=d.top=bs_batch_index(client);d.count=bs_batch_count(client);
    return BS_C_DIALOG(GetModuleHandle(NULL),&t.dialog,owner,collection_proc,(LPARAM)&d)==IDOK;
}
#undef BS_C_CREATE
#undef BS_C_TEXT
#undef BS_C_SEND
#undef BS_C_DIALOG
#undef BS_C_LITERAL
