#pragma once
#include <windows.h>
#include <cwchar>
namespace rg {
inline bool isReturnalProcess(){
    wchar_t path[32768]{};if(!GetModuleFileNameW(nullptr,path,32768))return false;
    auto slash=std::wcsrchr(path,L'\\');auto name=slash?slash+1:path;
    return CompareStringOrdinal(name,-1,L"Returnal-Win64-Shipping.exe",-1,TRUE)==CSTR_EQUAL;
}
}
