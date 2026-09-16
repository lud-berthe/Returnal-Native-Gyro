#pragma once
namespace rg {
// EInputTriggerStateFlags::FocusAim=1. Fire/AltFire alone are not the aim command.
constexpr bool aimCommandHeld(unsigned flags){return (flags&1u)!=0;}
constexpr bool gyroAimModeAllows(int mode,bool aimHeld){return mode==1?aimHeld:mode==2?!aimHeld:true;}
}
