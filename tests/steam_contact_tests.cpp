#include "rg/steam_contacts.hpp"
#include "rg/gyro_menu.hpp"
#include "rg/motion.hpp"
#include <array>
#include <stdexcept>
#include <iostream>
void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
int main(){try{
 const std::uint32_t flags[]={0x02000000,0x00200000,0x01000000,0x00100000,0x20000000,0x10000000};
 const int ids[]={12,13,14,15,17,18},either[]={5,5,16,16,19,19};
 for(unsigned char reportId:{0x42,0x45,0x47}){
  std::array<unsigned char,54> report{};report[0]=reportId;
  auto decode=[&](std::uint32_t f){for(int b=0;b<4;++b)report[2+b]=static_cast<unsigned char>(f>>(8*b));return rg::steamContactReport(report);};
  check(decode(0)==0,"released report is valid without contacts");
  check((*decode(0x0ccfffff)&rg::steamContactButtons)==0,"clicks, rear buttons and trigger clicks are not contacts");
  for(int i=0;i<6;++i){
   auto buttons=decode(flags[i]);check(buttons== (rg::buttonMask(ids[i])|rg::buttonMask(either[i])),"contact activates only its side and either binding");
   for(int id:{ids[i],either[i]})for(int mode:{0,3,5}){
    rg::Settings s;s.ActivationMode=mode;s.ActivationButton=id;s.GyroSpace=1;
    rg::MotionProcessor p;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};
    auto tick=[&](std::uint32_t mask){sample.buttons=mask;sample.sensorNs+=1'000'000;return p.process(sample,s,{true}).yawDegrees;};
    tick(0);tick(0);tick(0);tick(*buttons);double held=tick(*buttons);
    check((held>0)==(mode==3),"contact immediately enables or suspends without a click");
    check(mode!=5||!p.toggleEnabled(),"held touch toggles only once");
    tick(0);double released=tick(0);check((released>0)==(mode==0),"lifting contact restores the correct mode");
   }
  }
  check(decode(0x33300000)==rg::steamContactButtons,"all six contacts have all nine selectors");
  for(size_t n=0;n<46;++n)check(!rg::steamContactReport({report.data(),n}),"truncated reports rejected");
  report[0]=0x43;check(!rg::steamContactReport(report),"battery data cannot activate a contact");
 }
 // First generation: captured USB framing, independent pads and stick/pad interleaving.
 std::array<unsigned char,64> old{};old[0]=1;old[2]=1;old[3]=60;
 auto oldFlags=[&](std::uint32_t f){for(int b=0;b<3;++b)old[8+b]=static_cast<unsigned char>(f>>(8*b));return rg::steamFirstUsbContactReport(old);};
 check(oldFlags(0)==0,"SC1 released USB report");
 check(oldFlags(0x80000)==(rg::buttonMask(5)|rg::buttonMask(12)),"SC1 left touch");
 check(oldFlags(0x100000)==(rg::buttonMask(5)|rg::buttonMask(13)),"SC1 right touch");
 check(oldFlags(0x180000)==rg::steamTrackpadButtons,"SC1 both touches");
 check(oldFlags(0x800000)==(rg::buttonMask(5)|rg::buttonMask(12)),"interleaved left stick does not release left trackpad");
 check((*oldFlags(0x470000)&rg::steamTrackpadButtons)==0,"SC1 clicks and grip buttons do not act as contacts");
 old[2]=7;check(oldFlags(0x180000)==rg::steamTrackpadButtons,"SC1 alternate versioned packet");
 old[2]=4;check(!oldFlags(0x180000),"SC1 status is not input");old[2]=1;
 for(size_t n=0;n<64;++n)check(!rg::steamFirstUsbContactReport({old.data(),n}),"truncated declared USB payload rejected");
 old[3]=65;check(!rg::steamFirstUsbContactReport(old),"SC1 invalid payload length rejected");old[3]=60;
 rg::SteamFirstContactDecoder decoder;
 std::array<unsigned char,20> ble{};ble[0]=3;ble[1]=0xc0;ble[2]=0x14;ble[6]=0x18;
 check(decoder.feed(ble,true,1'000'000)==rg::steamTrackpadButtons,"SC1 BLE button chunk");
 ble[2]=0x04;ble[3]=0x08; // gyro-only delta packet must retain previous contact state
 check(decoder.feed(ble,true,2'000'000)==rg::steamTrackpadButtons,"BLE unchanged contacts survive omitted button chunk");
 check(!decoder.feed(ble,true,300'000'000),"BLE timeout requires fresh button state");
 ble[2]=0x14;ble[3]=0;ble[6]=0;
 check(decoder.feed(ble,true,301'000'000)==0,"BLE last finger release");
 // Full payload: all fields total 42 bytes and require three BLE segments.
 std::array<unsigned char,54> full{};full[0]=0xf4;full[1]=0x1f;full[4]=0x08;
 for(int i=0;i<3;++i){ble[1]=static_cast<unsigned char>(0x80|i|(i==2?0x40:0));std::copy_n(full.begin()+i*18,18,ble.begin()+2);auto result=decoder.feed(ble,true,302'000'000+i);check(i==2?result==(rg::buttonMask(5)|rg::buttonMask(12)):!result,"BLE reports assembled before decoding");}
 ble[1]=0xc2;check(!decoder.feed(ble,true,303'000'000)&&decoder.invalidated,"out-of-order BLE segments invalidate contacts");
 ble[1]=0xc0;ble[2]=0x04;ble[3]=0x08;check(!decoder.feed(ble,true,304'000'000),"lost BLE sequence cannot reuse old contacts");
 std::array<unsigned char,5> disconnected{1,0,3,1,1};check(!decoder.feed(disconnected,false,305'000'000)&&decoder.invalidated,"dongle disconnect clears contacts");
 check(rg::steamContactDevice(0x28de,0x1102)&&rg::steamContactDevice(0x28de,0x1106)&&rg::steamContactDevice(0x28de,0x1142),"Steam backend admits each SC1 product");
 check(rg::steamContactInterface(0x28de,0x1102,0xff00,1,2)&&!rg::steamContactInterface(0x28de,0x1102,0xff00,1,0),"SC1 USB accepts controller interface only");
 check(rg::steamContactInterface(0x28de,0x1142,0xff00,1,4)&&!rg::steamContactInterface(0x28de,0x1142,0xff00,1,5),"SC1 dongle slots bounded");
 const auto& sc1Option=rg::gyroOptions()[7];auto sc1Caps=rg::standardGyroButtons|rg::steamTrackpadButtons;
 check(rg::optionCount(sc1Option,sc1Caps)==11,"SC1 exposes three pad choices plus standard buttons");
 for(int i=0;i<rg::optionCount(sc1Option,sc1Caps);++i){rg::Settings value;rg::setOptionIndex(value,sc1Option,i,sc1Caps);check(value.ActivationButton<14,"SC1 hides stick and grip sensors");}
 check(rg::steamContactInterface(0x28de,0x1304,0xff00,1,2),"Puck controller interface accepted");
 check(!rg::steamContactInterface(0x28de,0x1304,0xff00,2,6),"Puck management interface excluded");
 check(!rg::steamContactInterface(0x054c,0x1302,0xff00,1,2),"unrelated vendors excluded");
 check(rg::steamContactAssociation(1,1)&&!rg::steamContactAssociation(2,1)&&!rg::steamContactAssociation(1,2)&&!rg::steamContactAssociation(1,0),"ambiguous or missing physical associations never exposed");
 check(rg::steamContactFresh(100,200)&&!rg::steamContactFresh(0,200)&&!rg::steamContactFresh(200,100)&&!rg::steamContactFresh(100,200'000'100),"stale contacts clear after 200 ms");
 const auto& option=rg::gyroOptions()[7];
 for(std::uint32_t caps:{0u,rg::standardGyroButtons,rg::standardGyroButtons|rg::buttonMask(5),rg::standardGyroButtons|rg::steamContactButtons,rg::buttonMask(17)|rg::buttonMask(19)}){
  for(int i=0;i<rg::optionCount(option,caps);++i){
   rg::Settings s;rg::setOptionIndex(s,option,i,caps);check(s.ActivationButton==0||(caps&rg::buttonMask(s.ActivationButton)),"unavailable choices are absent");check(rg::optionIndex(s,option,caps)==i,"filtered display indices retain persistent IDs");
   for(auto lang:{"en","fr","de","es","it","pt"}){auto label=rg::optionValue(option,i,lang,rg::ControllerLayout::Steam,true,caps);check(!label.empty()&&label.find("contact.")==std::string::npos&&label.find("UNAVAILABLE")==std::string::npos,"filtered choices translated");}
  }
 }
 rg::Settings s;s.ActivationButton=18;check(rg::optionIndex(s,option,rg::standardGyroButtons)==0&&s.ActivationButton==18,"hidden stored selection preserved without rewriting");
 check(rg::optionIndex(s,option,rg::standardGyroButtons|rg::steamContactButtons)>0,"selection restored when sensor reconnects");
 for(int id=12;id<=19;++id)check(rg::parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(id)+"\n").settings.GyroTouchpad+rg::parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(id)+"\n").settings.GyroStickSensor+rg::parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(id)+"\n").settings.GyroGripSensor==id,"new IDs persist in INI");
 std::cout<<"Steam contacts: decoding, contact gating, freshness, association and filtered menu passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
