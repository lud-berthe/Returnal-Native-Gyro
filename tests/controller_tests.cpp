#include "rg/controller_buttons.hpp"
#include "rg/gyro_menu.hpp"
#include "rg/motion.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
int main(){try{
 // Synthetic payload fixtures follow the documented SDL PS4/PS5 HID layouts.
 for(bool ps5:{false,true}){
  std::array<unsigned char,63> bytes{};const int c=ps5?32:34,b=ps5?7:4;
  bytes[c]=bytes[c+4]=0x80;
  auto decode=[&]{return rg::sonyButtons(bytes,ps5);};
  check(decode()==0,"released report must have no active bindings");
  bytes[b+2]=2;check(decode()==0,"mechanical touchpad click alone must not activate contact binding");bytes[b+2]=0;
  bytes[c]=7;check(decode()==16,"first finger touches without mechanical click");
  bytes[c+4]=9;check(decode()==16,"two contacts remain one activation bit");
  bytes[c]=0x87;check(decode()==16,"second finger alone maintains contact");
  bytes[c+4]=0x89;check(decode()==0,"last finger release clears contact");
  const int faceBits[]={0x20,0x40,0x10,0x80},faceIds[]={6,7,10,11};
  for(int i=0;i<4;++i){bytes[b]=static_cast<unsigned char>(faceBits[i]);check(decode()==rg::buttonMask(faceIds[i]),"each face button has its persistent ID");}bytes[b]=0;
  const int shoulderBits[]={1,2,0x40,0x80};
  for(int i=0;i<4;++i){bytes[b+1]=static_cast<unsigned char>(shoulderBits[i]);check(decode()==rg::buttonMask(i+1),"shoulder and stick click IDs preserved");}bytes[b+1]=0;
  for(int trigger=0;trigger<2;++trigger){int at=(ps5?4:7)+trigger;bytes[at]=127;check(decode()==0,"half trigger is below activation threshold");bytes[at]=128;check(decode()==rg::buttonMask(8+trigger),"L2/R2 activate above half travel");bytes[at]=0;}
  for(size_t n=0;n<=static_cast<size_t>(c+4);++n)check(rg::sonyButtons({bytes.data(),n},ps5)==0,"short report rejected without reading out of bounds");
  // Exercise decoded contact through motion gating, not only a bit-mask comparison.
  rg::Settings s;s.ActivationMode=0;s.ActivationButton=5;s.GyroSpace=1;
  rg::MotionProcessor processor;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};
  auto tick=[&]{sample.buttons=decode();sample.sensorNs+=1'000'000;return processor.process(sample,s,{true}).yawDegrees;};
  tick();tick();check(tick()>0,"gyro responds while touchpad released");
  bytes[b+2]=2;check(tick()>0,"click bit without contact does not suspend gyro");bytes[b+2]=0;
  bytes[c]=0;check(tick()==0,"touch immediately suspends gyro");
  bytes[c]=0x80;tick();check(tick()>0,"gyro resumes after lifting finger");
 }
 const auto& option=rg::gyroOptions()[7];
 const int ids[]={0,1,2,3,4,10,11,7,5};
 const char* french[]={"Aucun","L1","R1","L3","R3","CARRÉ","TRIANGLE","ROND","PAVÉ TACTILE"};
 check(rg::optionCount(option,rg::standardGyroButtons|rg::buttonMask(5))==9,"Sony exposes the existing eight buttons plus None");
 for(int removed:{6,8,9})check(rg::parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(removed)+"\n").settings.ActivationButton==0,"removed bindings migrate to None");
 for(int i=0;i<9;++i){
  auto s=rg::parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(ids[i])+"\n").settings;
  check(s.GyroButton+s.GyroTouchpad+s.GyroStickSensor+s.GyroGripSensor+s.GyroStick==ids[i],"all old/new binding IDs load unchanged");
  s.ActivationButton=ids[i];check(rg::optionIndex(s,option)==i,"legacy flat display keeps persistent ID order");
  check(rg::optionValue(option,i,"fr")==french[i],"French physical button names and order");
 }
 using L=rg::ControllerLayout;
 check(rg::buttonLabel(6,"en",L::Xbox)=="A"&&rg::buttonLabel(6,"en",L::Nintendo)=="B","south button respects Xbox/Nintendo labels");
 check(rg::buttonLabel(10,"en",L::Nintendo)=="Y"&&rg::buttonLabel(11,"en",L::Nintendo)=="X","Nintendo face labels are positional");
 check(rg::buttonLabel(8,"en",L::Xbox)=="LT"&&rg::buttonLabel(9,"en",L::Nintendo)=="ZR","trigger labels match controller");
 check(rg::buttonLabel(5,"fr",L::Nintendo,false)=="PAVÉ TACTILE (INDISPONIBLE)","missing touchpad is not mapped to unrelated hardware");
 for(auto language:{"en","fr","de","es","it","pt"}){
  auto help=rg::localize("description.GyroSpace",language);size_t count=0,at=0;
  while((at=help.find("\n\n",at))!=std::string::npos){++count;at+=2;}check(count==3,"all four gyro spaces explained in each language");
  for(auto layout:{L::Sony,L::Xbox,L::Nintendo,L::Generic})for(int id=1;id<=11;++id){auto label=rg::buttonLabel(id,language,layout);check(!label.empty()&&label.find("button.")==std::string::npos,"button descriptions translated for every layout");}
 }
 std::cout<<"PASS: PS4/PS5 touch versus click, two fingers, physical buttons, trigger boundaries, contact ratcheting, saved IDs and controller labels\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
