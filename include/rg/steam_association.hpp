#pragma once
#include <cstdint>
#include <span>
namespace rg {
struct SteamGamepadIdentity {std::uint64_t handle{};int slot{-1};bool xinput{};};
// A player index alone is not an identity. Only the XInput backend plus Steam's
// forward and reverse slot mapping can substitute for a missing SDL Steam handle.
inline bool steamGamepadMatches(const SteamGamepadIdentity& p,std::uint64_t selected,int steamSlot,std::uint64_t slotOwner){
 if(!selected)return false;
 if(p.handle)return p.handle==selected;
 return p.xinput&&p.slot>=0&&p.slot<4&&steamSlot==p.slot&&slotOwner==selected;
}
inline int selectSteamGamepad(std::span<const SteamGamepadIdentity> pads,std::uint64_t selected,int steamSlot,std::uint64_t slotOwner){
 int exact=-1,fallback=-1;bool duplicateExact=false,duplicateFallback=false;
 for(size_t i=0;i<pads.size();++i){if(!steamGamepadMatches(pads[i],selected,steamSlot,slotOwner))continue;
  if(pads[i].handle){if(exact>=0)duplicateExact=true;exact=static_cast<int>(i);}
  else{if(fallback>=0)duplicateFallback=true;fallback=static_cast<int>(i);}
 }
 if(duplicateExact)return -1;if(exact>=0)return exact;return duplicateFallback?-1:fallback;
}
}
