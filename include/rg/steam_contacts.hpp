#pragma once
#include "controller_buttons.hpp"
#include <optional>
#include <array>
#include <algorithm>
namespace rg {
// Valve Triton input reports, documented by SDL's Steam Triton HID driver.
// Read contact and rear-button flags. Never interpret pressure/click as touch.
inline std::optional<std::uint32_t> steamContactReport(std::span<const unsigned char> report){
 if(report.size()<46||(report[0]!=0x42&&report[0]!=0x45&&report[0]!=0x47))return {};
 std::uint32_t flags=report[2]|(std::uint32_t(report[3])<<8)|(std::uint32_t(report[4])<<16)|(std::uint32_t(report[5])<<24), result=0;
 if(flags&0x02000000)result|=buttonMask(12);
 if(flags&0x00200000)result|=buttonMask(13);
 if(flags&0x02200000)result|=buttonMask(5);
 if(flags&0x01000000)result|=buttonMask(14);
 if(flags&0x00100000)result|=buttonMask(15);
 if(flags&0x01100000)result|=buttonMask(16);
 if(flags&0x20000000)result|=buttonMask(17);
 if(flags&0x10000000)result|=buttonMask(18);
 if(flags&0x30000000)result|=buttonMask(19);
 if(flags&0x00020000)result|=buttonMask(20);
 if(flags&0x00000080)result|=buttonMask(21);
 if(flags&0x00040000)result|=buttonMask(22);
 if(flags&0x00000100)result|=buttonMask(23);
 return result;
}
inline constexpr std::uint32_t steamTrackpadButtons=buttonMask(5)|buttonMask(12)|buttonMask(13);
constexpr bool steamFirstGeneration(unsigned product){return product==0x1102||product==0x1106||product==0x1142;}
constexpr bool steamContactDevice(unsigned vendor,unsigned product){return vendor==0x28de&&(steamFirstGeneration(product)||product==0x1302);}
inline std::uint32_t steamFirstTrackpads(std::uint32_t flags,bool interleaved){
 bool left=(flags&0x00080000)||(interleaved&&(flags&0x00800000)),right=(flags&0x00100000)!=0;
 return (left?buttonMask(12):0)|(right?buttonMask(13):0)|((left||right)?buttonMask(5):0)|((flags&0x8000)?buttonMask(20):0)|((flags&0x10000)?buttonMask(21):0);
}
// First-generation USB/dongle reports carry a versioned header and full buttons.
inline std::optional<std::uint32_t> steamFirstUsbContactReport(std::span<const unsigned char> r){
 if(r.size()<16||r[0]!=1||r[1]!=0||(r[2]!=1&&r[2]!=7)||r[3]<12||r[3]>r.size()-4)return {};
 std::uint32_t flags=r[8]|(std::uint32_t(r[9])<<8)|(std::uint32_t(r[10])<<16);
 return steamFirstTrackpads(flags,true);
}
class SteamFirstContactDecoder {
 std::array<unsigned char,144> payload_{};unsigned expected_{};bool known_{};std::uint32_t flags_{};std::uint64_t segmentAt_{},packetAt_{};
 std::optional<std::uint32_t> packet(std::span<const unsigned char> p){
  if(p.size()<2||(p[0]&15)!=4)return {};
  unsigned chunks=(p[0]&0xf0)|(unsigned(p[1])<<8);
  if(chunks&0xe000)return {};
  const unsigned sizes[]={3,2,3,4,4,4,6,6,8};size_t required=2;
  for(unsigned i=0;i<9;++i)if(chunks&(0x10u<<i))required+=sizes[i];
  if(required>p.size())return {};
  if(chunks&0x10){flags_=p[2]|(std::uint32_t(p[3])<<8)|(std::uint32_t(p[4])<<16);known_=true;}
  // BLE updates omit unchanged chunks: retain known contacts on other state packets.
  return known_?std::optional<std::uint32_t>(steamFirstTrackpads(flags_,false)):std::nullopt;
 }
public:
 bool invalidated{};
 void reset(){expected_=0;known_=false;flags_=0;segmentAt_=packetAt_=0;}
 std::optional<std::uint32_t> feed(std::span<const unsigned char> r,bool bluetooth,std::uint64_t now){
  invalidated=false;
  if(!bluetooth){
   if(r.size()>=5&&r[0]==1&&r[1]==0&&r[2]==3&&r[3]>=1&&r[4]==1){reset();invalidated=true;return {};}
   return steamFirstUsbContactReport(r);
  }
  if(r.empty()||r[0]!=3)return {};
  if(r.size()!=20){reset();invalidated=true;return {};}
  unsigned h=r[1],segment=h&7;
  if(!(h&0x80))return {};
  if((expected_&&(!segmentAt_||now<segmentAt_||now-segmentAt_>100'000'000))||(packetAt_&&(now<packetAt_||now-packetAt_>=200'000'000))){reset();invalidated=true;}
  if(segment!=expected_){reset();invalidated=true;if(segment)return {};}
  std::copy_n(r.begin()+2,18,payload_.begin()+segment*18);segmentAt_=now;
  if(h&0x40){expected_=0;auto value=packet({payload_.data(),(segment+1)*18});if(value)packetAt_=now;return value;}
  if(segment==7){reset();invalidated=true;return {};}
  expected_=segment+1;return {};
 }
};
inline bool steamContactInterface(unsigned vendor,unsigned product,unsigned page,unsigned usage,int iface){
 if(vendor!=0x28de||page!=0xff00||usage!=1)return false;
 if(product==0x1106)return true; // Bluetooth HID vendor collection
 if(product==0x1102)return iface==2;
 if(product==0x1142)return iface>=1&&iface<=4;
 if(product==0x1304||product==0x1305)return iface>=2&&iface<=5;
 return product==0x1302; // Steam Controller 2026 USB/Bluetooth
}
// Never guess which controller a raw stream belongs to in an ambiguous setup.
constexpr bool steamContactAssociation(unsigned steamControllers,unsigned physicalStreams){return steamControllers==1&&physicalStreams==1;}
constexpr bool steamContactFresh(std::uint64_t last,std::uint64_t now){return last&&now>=last&&now-last<200'000'000;}
}
