#include "rg/runtime.hpp"
#include "rg/process_identity.hpp"
#include "rg/motion_session.hpp"
#include <thread>
#include <sstream>
#include <iomanip>
namespace rg {
void sensorLoop(Runtime& r){
    auto settings=r.snapshot();unsigned revision=r.revision.load();
    auto backend=makeMotionBackend(settings.SensorBackend);
    MotionProcessor processor;MotionSession motionSession;bool wasActive=false;std::uint64_t epoch=1;
    std::uint64_t lastPublish=0,lastLog=0;std::string previousError;
    for(;;){
        auto nextRevision=r.revision.load(std::memory_order_acquire);
        if(revision!=nextRevision){settings=r.snapshot();revision=nextRevision;}
        if(!backend->connected()){
            r.sensorActive=false;r.sampleTime=0;r.controllerButtons=0;r.availableButtons=0;r.controllerTouchpad=false;wasActive=false;r.motionEpoch=++epoch;
            if(!backend->connect(settings.DeviceIndex)){
                auto error=backend->error();if(error!=previousError){r.log(error);previousError=error;}
                {std::lock_guard lock(r.diagnosticsMutex);r.device=error;r.diagnostics={};}
                Sleep(1000);continue;
            }
            previousError.clear();auto info=backend->info();
            if(motionSession.reconnect(info.path,info.externalCalibration,monotonicNs())){
                processor.resumeDevice();r.log("Motion orientation preserved: same Steam controller resumed within 2 seconds; timing and queued motion cleared");
            }else{
                processor.resetDevice();r.log("Motion orientation reset: initial/new device, calibration owner change or expired stream; fresh acceleration will initialize gravity");
            }
            processor.setExternalCalibration(info.externalCalibration);r.externalCalibration.store(info.externalCalibration,std::memory_order_release);
            r.log(info.externalCalibration?"Calibration owner: Steam Input; mod manual/automatic calibration bypassed":"Calibration owner: mod (direct sensor)");
            r.controllerLayout.store(info.layout);r.controllerTouchpad.store(info.touchpad);
            std::ostringstream message;message<<info.name<<" VID="<<std::hex<<info.vendor<<" PID="<<info.product<<std::dec<<" Bluetooth="<<(info.connectionKnown?std::to_string(info.bluetooth):"unknown")<<" factoryCalibration="<<info.factoryCalibration;
            r.log(message.str());{std::lock_guard lock(r.diagnosticsMutex);r.device=message.str();}
        }
        auto command=r.calibrationCommand.exchange(0);
        if(command&&r.externalCalibration.load()){r.log("Calibration request ignored: use Steam Input gyro calibration");command=0;}
        if(command==1){processor.beginCalibration();r.log("Manual calibration started; keep controller still");}
        if(command==2){processor.resetCalibration();r.log("Calibration reset");}
        auto sample=backend->read();if(!sample)continue;motionSession.sample(monotonicNs());
        auto capabilities=backend->info();r.controllerTouchpad.store(capabilities.touchpad);
        auto previousButtons=r.availableButtons.exchange(capabilities.availableButtons);
        if(previousButtons!=capabilities.availableButtons)r.log("Gyro button capabilities="+std::to_string(capabilities.availableButtons));
        r.controllerButtons.store(sample->buttons,std::memory_order_release);
        auto now=monotonicNs();auto gameAt=r.gameTime.load(std::memory_order_acquire);
        auto flags=r.gameFlags.load();
        GameplayState game{(flags&1)!=0&&(now-gameAt)<100'000'000,(flags&2)!=0,(flags&4)!=0,r.panelOpen.load()||r.suspended.load(),(flags&8)!=0};
        auto delta=processor.process(*sample,settings,game);
        r.toggleEnabled.store(processor.toggleEnabled(),std::memory_order_release);
        bool active=processor.diagnostics().active;
        if(wasActive&&!active){++epoch;r.motionEpoch.store(epoch,std::memory_order_release);}
        wasActive=active;r.sensorActive.store(active,std::memory_order_release);
        r.sampleTime.store(now,std::memory_order_release);
        if((delta.yawDegrees!=0||delta.pitchDegrees!=0)&&!r.queue.push({delta,sample->arrivalNs,epoch}))++r.queueDrops;
        if(now-lastPublish>50'000'000){std::lock_guard lock(r.diagnosticsMutex);r.diagnostics=processor.diagnostics();lastPublish=now;}
        if(settings.VerboseSamples){std::ostringstream out;out<<"sample "<<sample->sensorNs<<" raw="<<sample->degreesPerSecond.x<<","<<sample->degreesPerSecond.y<<","<<sample->degreesPerSecond.z<<" delta="<<delta.yawDegrees<<","<<delta.pitchDegrees;r.log(out.str());}
        if(settings.DiagnosticLogging&&now-lastLog>2'000'000'000){
            const auto& d=processor.diagnostics();std::ostringstream out;
            out<<"state frames="<<r.cameraCalls.load()<<" blocked="<<r.blockedReasons.load()<<" flags="<<flags<<" active="<<d.active<<" enabled="<<settings.GyroEnabled<<" activation="<<settings.ActivationMode<<" gyroButton="<<settings.GyroButton<<" gyroPad="<<settings.GyroTouchpad<<" gyroStickSensor="<<settings.GyroStickSensor<<" gyroGrip="<<settings.GyroGripSensor<<" gyroStick="<<settings.GyroStick<<" toggle="<<processor.toggleEnabled()<<" ratchet="<<settings.RatchetButton<<" buttons="<<sample->buttons<<" Hz="<<d.sensorHz<<" gravity="<<d.gravity.x<<","<<d.gravity.y<<","<<d.gravity.z<<" calibration="<<static_cast<int>(d.calibration)<<" progress="<<d.calibrationProgress<<" inputCalls="<<r.inputCalls.load()<<" suppressed="<<r.suppressed.load()<<" HUD="<<r.presentation.load()<<" camera="<<r.controlYaw.load()<<","<<r.controlPitch.load()<<" applied="<<r.appliedYaw.load()<<","<<r.appliedPitch.load()<<" dropped="<<r.queueDrops.load()<<" flick="<<settings.FlickStick<<" flickXY="<<r.flickX.load()<<","<<r.flickY.load()<<" flickYaw="<<r.flickYaw.load()<<" stickSuppressed="<<r.flickSuppressed.load();
            r.log(out.str());lastLog=now;
        }
    }
}
DWORD WINAPI boot(void* argument){
    auto module=static_cast<HMODULE>(argument);
    // Pin before installing hooks; F10 suspends functionality without unsafe in-flight DLL unloading.
    HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&boot),&pinned);
    runtime=new Runtime;
    wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);runtime->directory=std::filesystem::path(path).parent_path();
    runtime->logFile.open(runtime->directory/"ReturnalGyro-runtime.log",std::ios::trunc);
    auto config=runtime->directory/"ReturnalGyro.ini";
    auto loaded=loadConfig(config);runtime->update(loaded.settings);
    for(auto& warning:loaded.warnings)runtime->log(warning);
    if(!std::filesystem::exists(config)){std::string error;if(!saveConfig(config,loaded.settings,error))runtime->log(error);}
    try {
        bool bound=installBindings(*runtime);
        if(bound){installNativeMenu(*runtime);}
        {std::lock_guard lock(runtime->diagnosticsMutex);if(!bound)runtime->status="Unsupported build or binding failure; see runtime log";else if(!runtime->nativeReady.load())runtime->status="Hooks installed; game-thread validation pending (see Diagnostics)";}
        std::thread([=]{try{sensorLoop(*runtime);}catch(const std::exception& e){runtime->sensorActive=false;runtime->log(std::string("Sensor stopped: ")+e.what());}}).detach();
        controlLoop(*runtime);
    }catch(const std::exception& e){runtime->suspended=true;runtime->log(std::string("Runtime disabled: ")+e.what());}
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH&&rg::isReturnalProcess()){auto thread=CreateThread(nullptr,0,rg::boot,instance,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
