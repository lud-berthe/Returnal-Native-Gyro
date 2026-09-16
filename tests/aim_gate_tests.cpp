#include "rg/motion.hpp"
#include "rg/aim_gate.hpp"
#include <iostream>
#include <stdexcept>
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b,const char* s){check(std::abs(a-b)<0.0001,s);}
int main(){try{
 for(unsigned flags:{0u,2u,4u,8u,12u,14u})check(!rg::aimCommandHeld(flags),"firing and AltFire without FocusAim are not aim input");
 for(unsigned flags:{1u,3u,5u,9u,13u,15u})check(rg::aimCommandHeld(flags),"FocusAim remains held during fire/AltFire");
 for(int mode:{1,2}){
  rg::Settings s;s.ActivationMode=mode;s.GyroSpace=1;s.SensitivityX=s.SensitivityY=1;s.AimMultiplier=2;s.AltFireMultiplier=3;
  rg::MotionProcessor p;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};sample.sensorNs=1'000'000;
  auto run=[&](rg::GameplayState game){rg::CameraDelta d;for(int i=0;i<4;++i){sample.sensorNs+=10'000'000;d=p.process(sample,s,game);}return d.yawDegrees;};
  // Hip firing can report IsAiming=true; it must not count as an aim-button press.
  auto hip=rg::GameplayState{true,true,false,false,false};double d=run(hip);check((d>0)==(mode==2),"hip firing uses the inverse input-based mode gates");
  auto press=rg::GameplayState{true,false,false,false,true};d=run(press);check((d>0)==(mode==1),"press works before aim animation starts");
  auto aim=rg::GameplayState{true,true,false,false,true};d=run(aim);near(d,mode==1?.2:0,"aim multiplier retained");
  auto alt=rg::GameplayState{true,true,true,false,true};d=run(alt);near(d,mode==1?.3:0,"AltFire multiplier retained while aiming");
  // Stop processing immediately on release/press, before the sensor sees the new state.
  check(!rg::gyroAimModeAllows(mode,mode==2),"camera gate rejects queued gyro when mode no longer matches");
  auto released=rg::GameplayState{true,true,true,false,false};d=run(released);check((d>0)==(mode==2),"release uses input even while both animated states persist");
  released.overlay=true;near(run(released),0,"menus still suppress gyro");
  released.overlay=false;released.allowed=false;near(run(released),0,"lost focus still suppresses gyro");
 }
 std::cout<<"Aim command gating: hip fire, entry/release animations, multipliers and camera gate passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
