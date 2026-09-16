#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
int wmain(int argc,wchar_t** argv){
    if(argc!=2)return 1;auto proxy=LoadLibraryW(argv[1]);if(!proxy)return 2;
    auto size=reinterpret_cast<DWORD(WINAPI*)(LPCWSTR,LPDWORD)>(GetProcAddress(proxy,"GetFileVersionInfoSizeW"));
    auto info=reinterpret_cast<BOOL(WINAPI*)(LPCWSTR,DWORD,DWORD,LPVOID)>(GetProcAddress(proxy,"GetFileVersionInfoW"));
    auto query=reinterpret_cast<BOOL(WINAPI*)(LPCVOID,LPCWSTR,LPVOID*,PUINT)>(GetProcAddress(proxy,"VerQueryValueW"));
    if(!size||!info||!query)return 3;
    wchar_t dir[32768]{};GetSystemDirectoryW(dir,32768);auto file=std::wstring(dir)+L"\\kernel32.dll";
    DWORD unused{};auto length=size(file.c_str(),&unused);if(!length)return 4;
    std::vector<char> data(length);if(!info(file.c_str(),0,length,data.data()))return 5;
    void* fixed{};UINT fixedLength{};if(!query(data.data(),L"\\",&fixed,&fixedLength)||fixedLength<sizeof(VS_FIXEDFILEINFO))return 6;
    if(static_cast<VS_FIXEDFILEINFO*>(fixed)->dwSignature!=0xfeef04bd)return 7;
    std::cout<<"PASS: system VERSION APIs forwarded through original argument-preserving stubs\n";return 0;
}
