#include "rg/gyro_menu.hpp"
#include "rg/controller_buttons.hpp"
#include "rg/steam_contacts.hpp"
#include "rg/motion.hpp"
#include <stdexcept>
#include <iostream>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int sum(const rg::Settings& s){return s.GyroButton+s.GyroTouchpad+s.GyroStickSensor+s.GyroGripSensor+s.GyroStick;}
int main(){try{
 using namespace rg;
 check(nativeGyroOptions().size()==19,"five family rows replace the single legacy row");
 for(auto& option:nativeGyroOptions())check(std::string_view(option.key)!="DisableWhileRightStick","obsolete right-stick suspension is absent");
 check(!parseConfig("ConfigVersion=3\nDisableWhileRightStick=1\n").settings.DisableWhileRightStick,"hidden legacy right-stick suspension cannot remain active");
 const std::uint32_t sony=analogStickCapabilities|standardGyroButtons|buttonMask(5),edge=sony|rearGyroButtons;
 const std::uint32_t sc1=buttonMask(29)|(standardGyroButtons&~buttonMask(4))|steamTrackpadButtons|buttonMask(20)|buttonMask(21),sc2=analogStickCapabilities|standardGyroButtons|steamContactButtons|rearGyroButtons;
 check(bindingFamilies(sony)==std::vector<int>({1,2,5}),"DualSense only buttons, one pad, clicks");
 check(bindingChoices(2,sony)==std::vector<int>({5}),"single physical pad has no side/both choices");
 check(bindingChoices(1,sony)==std::vector<int>({1,2,3,4,7,10,11}),"standard DualSense has no extra buttons");
 check(bindingChoices(1,edge)==std::vector<int>({1,2,3,4,7,10,11,20,21,22,23}),"Edge adds paddle and Fn inputs");
 check(bindingChoices(2,sc1)==std::vector<int>({12,13,5,24}),"Steam 1 left right either both pads");
 check(bindingChoices(5,sc1)==std::vector<int>({3}),"Steam 1 has one physical stick");
 check(bindingFamilies(sc2)==std::vector<int>({1,2,3,4,5}),"Steam 2 exposes all families");
 // Exercise the exact list used to create native panel children, not just visibility.
 for(auto caps:{sony,edge,sc1,sc2,0u}){
  auto visible=nativeVisibleOptions(caps);check(!visible.empty(),"disconnected menu retains ordinary settings");
  for(size_t i=0;i<visible.size();++i){check(nativeOptionVisible(*visible[i],caps),"navigation cannot visit an unsupported family");check(nativeFocusIndex(visible[i]->key,caps)==static_cast<int>(i),"focus tracks semantic option after index shifts");}
  for(auto& old:nativeGyroOptions()){int next=nativeFocusIndex(old.key,caps);check(next>=0&&next<static_cast<int>(visible.size()),"removed focus always resolves to an existing child");}
 }
 check(nativeVisibleOptions(sc2).size()==19&&nativeVisibleOptions(sc1).size()==17&&nativeVisibleOptions(0).size()==14,"panel children include no hidden placeholders");
 auto reduced=nativeVisibleOptions(sc1);
 check(std::string_view(reduced[nativeFocusIndex("GyroStickSensor",sc1)]->key)=="GyroStick","losing sensor focus moves forward to the next available family");
 auto disconnected=nativeVisibleOptions(0);
 check(std::string_view(disconnected[nativeFocusIndex("GyroButton",0)]->key)=="InvertX","disconnect skips every absent family");
 for(auto caps:{sony,edge,sc1,sc2,0u})for(auto& option:nativeGyroOptions()){
  Settings s;
  if(optionFamily(option)&&bindingChoices(optionFamily(option),caps).empty()){check(!nativeOptionVisible(option,caps),"unsupported family row hidden");continue;}
  check(nativeOptionVisible(option,caps),"supported row visible");
  for(int i=0;i<nativeOptionCount(s,option,caps);++i){setNativeOptionIndex(s,option,i,caps);check(nativeOptionIndex(s,option,caps)==i,"native index survives filtered list");
   for(auto lang:{"en","fr","de","es","it","pt"}){auto label=nativeOptionValue(s,option,i,lang,ControllerLayout::Sony,caps);check(!label.empty()&&label.find("choice.")==std::string::npos,"all values translated");auto desc=std::string("description.")+option.key;check(localize(desc,lang)!=desc,"all row help translated");}
  }
 }
 for(int id:activationButtonOrder){auto s=parseConfig("ConfigVersion=2\nActivationButton="+std::to_string(id)+"\n").settings;check(sum(s)==id&&s.ActivationButton==0&&s.ConfigVersion==3,"every legacy ID migrates to exactly one family");}
 Settings s;s.GyroButton=20;s.GyroTouchpad=24;s.GyroStickSensor=25;s.GyroGripSensor=26;s.GyroStick=28;
 auto saved=parseConfig(serializeConfig(s)).settings;check(sum(saved)==sum(s)&&saved.GyroButton==20&&saved.GyroStick==28,"independent family values persist together");
 check(!gyroActivationHeld(buttonMask(12)|buttonMask(14)|buttonMask(17)|buttonMask(3),s),"one side of each Both family is insufficient");
 const int pairs[][2]={{12,13},{14,15},{17,18}};
 for(auto& p:pairs)check(gyroActivationHeld(buttonMask(p[0])|buttonMask(p[1]),s),"any completed family independently triggers gyro");
 check(gyroActivationHeld(buttonMask(20),s),"physical button remains usable with sensor families");
 s.GyroTouchpad=5;s.GyroStickSensor=16;s.GyroGripSensor=19;s.GyroStick=27;
 for(int id:{12,13,14,15,17,18})check(gyroActivationHeld(buttonMask(id),s),"Either accepts either side");
 for(int mode:{0,3,5}){MotionProcessor p;Settings cfg;cfg.ActivationMode=mode;cfg.GyroSpace=1;cfg.GyroTouchpad=24;cfg.GyroButton=1;GyroSample sample;sample.degreesPerSecond={0,-10,0};
  auto run=[&](std::uint32_t b){sample.buttons=b;sample.sensorNs+=1'000'000;return p.process(sample,cfg,{true}).yawDegrees;};run(0);run(0);run(0);
  run(buttonMask(12));double one=run(buttonMask(12));check((one>0)==(mode!=3),"one contact does not trigger Both");
  run(buttonMask(12)|buttonMask(13));double both=run(buttonMask(12)|buttonMask(13));check((both>0)==(mode==3),"Both immediately affects activation mode");
  run(buttonMask(1));check(mode!=5||!p.toggleEnabled(),"handoff between active families does not toggle twice");
 }
 std::array<unsigned char,63> report{};report[32]=report[36]=0x80;report[9]=0xf0;
 check(sonyButtons(report,true,false)==0,"base DualSense cannot generate Edge-only buttons");check(sonyButtons(report,true,true)==rearGyroButtons,"Edge paddle/Fn flag mapping");
 std::array<unsigned char,54> steam{};steam[0]=0x42;unsigned raw=0x20000|0x80|0x40000|0x100;for(int b=0;b<4;++b)steam[2+b]=static_cast<unsigned char>(raw>>(8*b));check(steamContactReport(steam)==rearGyroButtons,"Steam 2 rear button mapping");
 check(steamFirstTrackpads(0x18000,true)==(buttonMask(20)|buttonMask(21)),"Steam 1 only two rear buttons");
 Settings analog;analog.GyroStick=28;check(gyroGameButtonSelection(analog,sc2)==0,"analog binding must never suppress stick clicks");
 check(!gyroActivationHeld(buttonMask(3)|buttonMask(4),analog),"stick clicks do not trigger analog Both");
 check(!gyroActivationHeld(0,analog,1,0)&&gyroActivationHeld(0,analog,1,1),"analog Both requires two deflected sticks");
 for(int selected:{3,4,27,28}){
  analog.GyroStick=selected;
  check(!gyroActivationHeld(0,analog,.2f,.2f),"threshold boundary stays inactive");
  check(gyroActivationHeld(0,analog,.21f,0)==(selected==3||selected==27),"left tilt selection");
  check(gyroActivationHeld(0,analog,0,.21f)==(selected==4||selected==27),"right tilt selection");
  check(gyroActivationHeld(0,analog,.21f,.21f),"both tilts satisfy each selector");
 }
 analog.GyroButton=3;check(gyroActivationHeld(buttonMask(3),analog),"L3 in Buttons operates independently");
 check(gyroGameButtonSelection(analog,sc2)==buttonMask(3),"L3 retains short-press filtering in Buttons");
 check(bindingChoices(5,standardGyroButtons).empty(),"click buttons alone do not imply analog axes");
 check(bindingChoices(5,buttonMask(29))==std::vector<int>({3}),"left axis available without a stick click");
 for(int mode:{0,1,2,3,5}){
  MotionProcessor processor;Settings cfg;cfg.ActivationMode=mode;cfg.GyroSpace=1;cfg.GyroStick=4;
  GyroSample sample;sample.degreesPerSecond={0,-10,0};GameplayState game{true,false,false,false,mode==1};
  auto run=[&](float right){sample.rightStickMagnitude=right;sample.sensorNs+=1'000'000;return processor.process(sample,cfg,game).yawDegrees;};
  run(0);run(0);run(0);check((run(0)>0)==(mode!=3),"centered stick leaves automatic gyro active");
  run(1);check((run(1)>0)==(mode==3),"tilt controls each activation mode immediately");
  run(0);check((run(0)>0)==(mode!=3&&mode!=5),"recenter restores automatic mode while Toggle keeps its state");
 }
 for(auto lang:{"en","fr","de","es","it","pt"})for(auto key:{"GyroButton","GyroTouchpad","GyroStickSensor","GyroGripSensor","GyroStick"})check(localize(std::string("description.")+key,lang).find("\n\n")==std::string::npos,"family descriptions omit repeated paragraph");
 Settings unavailable;unavailable.GyroGripSensor=26;for(auto& o:nativeGyroOptions())if(optionFamily(o)==4)check(!nativeOptionVisible(o,sony)&&unavailable.GyroGripSensor==26,"hidden setting retained across device changes");
 s.MixedInput=false;resetGyroOptions(s);check(sum(s)==0&&!s.MixedInput,"reset clears all families but preserves unrelated settings");
 auto invalid=parseConfig("ConfigVersion=3\nGyroButton=24\nGyroTouchpad=17\nGyroStickSensor=3\nGyroGripSensor=14\nGyroStick=12\n").settings;check(sum(invalid)==0,"invalid cross-family IDs rejected");
 std::cout<<"Family controls: capability matrix, migration, independent persistence, Either/Both, motion and rear buttons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
