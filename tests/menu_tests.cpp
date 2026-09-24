#include "rg/gyro_menu.hpp"
#include "rg/activation_label.hpp"
#include "rg/motion.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 rg::Settings s;check(s.FlickStick==0&&s.FlickSpinDurationMs==150,"flick defaults");
 check(s.GyroEnabled&&s.SensitivityX==2.5f&&s.SensitivityY==2.5f&&s.AimMultiplier==1&&s.AltFireMultiplier==1,"requested numeric defaults");
 check(s.GyroSpace==0&&s.ActivationMode==1&&s.ActivationButton==0&&!s.DisableWhileRightStick&&!s.InvertX&&!s.InvertY&&!s.AutomaticCalibration&&!s.Smoothing&&!s.Acceleration,"requested mode and switch defaults");
 const char* expected[]={"GyroEnabled","SensitivityX","SensitivityY","AimMultiplier","AltFireMultiplier","GyroSpace","ActivationMode","ActivationButton","DisableWhileRightStick","InvertX","InvertY","AutomaticCalibration","Smoothing","Acceleration","FlickStick","FlickSpinDurationMs"};
 check(rg::gyroOptions().size()==16,"exactly sixteen menu options");int row=0;
 for(const auto& option:rg::gyroOptions()){
  check(std::string_view(option.key)==expected[row++],"only requested options in requested order");
  for(int index=0;index<option.count;++index){rg::setOptionIndex(s,option,index);check(rg::optionIndex(s,option)==index,"every native value roundtrips");check(!rg::optionValue(option,index,"en").empty(),"English value available");}
  rg::setOptionIndex(s,option,999);check(rg::optionIndex(s,option)==option.count-1,"upper bound");
  rg::setOptionIndex(s,option,-99);check(rg::optionIndex(s,option)==0,"lower bound");
  for(auto language:{"en","fr","de","es","it","pt","xx"}){check(!rg::localize(option.key,language).empty(),"localized label and English fallback");auto description=std::string("description.")+option.key;check(rg::localize(description,language)!=description,"localized description");}
 }
 const auto smoothing=rg::gyroOptions()[12];check(smoothing.step==5&&smoothing.count==101,"smoothing spans 0..500ms in 5ms steps");
 check(rg::optionValue(smoothing,0,"fr")==rg::localize("off","fr")&&rg::optionValue(smoothing,1,"en")=="5 ms"&&rg::optionValue(smoothing,100,"en")=="500 ms","smoothing display endpoints");
 s.MixedInput=false;s.DeviceIndex=3;rg::resetGyroOptions(s);check(!s.MixedInput&&s.DeviceIndex==3,"gyro reset preserves hidden configuration");
 check(s.SensitivityX==2.5f&&s.ActivationMode==1&&!s.LinkXY&&!s.RatchetButton,"gyro reset restores requested defaults");
 auto legacy=rg::parseConfig("ActivationMode=4\nActivationButton=1\nRatchetButton=5\n").settings;
 check(legacy.ActivationMode==0&&legacy.GyroButton==1&&legacy.RatchetButton==0,"legacy hold-disable keeps its activation button");
 legacy=rg::parseConfig("ActivationMode=1\nActivationButton=1\nRatchetButton=5\n").settings;
 check(legacy.ActivationMode==1&&legacy.GyroTouchpad==5,"legacy automatic mode migrates ratchet to shared button");
 auto modern=rg::parseConfig("ConfigVersion=2\nActivationMode=1\nActivationButton=1\nRatchetButton=5\n").settings;
 check(modern.GyroButton==1&&modern.RatchetButton==0,"modern shared button is authoritative");
 s=rg::Settings{};s.GyroEnabled=true;s.ActivationMode=5;s.ActivationButton=1;s.RatchetButton=0;s.GyroSpace=1;
 rg::MotionProcessor processor;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};
 auto tick=[&](unsigned buttons,rg::GameplayState game){sample.buttons=buttons;sample.sensorNs+=1'000'000;return processor.process(sample,s,game);};
 tick(0,{true});tick(0,{true});check(tick(0,{true}).yawDegrees>0,"toggle starts on");
 tick(1,{true});check(!processor.toggleEnabled(),"L1 switches toggle off");check(tick(1,{true}).yawDegrees==0,"held button does not repeatedly toggle");
 check(rg::activationLabel(s,processor.toggleEnabled())=="Toggle OFF - L1","off reason names activation button");
 tick(0,{true});tick(1,{true});check(processor.toggleEnabled(),"second press resumes");tick(0,{true});
 tick(1,{false,false,false,true});check(processor.toggleEnabled(),"menu press cannot toggle gyro");tick(1,{true});check(processor.toggleEnabled(),"closing menu while held cannot create an edge");tick(0,{true});
 tick(1,{true});check(!processor.toggleEnabled(),"toggle can be disabled again");s.ActivationMode=0;tick(0,{true});check(tick(0,{true}).yawDegrees>0,"Always On recovers from disabled toggle");
 for(int mode:{0,1,2,3}){s=rg::Settings{};s.ActivationMode=mode;s.ActivationButton=1;s.GyroSpace=1;rg::MotionProcessor common;rg::GyroSample input;input.degreesPerSecond={0,-10,0};rg::GameplayState game{true,mode==1,false,false,mode==1};
  auto advance=[&](unsigned held){input.buttons=held;input.sensorNs+=1'000'000;return common.process(input,s,game).yawDegrees;};
  advance(0);advance(0);double free=advance(0);advance(1);double held=advance(1);
  check(mode==3?(free==0&&held>0):(free>0&&held==0),"common button enables in held mode and ratchets in automatic modes");
 }
 std::cout<<"PASS: native menu defaults, sixteen-option contract, presets, localization, migration and toggle safety\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
