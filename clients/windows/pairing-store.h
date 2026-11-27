/* User-scoped DPAPI; plaintext legacy entries survive until migration is verified. */
#include <wincrypt.h>
#include <dpapi.h>
static int pairing_read_protected(wchar_t out[65]) {
    wchar_t encoded[2048];BYTE bytes[1536];DWORD size=sizeof bytes;DATA_BLOB input,plain;int okay=0;
    out[0]=0;GetPrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",L"",encoded,2048,ini_path);
    if(!encoded[0])return 1;
    if(!CryptStringToBinaryW(encoded,0,CRYPT_STRING_BASE64,bytes,&size,NULL,NULL))return 0;
    input.pbData=bytes;input.cbData=size;
    if(!CryptUnprotectData(&input,NULL,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&plain))return 0;
    if(plain.cbData==65*sizeof(wchar_t) && ((wchar_t *)plain.pbData)[64]==0){memcpy(out,plain.pbData,65*sizeof(wchar_t));okay=1;}
    SecureZeroMemory(plain.pbData,plain.cbData);LocalFree(plain.pbData);SecureZeroMemory(bytes,sizeof bytes);return okay;
}
static int pairing_save(const wchar_t *key) {
    DATA_BLOB input,protected;wchar_t encoded[2048],check[65];DWORD size=2048;int okay;
    if(!key[0])return WritePrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",NULL,ini_path) && WritePrivateProfileStringW(L"BitSkim",L"PairingKey",NULL,ini_path);
    if(wcslen(key)!=64)return 0;
    input.pbData=(BYTE *)key;input.cbData=65*sizeof(wchar_t);
    if(!CryptProtectData(&input,L"BitSkim pairing key",NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&protected))return 0;
    okay=CryptBinaryToStringW(protected.pbData,protected.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,encoded,&size) && WritePrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",encoded,ini_path);
    SecureZeroMemory(protected.pbData,protected.cbData);LocalFree(protected.pbData);
    if(okay)okay=pairing_read_protected(check) && !wcscmp(check,key);
    SecureZeroMemory(check,sizeof check);
    if(okay)okay=WritePrivateProfileStringW(L"BitSkim",L"PairingKey",NULL,ini_path);
    return okay;
}
static int pairing_load(wchar_t out[65]) {
    wchar_t legacy[65],protected[2048];int okay;
    GetPrivateProfileStringW(L"BitSkim",L"PairingKeyProtected",L"",protected,2048,ini_path);
    if(protected[0]){if(!pairing_read_protected(out))return 0;return WritePrivateProfileStringW(L"BitSkim",L"PairingKey",NULL,ini_path);}
    GetPrivateProfileStringW(L"BitSkim",L"PairingKey",L"",legacy,65,ini_path);memcpy(out,legacy,sizeof legacy);
    okay=!legacy[0] || pairing_save(legacy);SecureZeroMemory(legacy,sizeof legacy);return okay;
}
