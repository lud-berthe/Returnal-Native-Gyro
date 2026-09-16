#include "rg/short_press.hpp"
#include "rg/motion.hpp"
#include "rg/controller_buttons.hpp"
#include "rg/gyro_menu.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 using D=rg::KeyDisposition;
 for(auto duration:{1ull,50'000'000ull,199'999'999ull,200'000'000ull,800'000'000ull}){
  rg::ShortPressGate g;check(g.event(0,1,true)==D::Suppress,"game press deferred");
  check(g.event(2,1+duration/2,true)==D::Suppress,"native key repeat cannot escape delay");
  check(g.event(1,1+duration,true)==(duration<200'000'000?D::Tap:D::Suppress),"strict 200ms tap boundary");
  check(g.event(1,2+duration,true)==D::Forward,"no duplicate delayed tap");
 }
 {rg::ShortPressGate g;check(g.event(0,1,false)==D::Forward,"menus preserve immediate input");check(g.event(1,2,false)==D::Forward,"menu releases unchanged");
 g.event(0,3,true);g.cancel();check(g.event(1,4,true)==D::Suppress,"focus/menu/config cancellation cannot fire later");
 g.event(0,5,true);check(g.event(1,6,false)==D::Suppress,"release outside gameplay cancels");
 g.event(0,9,true);check(g.event(1,8,true)==D::Suppress,"reversed clock cannot generate tap");}
 // An available hold interaction bypasses the tap filter for the complete press cycle.
 for(auto duration:{50'000'000ull,200'000'000ull,1'500'000'000ull,3'000'000'000ull}){
  rg::ShortPressGate g;check(g.event(0,1,true,true)==D::Forward,"interaction starts immediately");
  check(!g.pending(),"native interaction is never a deferred tap");
  check(g.event(2,1+duration/2,true,false)==D::Forward,"interaction repeat survives disappearing prompt");
  g.cancel();check(g.event(1,1+duration,false,false)==D::Forward,"release is balanced after interaction/menu/config transition");
  check(g.event(0,2+duration,true,false)==D::Suppress,"next ordinary press restores 200ms gate");
  check(g.event(1,3+duration,true,false)==D::Tap,"ordinary tap after interaction is replayed normally");
 }
 {rg::ShortPressGate g;g.event(0,1,false);check(g.event(0,2,true)==D::Forward,"native press is not captured after gameplay changes");check(g.event(1,3,true)==D::Forward,"native cycle ends without a synthetic tap");}
 {rg::ShortPressGate g;g.event(0,1,true);check(g.event(2,2,true,true)==D::Suppress,"a prompt appearing after a deferred press cannot create an unmatched native repeat");check(g.event(1,1'500'000'001,true,true)==D::Suppress,"old deferred hold cannot turn into a late interaction tap");}
 // Physical gyro input is processed immediately, before the delayed native action.
 for(int mode:{0,3,5}){
  rg::Settings s;s.ActivationMode=mode;s.ActivationButton=10;s.GyroSpace=1;
  rg::MotionProcessor motion;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};sample.sensorNs=1;
  motion.process(sample,s,{true});sample.sensorNs+=1'000'000;motion.process(sample,s,{true});
  rg::ShortPressGate g;check(g.event(0,1,true)==D::Suppress,"square gameplay action deferred");sample.buttons=rg::buttonMask(10);
  sample.sensorNs+=1'000'000;motion.process(sample,s,{true});sample.sensorNs+=1'000'000;
  auto d=motion.process(sample,s,{true});check((d.yawDegrees!=0)==(mode==3),"gyro reacts to square within sensor intervals, not 200ms");
  if(mode==5)check(!motion.toggleEnabled(),"toggle changes on physical press before tap classification");
 }
 {rg::CalibrationCountdown c;c.start(100);check(c.seconds(100)==5,"five second countdown");
 c.start(1100);check(c.seconds(1100)==4,"repeat does not restart countdown");
 check(!c.tick(5099,true)&&c.seconds(5099)==1,"no early calibration");check(c.tick(5100,true),"calibrate exactly after five seconds");check(!c.tick(5200,true),"calibrate once");
 c.start(6000);check(!c.tick(7000,false)&&!c.active(),"leaving page or disconnect cancels countdown");check(!c.tick(12000,true),"cancelled countdown cannot replay");}
 for(auto lang:{"en","fr","de","es","it","pt"})for(auto key:{"action","countdown","collecting","complete","unavailable","steam","steamHelp"}){
  std::string id=std::string("calibration.")+key;check(rg::localize(id,lang)!=id,"calibration translations complete");}
 std::cout<<"PASS: 200ms tap/hold, immediate physical gyro, cancellation and five-second calibration\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
