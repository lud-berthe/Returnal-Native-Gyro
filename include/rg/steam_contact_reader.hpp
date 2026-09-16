#pragma once
#include "steam_contacts.hpp"
#include <SDL3/SDL_hidapi.h>
#include <algorithm>
#include <array>
#include <string>
#include <vector>
namespace rg {
// Passive supplementary contact reader. Steam retains all device configuration,
// motion calibration and outputs. No write/feature reports are sent here.
class SteamContactReader {
 struct Endpoint{SDL_hid_device* device{};std::string path;std::uint64_t last{};std::uint32_t buttons{};unsigned product{};bool bluetooth{};SteamFirstContactDecoder first;};
 std::vector<Endpoint> endpoints_;std::uint64_t nextScan_{};unsigned selectedProduct_{};
 void scan(std::uint64_t now){
  if(now<nextScan_)return;nextScan_=now+2'000'000'000;
  auto list=SDL_hid_enumerate(0x28de,0);
  for(auto d=list;d&&endpoints_.size()<16;d=d->next){
   if(!steamContactInterface(d->vendor_id,d->product_id,d->usage_page,d->usage,d->interface_number)||!d->path)continue;
   if(selectedProduct_&&steamFirstGeneration(d->product_id)!=steamFirstGeneration(selectedProduct_))continue;
   if(std::any_of(endpoints_.begin(),endpoints_.end(),[&](auto& e){return e.path==d->path;}))continue;
   if(auto h=SDL_hid_open_path(d->path))endpoints_.push_back({h,d->path,0,0,d->product_id,d->bus_type==SDL_HID_API_BUS_BLUETOOTH});
  }
  SDL_hid_free_enumeration(list);
 }
public:
 std::uint32_t available{},buttons{};unsigned detectedProduct{};
 ~SteamContactReader(){reset();}
 void reset(){for(auto& e:endpoints_)SDL_hid_close(e.device);endpoints_.clear();nextScan_=0;selectedProduct_=0;available=buttons=0;detectedProduct=0;}
 void update(std::uint64_t now,unsigned steamControllers,unsigned selectedProduct=0x1302){
  if(selectedProduct_!=selectedProduct){reset();selectedProduct_=selectedProduct;}
  available=buttons=0;detectedProduct=0;
  if(steamControllers!=1){reset();return;}
  scan(now);unsigned active=0;std::uint32_t current=0,currentCapabilities=0;unsigned currentProduct=0;
  for(auto& e:endpoints_){
   std::array<unsigned char,128> data{};
   for(int i=0;i<64;++i){
    int n=SDL_hid_read_timeout(e.device,data.data(),data.size(),0);
    if(n<0){SDL_hid_close(e.device);e.device=nullptr;e.last=0;break;}
    if(!n)break;
    auto report=std::span<const unsigned char>(data.data(),static_cast<size_t>(n));
    if(steamFirstGeneration(e.product)){
     auto decoded=e.first.feed(report,e.bluetooth,now);
     if(e.first.invalidated)e.last=0;
     if(decoded){e.last=now;e.buttons=*decoded;}
    }
    else if(auto decoded=steamContactReport(report)){e.last=now;e.buttons=*decoded;}
    // Explicit wireless disconnect invalidates a previously held contact.
    else if(n>=2&&data[0]==0x79&&data[1]==1)e.last=0;
   }
   if(steamContactFresh(e.last,now)){++active;current=e.buttons;currentProduct=steamFirstGeneration(e.product)?e.product:0x1302;currentCapabilities=steamFirstGeneration(e.product)?(steamTrackpadButtons|buttonMask(20)|buttonMask(21)):(steamContactButtons|rearGyroButtons);}
  }
  std::erase_if(endpoints_,[](auto& e){return !e.device;});
  if(steamContactAssociation(steamControllers,active)){available=currentCapabilities;buttons=current;detectedProduct=currentProduct;}
 }
};
}
