#include "rg/runtime.hpp"
#include "rg/ipc.hpp"
#include <vector>
namespace rg {
void controlLoop(Runtime& r){
    auto pid=GetCurrentProcessId();SettingsChannel channel(pid,true);
    if(!channel.valid()){r.log("Settings communication unavailable; native/mixed hooks remain loaded");return;}
    {auto state=channel.lock();if(!state)return;*state=SharedSettings{};state->magic=settingsMagic;state->abi=settingsAbi;state->gamePid=pid;sharedText(state->initialConfig,serializeConfig(r.snapshot()));}
    bool oldF9=false,oldF10=false;unsigned lastRequest=0,observedRevision=r.revision.load(),savedRevision=observedRevision;ULONGLONG lastChange=GetTickCount64();
    r.log("Control service ready: native settings; F9 calibrate; F10 suspend.");
    for(;;){
        DWORD foreground{};GetWindowThreadProcessId(GetForegroundWindow(),&foreground);bool focused=foreground==pid;
        bool f9=(GetAsyncKeyState(VK_F9)&0x8000)!=0,f10=(GetAsyncKeyState(VK_F10)&0x8000)!=0;
        if(focused&&f9&&!oldF9)r.calibrationCommand=1;
        if(focused&&f10&&!oldF10){r.suspended=!r.suspended.load();r.log(r.suspended?"Mod suspended":"Mod resumed");}
        oldF9=f9;oldF10=f10;
        {
            auto state=channel.lock();
            if(state){
                if(state->requestVersion!=lastRequest){
                    state->requestedConfig[sizeof(state->requestedConfig)-1]=0;
                    auto parsed=parseConfig(state->requestedConfig);r.update(parsed.settings);
                    sharedText(state->initialConfig,serializeConfig(r.snapshot()));
                    lastRequest=state->requestVersion;state->acceptedVersion=lastRequest;
                    for(auto& warning:parsed.warnings)r.log(warning);
                    r.log("Settings applied from helper; native gyro="+std::to_string(parsed.settings.GyroEnabled));
                }
                auto command=state->command;state->command=0;if(command==1||command==2)r.calibrationCommand=command;
                state->visible=r.panelOpen;state->uiActuallyVisible=r.panelActuallyVisible;state->uiFrames=r.panelFrames;state->heartbeat=GetTickCount64();
                state->nativeReady=r.nativeReady;state->mixedReady=r.mixedReady;state->suspended=r.suspended;
                state->gameFlags=r.gameFlags;state->blocked=r.blockedReasons;state->presentation=static_cast<unsigned>(r.presentation.load());state->suppressed=r.suppressed;
                state->yaw=r.controlYaw;state->pitch=r.controlPitch;state->appliedYaw=r.appliedYaw;state->appliedPitch=r.appliedPitch;
                {std::lock_guard lock(r.diagnosticsMutex);state->motion=r.diagnostics;sharedText(state->device,r.device);sharedText(state->status,r.status);}
            }
        }
        // Persist away from the game/render thread; debounce held spinner input.
        auto revision=r.revision.load();auto now=GetTickCount64();
        if(revision!=observedRevision){observedRevision=revision;lastChange=now;}
        if(observedRevision!=savedRevision&&now>=lastChange&&now-lastChange>=300){std::string error;if(saveConfig(r.directory/"ReturnalGyro.ini",r.snapshot(),error)){savedRevision=observedRevision;r.log("Gyro configuration saved");}else{r.log(error);lastChange=now+2000;}}
        Sleep(20);
    }
}
}
