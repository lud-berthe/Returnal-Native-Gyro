#include <windows.h>
#include "rg/process_identity.hpp"
#include <string>
#include <array>
#include <cstdint>
namespace {
HMODULE ownModule{};
std::uintptr_t WINAPI unavailable(){SetLastError(ERROR_PROC_NOT_FOUND);return 0;}
const char* const names[]={"GetFileVersionInfoA","GetFileVersionInfoByHandle","GetFileVersionInfoExA","GetFileVersionInfoExW","GetFileVersionInfoSizeA","GetFileVersionInfoSizeExA","GetFileVersionInfoSizeExW","GetFileVersionInfoSizeW","GetFileVersionInfoW","VerFindFileA","VerFindFileW","VerInstallFileA","VerInstallFileW","VerLanguageNameA","VerLanguageNameW","VerQueryValueA","VerQueryValueW"};
DWORD WINAPI loadMod(void*){
    wchar_t path[32768]{};GetModuleFileNameW(ownModule,path,32768);std::wstring filename(path);filename.resize(filename.find_last_of(L"\\/")+1);filename+=L"ReturnalGyro.dll";
    if(!LoadLibraryExW(filename.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS))OutputDebugStringW(L"ReturnalGyro: runtime DLL or dependency could not load.");
    return 0;
}
}
extern "C" void* rgResolveVersion(unsigned index){
    // Lazy outside DllMain. All API arguments are preserved by the assembly tail-call stubs.
    static const auto exports=[](){
        std::array<void*,17> result{};wchar_t directory[32768]{};GetSystemDirectoryW(directory,32768);
        auto module=LoadLibraryW((std::wstring(directory)+L"\\version.dll").c_str());
        for(size_t i=0;i<result.size();++i){auto address=module?GetProcAddress(module,names[i]):nullptr;result[i]=address?reinterpret_cast<void*>(address):reinterpret_cast<void*>(&unavailable);}
        return result;
    }();
    return index<exports.size()?exports[index]:reinterpret_cast<void*>(&unavailable);
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH&&rg::isReturnalProcess()){ownModule=instance;auto thread=CreateThread(nullptr,0,loadMod,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;
}
