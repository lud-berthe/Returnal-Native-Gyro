#pragma once
#include "backend.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
namespace rg {
inline ControllerPresentation sdlControllerPresentation(SDL_Gamepad* p){
 using L=ControllerLayout;using D=ControllerDiagram;
 switch(SDL_GetGamepadType(p)){
 case SDL_GAMEPAD_TYPE_PS3:return {L::Sony,D::Native};
 case SDL_GAMEPAD_TYPE_PS4:return {L::Sony,D::DualShock};
 case SDL_GAMEPAD_TYPE_PS5:return {L::Sony,D::DualSense};
 case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
 case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:return {L::Nintendo,D::SwitchPro};
 case SDL_GAMEPAD_TYPE_XBOX360:return {L::Xbox,D::Xbox360};
 case SDL_GAMEPAD_TYPE_XBOXONE:return {L::Xbox,D::Xbox};
 case SDL_GAMEPAD_TYPE_STANDARD:return {L::Xbox,D::Native};
 default:return {};
 }
}
inline std::uint32_t sdlAvailableButtons(SDL_Gamepad* p){
 std::uint32_t result=0;
 const std::pair<SDL_GamepadButton,int> mapping[]={{SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,1},{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,2},{SDL_GAMEPAD_BUTTON_LEFT_STICK,3},{SDL_GAMEPAD_BUTTON_RIGHT_STICK,4},{SDL_GAMEPAD_BUTTON_EAST,7},{SDL_GAMEPAD_BUTTON_WEST,10},{SDL_GAMEPAD_BUTTON_NORTH,11},{SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,20},{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,21},{SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,22},{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,23}};
 for(auto [button,id]:mapping)if(SDL_GamepadHasButton(p,button))result|=buttonMask(id);
 if(SDL_GetNumGamepadTouchpads(p)>0)result|=buttonMask(5);
 if(SDL_GamepadHasAxis(p,SDL_GAMEPAD_AXIS_LEFTX)&&SDL_GamepadHasAxis(p,SDL_GAMEPAD_AXIS_LEFTY))result|=buttonMask(29);
 if(SDL_GamepadHasAxis(p,SDL_GAMEPAD_AXIS_RIGHTX)&&SDL_GamepadHasAxis(p,SDL_GAMEPAD_AXIS_RIGHTY))result|=buttonMask(30);
 return result;
}
inline void readSdlControls(SDL_Gamepad* p,GyroSample& s){
 const std::pair<SDL_GamepadButton,int> mapping[]={
  {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,1},{SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,2},
  {SDL_GAMEPAD_BUTTON_LEFT_STICK,3},{SDL_GAMEPAD_BUTTON_RIGHT_STICK,4},
  {SDL_GAMEPAD_BUTTON_SOUTH,6},{SDL_GAMEPAD_BUTTON_EAST,7},
  {SDL_GAMEPAD_BUTTON_WEST,10},{SDL_GAMEPAD_BUTTON_NORTH,11},{SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,20},{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,21},{SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,22},{SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,23}};
 for(auto [button,id]:mapping)if(SDL_GetGamepadButton(p,button))s.buttons|=buttonMask(id);
 for(int pad=0;pad<SDL_GetNumGamepadTouchpads(p);++pad)
  for(int finger=0;finger<SDL_GetNumGamepadTouchpadFingers(p,pad);++finger){
   bool down=false;if(SDL_GetGamepadTouchpadFinger(p,pad,finger,&down,nullptr,nullptr,nullptr)&&down)s.buttons|=buttonMask(5);
  }
 if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFT_TRIGGER)>16384)s.buttons|=buttonMask(8);
 if(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)>16384)s.buttons|=buttonMask(9);
 s.leftStickMagnitude=std::min(1.0f,std::hypot(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTX)/32768.0f,SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_LEFTY)/32768.0f));
 s.rightStickMagnitude=std::min(1.0f,std::hypot(SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_RIGHTX)/32768.0f,SDL_GetGamepadAxis(p,SDL_GAMEPAD_AXIS_RIGHTY)/32768.0f));
}
}
