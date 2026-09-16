#include "rg/ipc.hpp"
namespace rg {
SettingsChannel::SettingsChannel(DWORD pid,bool create){
    auto name=L"Local\\ReturnalGyroSettings_"+std::to_wstring(pid)+L"_v1";
    auto mutexName=name+L"_mutex";
    mutex_=create?CreateMutexW(nullptr,FALSE,mutexName.c_str()):OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,mutexName.c_str());
    mapping_=create?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(SharedSettings),name.c_str()):OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name.c_str());
    if(mapping_)data_=static_cast<SharedSettings*>(MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,sizeof(SharedSettings)));
}
SettingsChannel::~SettingsChannel(){if(data_)UnmapViewOfFile(data_);if(mapping_)CloseHandle(mapping_);if(mutex_)CloseHandle(mutex_);}
SettingsChannel::Guard::Guard(HANDLE mutex,SharedSettings* data):mutex_(mutex){
    if(!mutex||!data)return;auto result=WaitForSingleObject(mutex,50);if(result==WAIT_OBJECT_0||result==WAIT_ABANDONED)data_=data;
}
}
