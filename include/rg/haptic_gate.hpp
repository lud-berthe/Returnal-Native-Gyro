#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
namespace rg {
// Supported cooked BP_PlayerController layout. Runtime FNames are resolved afresh.
inline constexpr size_t mouseRumbleCall=0x2b22;
inline constexpr size_t controllerRumbleCall=0x2b71;
inline constexpr size_t mouseAxisEntry=0x2b9a;
inline constexpr size_t rumbleCallSize=15;
template<class T> inline T scriptValue(std::span<const unsigned char> code,size_t at){T v{};if(at<=code.size()&&sizeof(T)<=code.size()-at)std::memcpy(&v,code.data()+at,sizeof(T));return v;}
inline bool rumbleCallMatches(std::span<const unsigned char> code,size_t at,std::uint64_t name,bool enabled){
 if(at>code.size()||rumbleCallSize>code.size()-at)return false;
 return code[at]==0x45&&scriptValue<std::uint32_t>(code,at+1)==static_cast<std::uint32_t>(name)&&scriptValue<std::uint32_t>(code,at+9)==static_cast<std::uint32_t>(name>>32)&&code[at+13]==(enabled?0x27:0x28)&&code[at+14]==0x16;
}
inline bool validMouseRumbleBranch(std::span<const unsigned char> graph,std::span<const unsigned char> mouseAxis,std::uintptr_t graphObject,std::uint64_t toggleName){
 return graph.size()==13232&&mouseAxis.size()==36&&mouseAxis[18]==0x46&&scriptValue<std::uintptr_t>(mouseAxis,19)==graphObject&&mouseAxis[27]==0x1d&&scriptValue<std::uint32_t>(mouseAxis,28)==mouseAxisEntry&&mouseAxis[32]==0x16&&mouseAxis[33]==0x04&&mouseAxis[34]==0x0b&&mouseAxis[35]==0x53&&rumbleCallMatches(graph,mouseRumbleCall,toggleName,false)&&rumbleCallMatches(graph,controllerRumbleCall,toggleName,true);
}
}
