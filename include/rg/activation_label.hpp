#pragma once
#include "gyro_menu.hpp"
#include <string>
#include <algorithm>
namespace rg {
inline std::string activationLabel(const Settings& settings,bool toggleEnabled,ControllerLayout layout=ControllerLayout::Sony,bool touchpad=true){
 if(!settings.GyroEnabled)return "OFF (disabled in settings)";
 switch(settings.ActivationMode){
 case 1:return "ON - Aim Only";
 case 2:return "ON - Hip-fire Only";
 case 3:return std::string("Hold ")+buttonLabel(settings.ActivationButton,"en",layout,touchpad)+" to enable";
 case 4:return std::string("Hold ")+buttonLabel(settings.ActivationButton,"en",layout,touchpad)+" to disable";
 case 5:return std::string("Toggle ")+(toggleEnabled?"ON":"OFF")+" - "+buttonLabel(settings.ActivationButton,"en",layout,touchpad);
 default:return "ON - Always On";
 }
}
}
