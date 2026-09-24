#include "rg/backend.hpp"
#include "rg/steam_api.hpp"
#include "rg/steam_association.hpp"
#include "rg/sdl_controls.hpp"
#include "rg/steam_contact_reader.hpp"
#include <algorithm>
#include <array>
#include <vector>
namespace rg {
namespace {
SteamMotionApi& steam(){static SteamMotionApi api;return api;}
bool motionType(int t){
 switch(t){case 1:case 5:case 8:case 9:case 10:case 13:case 14:case 15:case 16:case 17:case 18:return true;default:return false;}
}
class SteamBackend final:public MotionBackend {
 SDL_Gamepad* pad_{};std::uint64_t handle_{},lastPoll_{},lastValid_{};bool initialized_{};int inputType_{};unsigned contactProduct_{};
 DeviceInfo info_;std::string error_;SteamContactReader contacts_;
 SteamGamepadIdentity identity(SDL_Gamepad* p)const{
  auto guid=SDL_GetJoystickGUID(SDL_GetGamepadJoystick(p));
  // SDL 3.4.16 GUID byte 14 identifies the actual backend, not the device name.
  return {SDL_GetGamepadSteamHandle(p),SDL_GetGamepadPlayerIndex(p),guid.data[14]=='x'};
 }
 void close(){contacts_.reset();if(pad_)SDL_CloseGamepad(pad_);pad_=nullptr;handle_=0;}
public:
 ~SteamBackend()override{close();if(initialized_)SDL_QuitSubSystem(SDL_INIT_GAMEPAD);}
 bool connect(int index)override{
  close();auto& api=steam();if(!api.initialize()){error_=api.error;return false;}
  api.update();std::array<std::uint64_t,16> handles{};int count=api.controllers(handles.data());
  if(count<0||count>16){error_="Invalid Steam Input controller count";return false;}
  std::vector<std::uint64_t> candidates;
  for(int i=0;i<count;++i)if(handles[i]&&motionType(api.type(handles[i])))candidates.push_back(handles[i]);
  std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){int sa=api.slot(a),sb=api.slot(b);if(sa<0)sa=16;if(sb<0)sb=16;return sa!=sb?sa<sb:a<b;});
  if(index<0||static_cast<size_t>(index)>=candidates.size()){error_="No selected Steam Input motion controller; enable Steam Input for Returnal";return false;}
  const auto selected=candidates[index];
  if(!initialized_){
   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
   SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS4,"0");SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5,"0");
   initialized_=SDL_InitSubSystem(SDL_INIT_GAMEPAD);if(!initialized_){error_=SDL_GetError();return false;}
  }
  SDL_UpdateGamepads();int n{};auto ids=SDL_GetGamepads(&n);
  int slot=api.slot(selected);auto owner=slot>=0&&slot<4?api.controllerForSlot(slot):0;
  std::vector<SDL_Gamepad*> opened;std::vector<SteamGamepadIdentity> identities;
  std::string association="; selected="+std::to_string(selected)+" slot="+std::to_string(slot)+" slotOwner="+std::to_string(owner)+" SDL count="+std::to_string(n);
  for(int i=0;i<n;++i){auto p=SDL_OpenGamepad(ids[i]);if(!p)continue;
   auto id=identity(p);opened.push_back(p);identities.push_back(id);
   association+=" ["+std::string(SDL_GetGamepadName(p)?SDL_GetGamepadName(p):"unknown")+" handle="+std::to_string(id.handle)+" player="+std::to_string(id.slot)+" xinput="+std::to_string(id.xinput)+"]";
  }
  SDL_free(ids);int selectedIndex=selectSteamGamepad(identities,selected,slot,owner);
  for(size_t i=0;i<opened.size();++i){if(static_cast<int>(i)==selectedIndex)pad_=opened[i];else SDL_CloseGamepad(opened[i]);}
  if(!pad_){error_="Steam motion found, waiting for its SDL gamepad (use a Gamepad layout and press a button)"+association;return false;}
  bool fallback=identities[selectedIndex].handle==0;
  handle_=selected;const char* name=SDL_GetGamepadName(pad_);int type=api.type(handle_);inputType_=type;
  info_={std::string(name?name:"Controller")+" [Steam Input]","steam:"+std::to_string(handle_),SDL_GetGamepadVendor(pad_),SDL_GetGamepadProduct(pad_),false,false};
  if(fallback){info_.name="Controller [Steam Input, verified XInput slot "+std::to_string(slot)+"]";info_.vendor=info_.product=0;}
  auto presentation=resolveSteamPresentation(type,info_.vendor,info_.product,sdlControllerPresentation(pad_),!fallback);info_.layout=presentation.layout;info_.diagram=presentation.diagram;
  contactProduct_=steamContactDevice(info_.vendor,info_.product)?info_.product:0;
  info_.name+=" type="+std::to_string(type)+" layout="+std::to_string(static_cast<int>(info_.layout))+" diagram="+std::to_string(static_cast<int>(info_.diagram))+" exactAssociation="+std::to_string(!fallback);
  info_.availableButtons=sdlAvailableButtons(pad_);if(steamFirstGeneration(info_.product))info_.availableButtons&=~(buttonMask(4)|buttonMask(30));info_.touchpad=(info_.availableButtons&buttonMask(5))!=0;info_.connectionKnown=false;info_.externalCalibration=true;
  lastPoll_=0;lastValid_=monotonicNs();error_.clear();return true;
 }
 std::optional<GyroSample> read()override{
  if(!pad_)return {};
  auto now=monotonicNs();if(now-lastPoll_<4'000'000){SDL_Delay(1);return {};}
  lastPoll_=now;auto& api=steam();api.update();SDL_UpdateGamepads();
  std::array<std::uint64_t,16> h{};int n=api.controllers(h.data());
  bool present=n>=0&&n<=16&&std::find(h.begin(),h.begin()+std::clamp(n,0,16),handle_)!=h.begin()+std::clamp(n,0,16);
  int slot=api.slot(handle_);auto owner=slot>=0&&slot<4?api.controllerForSlot(slot):0;
  if(!present||!SDL_GamepadConnected(pad_)||!steamGamepadMatches(identity(pad_),handle_,slot,owner)){error_="Steam Input motion controller disconnected or reassigned";close();return {};}
  auto sample=steamMotionSample(api.motion(handle_),now);
  if(!sample){if(now-lastValid_>2'000'000'000){error_="Steam Input motion data unavailable; reconnect controller";close();}return {};}
  lastValid_=now;readSdlControls(pad_,*sample);
  unsigned steamControllers=0;
  for(int i=0;i<n;++i){int t=api.type(h[i]);if(t==1||t==18)++steamControllers;}
  if(inputType_==1||inputType_==18)contacts_.update(now,steamControllers,contactProduct_);
  else contacts_.reset();
  if(contacts_.detectedProduct){info_.vendor=0x28de;info_.product=static_cast<std::uint16_t>(contacts_.detectedProduct);}
  sample->buttons|=contacts_.buttons;
  info_.availableButtons=sdlAvailableButtons(pad_)|contacts_.available;
  if((inputType_==1||inputType_==18)&&(!info_.product||steamFirstGeneration(info_.product))){info_.availableButtons&=~(buttonMask(4)|buttonMask(30));sample->buttons&=~buttonMask(4);sample->rightStickMagnitude=0;}
  info_.touchpad=(info_.availableButtons&buttonMask(5))!=0;
  // This mod does not use SDL's event queue for Steam samples. Keep it bounded.
  SDL_FlushEvents(SDL_EVENT_GAMEPAD_ADDED,SDL_EVENT_GAMEPAD_SENSOR_UPDATE);
  return sample;
 }
 bool connected()const override{return pad_!=nullptr;}
 DeviceInfo info()const override{return info_;}
 std::string error()const override{return error_;}
};
}
std::optional<ControllerPresentation> steamPresentation(int index){
 auto& api=steam();if(!api.initialize())return {};api.update();
 std::array<std::uint64_t,16> handles{};int n=api.controllers(handles.data());if(n<0||n>16)return {};
 std::vector<std::uint64_t> candidates;for(int i=0;i<n;++i)if(handles[i])candidates.push_back(handles[i]);
 std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){int sa=api.slot(a),sb=api.slot(b);if(sa<0)sa=16;if(sb<0)sb=16;return sa!=sb?sa<sb:a<b;});
 if(index<0||static_cast<size_t>(index)>=candidates.size())return {};
 auto presentation=steamControllerPresentation(api.type(candidates[index]));
 if(presentation.layout==ControllerLayout::Generic)return {};
 return presentation;
}
std::unique_ptr<MotionBackend> makeSteamBackend(){return std::make_unique<SteamBackend>();}
}
