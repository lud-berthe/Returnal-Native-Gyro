#pragma once
#include "settings.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>
namespace rg {
inline int menuCategory(std::string_view name){
 if(name=="MixedInput"||name=="IgnoreMouseMotion"||name=="HudMode")return 2;
 if(name=="GyroInputMode"||name=="SensorBackend"||name=="DeviceIndex"||name=="UIScale"||name=="DiagnosticLogging"||name=="DebugOverlay"||name=="VerboseSamples")return 3;
 if(name=="AutomaticCalibration"||name=="CalibrationSeconds"||name.starts_with("Tightening")||name.starts_with("Smooth")||name.starts_with("Cutoff")||name.starts_with("Acceleration")||name=="MinSensitivity"||name=="MaxSensitivity")return 1;
 return 0;
}
inline double menuStep(const SettingInfo& field,bool fine){
 if(field.integral)return 1;
 std::string_view name=field.name;
 if(name=="UIScale")return .25;
 if(name=="SmoothingSeconds")return fine?.001:.005;
 if(name=="RightStickThreshold")return fine?.01:.05;
 if(name.ends_with("Dps"))return fine?.1:1.;
 return fine?.01:.1;
}
inline void adjustMenuSetting(Settings& settings,const SettingInfo& field,int direction,bool fine){
 double value=field.get(settings);
 if(field.integral&&field.minimum==0&&field.maximum==1)value=value==0?1:0;
 else value=std::round((value+direction*menuStep(field,fine))*1000000.)/1000000.;
 field.set(settings,std::clamp(value,field.minimum,field.maximum));
}
}
