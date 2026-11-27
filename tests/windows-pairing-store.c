/* Isolated native DPAPI migration test; never reads the user's profile. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
static wchar_t ini_path[MAX_PATH];
#include "../clients/windows/pairing-store.h"
#define CHECK(test,label) do {if(!(test)){printf("FAIL %s\n",label);okay=0;goto done;}printf("PASS %s\n",label);}while(0)
int main(void){wchar_t folder[MAX_PATH],key[65],out[65],stored[2048];int okay=1,i;
 if(!GetTempPathW(MAX_PATH,folder) || !GetTempFileNameW(folder,L"BSK",0,ini_path))return 1;
 for(i=0;i<64;i++){key[i]=L"0123456789abcdef"[i%16];}key[64]=0;
 CHECK(WritePrivateProfileStringW(L"BitSkim",L"PairingKey",key,ini_path),"seed isolated legacy profile");
 CHECK(pairing_load(out) && !wcscmp(out,key),"verified legacy migration");
 GetPrivateProfileStringW(L"BitSkim",L"PairingKey",L"",stored,2048,ini_path);CHECK(!stored[0],"legacy plaintext removed after migration");
 GetPrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",L"",stored,2048,ini_path);CHECK(stored[0] && !wcsstr(stored,key),"protected value stored");
 key[0]=L'f';CHECK(pairing_save(key) && pairing_load(out) && !wcscmp(out,key),"update and decrypt under current Windows user");
 CHECK(WritePrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",L"invalid!",ini_path) && WritePrivateProfileStringW(L"BitSkim",L"PairingKey",key,ini_path),"seed damaged encrypted record");
 CHECK(!pairing_load(out),"damaged encrypted record fails closed");
 GetPrivateProfileStringW(L"BitSkim",L"PairingKey",L"",out,65,ini_path);CHECK(!wcscmp(out,key),"failed migration preserves legacy data");
 CHECK(pairing_save(L"") && pairing_load(out) && !out[0],"clear removes both formats");
 done:SecureZeroMemory(key,sizeof key);SecureZeroMemory(out,sizeof out);DeleteFileW(ini_path);return okay?0:1;
}
