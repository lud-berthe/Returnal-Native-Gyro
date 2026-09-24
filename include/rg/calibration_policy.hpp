#pragma once
#include "controller_buttons.hpp"
#include <cstdint>
namespace rg {
// INI value 1 keeps the previous enabled/anytime behavior. UI order is separate.
constexpr bool automaticCalibrationAllowed(int mode,bool menuOpen,bool external){
 return !external&&(mode==1||(mode==2&&menuOpen));
}
constexpr bool freshCalibrationMenu(std::uint64_t at,std::uint64_t now){
 return at&&now>=at&&now-at<100'000'000;
}
// Actor ticks can stop in paused menus. The UI publishes an independent,
// expiring heartbeat; a gameplay tick must not overwrite that evidence.
constexpr bool calibrationMenuContext(std::uint64_t controllerAt,std::uint64_t uiAt,std::uint64_t now){
 return freshCalibrationMenu(controllerAt,now)||freshCalibrationMenu(uiAt,now);
}
constexpr bool calibrationMenuWidgetEligible(bool valid,bool visible,bool inViewport,bool foreground,bool suspended){
 return valid&&visible&&inViewport&&foreground&&!suspended;
}
constexpr bool calibrationPromptVisible(bool gyroPage,int presentation,bool external){
 return gyroPage&&presentation==1&&!external;
}
// EControllerVendor values from the supported Returnal profile. Unknown layouts
// retain the native icon; Steam uses Xbox-style face labels in this game.
constexpr int calibrationIconVendor(ControllerLayout layout){
 switch(layout){case ControllerLayout::Sony:return 1;case ControllerLayout::Nintendo:return 5;
 case ControllerLayout::Steam:case ControllerLayout::Xbox:return 3;default:return -1;}
}
struct ControllerGlyphStyle {bool sony;unsigned char vendor;};
constexpr ControllerGlyphStyle controllerGlyphStyle(bool controller,bool identityFresh,ControllerLayout layout,bool nativeSony,unsigned char nativeVendor){
 int vendor=calibrationIconVendor(layout);
 if(!controller||!identityFresh||vendor<0)return {nativeSony,nativeVendor};
 return {vendor==1||vendor==2,static_cast<unsigned char>(vendor)};
}
}
