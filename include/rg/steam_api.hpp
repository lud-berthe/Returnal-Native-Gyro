#pragma once
#include "steam_motion.hpp"
#include <windows.h>
#include <string>
namespace rg {
// Bind SteamInput001, whose vtable contract is independent of newer SDK flat wrappers.
// Never replace steam_api64.dll, initialize Steam globally, change action sets or shut
// down the game's shared Steam Input service. One instance lives for the process lifetime.
class SteamMotionApi {
 void* input_{};
 template<class F>F method(unsigned i)const{return reinterpret_cast<F>((*static_cast<void***>(input_))[i]);}
public:
 std::string error;
 bool initialize(){
  if(input_)return true;
  auto m=GetModuleHandleW(L"steam_api64.dll");if(!m){error="Steam API is not loaded in this process";return false;}
  auto user=reinterpret_cast<int(*)()>(GetProcAddress(m,"SteamAPI_GetHSteamUser"));
  auto find=reinterpret_cast<void*(*)(int,const char*)>(GetProcAddress(m,"SteamInternal_FindOrCreateUserInterface"));
  if(!user||!find||!user()){error="Waiting for the game's Steam user session";return false;}
  input_=find(user(),"SteamInput001");if(!input_){error="SteamInput001 is unavailable";return false;}
  if(!method<bool(*)(void*)>(0)(input_)){input_=nullptr;error="Steam Input initialization failed";return false;}
  error.clear();return true;
 }
 void update()const{method<void(*)(void*)>(2)(input_);}
 int controllers(std::uint64_t* h)const{return method<int(*)(void*,std::uint64_t*)>(3)(input_,h);}
 SteamMotionData motion(std::uint64_t h)const{
  SteamMotionData m;
  // Windows x64 member aggregate return: this, return buffer, input handle.
  method<SteamMotionData*(*)(void*,SteamMotionData*,std::uint64_t)>(20)(input_,&m,h);return m;
 }
 int type(std::uint64_t h)const{return method<int(*)(void*,std::uint64_t)>(26)(input_,h);}
 std::uint64_t controllerForSlot(int slot)const{return method<std::uint64_t(*)(void*,int)>(27)(input_,slot);}
 int slot(std::uint64_t h)const{return method<int(*)(void*,std::uint64_t)>(28)(input_,h);}
};
}
