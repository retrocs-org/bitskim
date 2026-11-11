#ifndef BITSKIM_WINDOWS_NAMES_H
#define BITSKIM_WINDOWS_NAMES_H
#include <wchar.h>
#include <wctype.h>
/* Win32 normalizes DOS device basenames even when an extension is present. */
static int windows_name_supported(const wchar_t *name) {
    wchar_t stem[16]; const wchar_t *p; size_t length=wcslen(name); int n=0;
    if(!length || length>255 || name[length-1]==L'.' || name[length-1]==L' ') return 0;
    for(p=name;*p;p++) if(*p<32 || wcschr(L"<>:\"/\\|?*",*p)) return 0;
    for(p=name;*p && *p!=L'.' && n<15;p++) stem[n++]=towupper(*p);
    while(n && stem[n-1]==L' ') n--;
    stem[n]=0;
    if(!wcscmp(stem,L"CON") || !wcscmp(stem,L"PRN") || !wcscmp(stem,L"AUX") || !wcscmp(stem,L"NUL") || !wcscmp(stem,L"CLOCK$") || !wcscmp(stem,L"CONIN$") || !wcscmp(stem,L"CONOUT$")) return 0;
    if(n==4 && (!wcsncmp(stem,L"COM",3) || !wcsncmp(stem,L"LPT",3)) && ((stem[3]>=L'1' && stem[3]<=L'9') || stem[3]==0xb9 || stem[3]==0xb2 || stem[3]==0xb3)) return 0;
    return 1;
}
#endif
