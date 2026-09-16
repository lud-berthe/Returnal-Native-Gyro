#include "rg/backend.hpp"
#include "rg/sdl_controls.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <numbers>
#include <vector>
namespace rg {
namespace {
class SdlBackend final:public MotionBackend {
    SDL_Gamepad* pad_{};SDL_JoystickID id_{};DeviceInfo info_;std::string error_;Vec3 accel_{0,1,0};
    std::uint64_t accelTime_{};bool initialized_{};
    void close(){if(pad_)SDL_CloseGamepad(pad_);pad_=nullptr;}
public:
    ~SdlBackend()override{close();if(initialized_)SDL_QuitSubSystem(SDL_INIT_GAMEPAD);}
    bool connect(int index) override {
        close();
        if(!initialized_) {
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
            // Sony is handled by the passive backend. Avoid any SDL Sony output ownership.
            SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4,"0");SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"0");
            initialized_=SDL_InitSubSystem(SDL_INIT_GAMEPAD);
            if(!initialized_){error_=SDL_GetError();return false;}
        }
        SDL_UpdateGamepads();int count=0;auto ids=SDL_GetGamepads(&count);
        std::vector<SDL_JoystickID> choices;
        for(int i=0;i<count;++i)if(SDL_GetGamepadVendorForID(ids[i])!=0x054c)choices.push_back(ids[i]);
        SDL_free(ids);
        std::sort(choices.begin(),choices.end(),[](auto a,auto b){const char* pa=SDL_GetGamepadPathForID(a);const char* pb=SDL_GetGamepadPathForID(b);return std::string(pa?pa:"")<std::string(pb?pb:"");});
        int found=0;
        for(auto id:choices) {
            auto p=SDL_OpenGamepad(id);if(!p)continue;
            if(!SDL_GamepadHasSensor(p,SDL_SENSOR_GYRO)||!SDL_GamepadHasSensor(p,SDL_SENSOR_ACCEL)||found++!=index){SDL_CloseGamepad(p);continue;}
            if(!SDL_SetGamepadSensorEnabled(p,SDL_SENSOR_ACCEL,true)||!SDL_SetGamepadSensorEnabled(p,SDL_SENSOR_GYRO,true)){error_=SDL_GetError();SDL_CloseGamepad(p);return false;}
            pad_=p;id_=id;const char* name=SDL_GetGamepadName(p);const char* path=SDL_GetGamepadPath(p);
            info_={name?name:"SDL gamepad",path?path:"",SDL_GetGamepadVendor(p),SDL_GetGamepadProduct(p),SDL_GetGamepadConnectionState(p)==SDL_JOYSTICK_CONNECTION_WIRELESS,true};
            switch(SDL_GetGamepadType(p)){
            case SDL_GAMEPAD_TYPE_PS3:case SDL_GAMEPAD_TYPE_PS4:case SDL_GAMEPAD_TYPE_PS5:info_.layout=ControllerLayout::Sony;break;
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:info_.layout=ControllerLayout::Nintendo;break;
            case SDL_GAMEPAD_TYPE_XBOX360:case SDL_GAMEPAD_TYPE_XBOXONE:case SDL_GAMEPAD_TYPE_STANDARD:info_.layout=ControllerLayout::Xbox;break;
            default:info_.layout=ControllerLayout::Generic;break;
            }
            info_.touchpad=SDL_GetNumGamepadTouchpads(p)>0;info_.availableButtons=sdlAvailableButtons(p);
            accelTime_=0;error_.clear();return true;
        }
        error_="No selected non-Sony SDL motion gamepad";return false;
    }
    std::optional<GyroSample> read() override {
        if(!pad_)return {};
        SDL_UpdateGamepads();SDL_Event event{};
        while(SDL_PeepEvents(&event,1,SDL_GETEVENT,SDL_EVENT_GAMEPAD_SENSOR_UPDATE,SDL_EVENT_GAMEPAD_SENSOR_UPDATE)==1) {
            if(event.gsensor.which!=id_)continue;
            if(event.gsensor.sensor==SDL_SENSOR_ACCEL){accel_={event.gsensor.data[0]/SDL_STANDARD_GRAVITY,event.gsensor.data[1]/SDL_STANDARD_GRAVITY,event.gsensor.data[2]/SDL_STANDARD_GRAVITY};accelTime_=event.gsensor.sensor_timestamp;continue;}
            if(event.gsensor.sensor!=SDL_SENSOR_GYRO||!accelTime_||event.gsensor.sensor_timestamp<accelTime_||event.gsensor.sensor_timestamp-accelTime_>50'000'000)continue;
            constexpr float toDegrees=180.0f/std::numbers::pi_v<float>;
            GyroSample s;s.sensorNs=event.gsensor.sensor_timestamp;s.arrivalNs=monotonicNs();s.accelG=accel_;
            s.degreesPerSecond={event.gsensor.data[0]*toDegrees,event.gsensor.data[1]*toDegrees,event.gsensor.data[2]*toDegrees};
            readSdlControls(pad_,s);
            return s;
        }
        SDL_FlushEvents(SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_TOUCHPAD_UP);
        if(!SDL_GamepadConnected(pad_)){error_="SDL motion gamepad disconnected";close();}
        SDL_Delay(1);return {};
    }
    bool connected()const override{return pad_!=nullptr;}
    DeviceInfo info()const override{return info_;}
    std::string error()const override{return error_;}
};
}
std::unique_ptr<MotionBackend> makeSdlBackend(){return std::make_unique<SdlBackend>();}
}
