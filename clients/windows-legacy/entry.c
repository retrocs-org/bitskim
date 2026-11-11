/* Avoid modern MinGW CRT startup, which imports UCRT and post-9x APIs. */
#include <windows.h>
extern int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int);
void __cdecl legacy_entry(void) {
    STARTUPINFOA startup; ZeroMemory(&startup,sizeof startup); startup.cb=sizeof startup;
    GetStartupInfoA(&startup);
    ExitProcess(WinMain(GetModuleHandleA(NULL),NULL,GetCommandLineA(),startup.dwFlags&STARTF_USESHOWWINDOW?startup.wShowWindow:SW_SHOWDEFAULT));
}
