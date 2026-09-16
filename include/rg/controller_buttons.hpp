#pragma once
#include <array>
#include "settings.hpp"
#include <cstdint>
#include <span>
namespace rg {
enum class ControllerLayout { Sony, Xbox, Nintendo, Generic, Steam };
// IDs are persistent INI values. UI order must not change saved bindings.
inline constexpr std::array<int,26> activationButtonOrder{0,1,2,3,4,10,11,7,5,12,13,16,14,15,19,17,18,20,21,22,23,24,25,26,27,28};
constexpr std::uint32_t buttonMask(int id){return id>0&&id<=30?1u<<(id-1):0;}
inline constexpr std::uint32_t allGyroButtons=(1u<<30)-1;
inline constexpr std::uint32_t standardGyroButtons=buttonMask(1)|buttonMask(2)|buttonMask(3)|buttonMask(4)|buttonMask(7)|buttonMask(10)|buttonMask(11);
inline constexpr std::uint32_t steamContactButtons=buttonMask(5)|buttonMask(12)|buttonMask(13)|buttonMask(14)|buttonMask(15)|buttonMask(16)|buttonMask(17)|buttonMask(18)|buttonMask(19);
inline constexpr std::uint32_t analogStickCapabilities=buttonMask(29)|buttonMask(30);
inline constexpr std::uint32_t rearGyroButtons=buttonMask(20)|buttonMask(21)|buttonMask(22)|buttonMask(23);
inline std::uint32_t pairedButtonCapabilities(std::uint32_t available){
 const int pairs[][3]={{12,13,24},{14,15,25},{17,18,26},{3,4,27},{3,4,28}};
 for(auto& p:pairs)if((available&buttonMask(p[0]))&&(available&buttonMask(p[1])))available|=buttonMask(p[2]);
 return available;
}
inline bool gyroButtonHeld(std::uint32_t buttons,int id){
 auto has=[&](int n){return (buttons&buttonMask(n))!=0;};
 switch(id){case 5:return has(5)||has(12)||has(13);case 16:return has(14)||has(15)||has(16);case 19:return has(17)||has(18)||has(19);
 case 24:return has(12)&&has(13);case 25:return has(14)&&has(15);case 26:return has(17)&&has(18);case 27:return has(3)||has(4);case 28:return has(3)&&has(4);default:return has(id);}
}
inline bool gyroStickHeld(float left,float right,const Settings& s){
 bool l=left>s.RightStickThreshold,r=right>s.RightStickThreshold;
 switch(s.GyroStick){case 3:return l;case 4:return r;case 27:return l||r;case 28:return l&&r;default:return false;}
}
inline bool gyroActivationHeld(std::uint32_t buttons,const Settings& s,float left=0,float right=0){
 return gyroButtonHeld(buttons,s.GyroButton)||gyroButtonHeld(buttons,s.GyroTouchpad)||gyroButtonHeld(buttons,s.GyroStickSensor)||gyroButtonHeld(buttons,s.GyroGripSensor)||gyroStickHeld(left,right,s)||gyroButtonHeld(buttons,s.ActivationButton);
}
inline std::uint32_t gyroGameButtonSelection(const Settings& s,std::uint32_t available=allGyroButtons){
 available=pairedButtonCapabilities(available);
 auto result=(buttonMask(s.GyroButton)|buttonMask(s.ActivationButton))&available;

 return result;
}
// Payload begins after the USB/Bluetooth report header; caller validates framing/CRC.
inline std::uint32_t sonyButtons(std::span<const unsigned char> p,bool ps5,bool edge=false){
 const std::size_t firstContact=ps5?32:34;
 if(p.size()<=firstContact+4)return 0;
 const std::size_t b=ps5?7:4;std::uint32_t result=0;
 if(p[b+1]&1)result|=buttonMask(1);
 if(p[b+1]&2)result|=buttonMask(2);
 if(p[b+1]&0x40)result|=buttonMask(3);
 if(p[b+1]&0x80)result|=buttonMask(4);
 // Bit 7 is clear while a finger touches the surface; mechanical click is ignored.
 if(!(p[firstContact]&0x80)||!(p[firstContact+4]&0x80))result|=buttonMask(5);
 if(p[b]&0x20)result|=buttonMask(6);
 if(p[b]&0x40)result|=buttonMask(7);
 if(p[ps5?4:7]>127)result|=buttonMask(8);
 if(p[ps5?5:8]>127)result|=buttonMask(9);
 if(p[b]&0x10)result|=buttonMask(10);
 if(p[b]&0x80)result|=buttonMask(11);
 if(ps5&&edge){
  if(p[b+2]&0x40)result|=buttonMask(20);
  if(p[b+2]&0x80)result|=buttonMask(21);
  if(p[b+2]&0x10)result|=buttonMask(22);
  if(p[b+2]&0x20)result|=buttonMask(23);
 }
 return result;
}
}
