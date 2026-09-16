#pragma once
#include "motion.hpp"
#include <windows.h>
#include <cstdint>
#include <string>
#include <cstring>
namespace rg {
// Local, per-game-process transport. Fixed-size text is validated before applying settings.
// The named mutex is used only by control/UI workers, never the game or sensor callbacks.
struct SharedSettings {
    std::uint32_t magic{},abi{},gamePid{},uiPid{},requestVersion{},acceptedVersion{};
    std::uint32_t command{},gameFlags{},blocked{},presentation{};
    bool exitRequested{},uiActuallyVisible{},visible{},nativeReady{},mixedReady{},suspended{};
    std::uint64_t uiFrames{},heartbeat{},suppressed{};
    float yaw{},pitch{},appliedYaw{},appliedPitch{};
    MotionDiagnostics motion{};
    char initialConfig[16384]{},requestedConfig[16384]{},device[512]{},status[512]{};
};
class SettingsChannel {
    HANDLE mapping_{},mutex_{};SharedSettings* data_{};
public:
    SettingsChannel(DWORD gamePid,bool create);
    ~SettingsChannel();
    SettingsChannel(const SettingsChannel&)=delete;
    SettingsChannel& operator=(const SettingsChannel&)=delete;
    bool valid()const{return data_&&mutex_;}
    class Guard {
        HANDLE mutex_{};SharedSettings* data_{};
    public:
        Guard(HANDLE mutex,SharedSettings* data);
        ~Guard(){if(data_)ReleaseMutex(mutex_);}
        explicit operator bool()const{return data_!=nullptr;}
        SharedSettings* operator->()const{return data_;}
        SharedSettings& operator*()const{return *data_;}
    };
    Guard lock(){return Guard(mutex_,data_);}
};
template<size_t N>void sharedText(char(&destination)[N],const std::string& value){
    size_t length=(value.size()<N-1)?value.size():N-1;std::memcpy(destination,value.data(),length);destination[length]=0;
}
constexpr std::uint32_t settingsMagic=0x52475952,settingsAbi=1;
}
