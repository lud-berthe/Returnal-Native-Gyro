#pragma once
#include <optional>
namespace rg {
// Native EInputTriggerStateFlags: FocusAim=1, Fire=2, AltFirePrep=4, AltFire=8.
// Separate/remapped Alt-Fire must count as aiming without requiring FocusAim.
// Ordinary Fire and lingering aim/Alt-Fire animation states do not count.
// The separate Alt-Fire action uses UAltFireComponent::bWantsToAltFireMode,
// not FInputTriggerState. A known released button is distinct from unavailable data.
constexpr std::optional<unsigned> combinedAimCommands(std::optional<unsigned> trigger,
                                                     std::optional<bool> altFireButtonRequest){
 if(!trigger&&!altFireButtonRequest)return {};
 unsigned flags=trigger.value_or(0);
 if(altFireButtonRequest.value_or(false))flags|=4u;
 return flags;
}
constexpr bool aimCommandHeld(unsigned flags){return (flags&(1u|4u|8u))!=0;}
constexpr bool gyroAimModeAllows(int mode,bool aimHeld){return mode==1?aimHeld:mode==2?!aimHeld:true;}
}
