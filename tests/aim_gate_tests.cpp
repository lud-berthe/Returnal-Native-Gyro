#include "rg/motion.hpp"
#include "rg/aim_gate.hpp"
#include "rg/flick_stick.hpp"
#include <iostream>
#include <stdexcept>
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void near(double a,double b,const char* s){check(std::abs(a-b)<0.0001,s);}
int main(){try{
 for(unsigned flags:{0u,2u,16u,18u})check(!rg::aimCommandHeld(flags),"ordinary fire and unrelated flags are not aim commands");
 for(unsigned flags:{1u,3u,4u,6u,8u,10u,12u,14u,13u,15u})check(rg::aimCommandHeld(flags),"normal aim and independent Alt-Fire preparation/fire enable aim");
 for(unsigned flags=0;flags<32;++flags){
  auto flick=rg::flickGameplayForTrigger({true,true,true},flags);
  check(rg::flickEnabled(1,flick)==rg::gyroAimModeAllows(2,rg::aimCommandHeld(flags)),"hip-only gyro agrees with existing Flick Stick aim/Alt-Fire gating");
 }
 // Separate native button path: its input intent never appears in trigger flags.
 check(!rg::combinedAimCommands({},{}),"no available source is not a fabricated command");
 check(rg::combinedAimCommands({},false)==0u,"known released button works before any trigger callback");
 check(rg::combinedAimCommands({},true)==4u,"independent Alt-Fire works before the first normal aim event");
 for(int mode:{1,2}){
  rg::Settings cfg;cfg.ActivationMode=mode;cfg.GyroSpace=1;cfg.SensitivityX=cfg.SensitivityY=1;
  rg::MotionProcessor processor;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};
  auto frame=[&](std::optional<unsigned> trigger,std::optional<bool> request,bool animatedAlt){
   auto combined=rg::combinedAimCommands(trigger,request);
   rg::GameplayState game{true,true,animatedAlt,false,combined&&rg::aimCommandHeld(*combined)};
   sample.sensorNs+=10'000'000;
   auto delta=processor.process(sample,cfg,game);
   // Same combined command feeds Flick Stick, including before the first trigger event.
   auto flickState=rg::flickGameplayForTrigger(game,combined.value_or(0));
   check(rg::flickEnabled(1,flickState)==!game.aimInputHeld,"gyro and hip-only Flick Stick share separate-button eligibility");
   return delta.yawDegrees;
  };
  for(int i=0;i<4;++i)frame({},false,false);
  near(frame({},true,false),0,"independent button edge still drops its crossing interval");
  near(frame({},true,true),mode==1?.1:0,"L1-style independent intent activates Aim Only with no trigger event");
  near(frame(0u,true,true),mode==1?.1:0,"zero trigger flags cannot override a held independent button");
  near(frame(2u,true,true),mode==1?.1:0,"normal fire does not cancel the held Alt-Fire button");
  near(frame(1u,false,true),mode==1?.1:0,"handoff from Alt-Fire to normal aim stays active");
  near(frame(0u,false,true),0,"release stops immediately even while animated Alt-Fire remains active");
  near(frame(0u,false,true),mode==2?.1:0,"inverse mode resumes while exit animation still plays");
  near(frame(2u,false,true),mode==2?.1:0,"hip fire alone remains excluded after independent release");
  check(!rg::aimCommandHeld(*rg::combinedAimCommands(0u,{})),"missing new-pawn button source cannot reuse prior held state");
 }
 for(int mode:{1,2}){
  rg::Settings s;s.ActivationMode=mode;s.GyroSpace=1;s.SensitivityX=s.SensitivityY=1;s.AimMultiplier=2;s.AltFireMultiplier=3;
  rg::MotionProcessor p;rg::GyroSample sample;sample.degreesPerSecond={0,-10,0};sample.sensorNs=1'000'000;
  auto step=[&](unsigned flags,rg::GameplayState game){game.aimInputHeld=rg::aimCommandHeld(flags);sample.sensorNs+=10'000'000;return p.process(sample,s,game).yawDegrees;};
  auto run=[&](unsigned flags,rg::GameplayState game){double d=0;for(int i=0;i<4;++i)d=step(flags,game);return d;};
  auto hip=rg::GameplayState{true,true,false};
  check((run(2,hip)>0)==(mode==2),"hip fire with animated IsAiming=true does not enable Aim Only");
  near(run(1,{true,false,false}),mode==1?.1:0,"aim command works before entry animation");
  near(run(1,{true,true,false}),mode==1?.2:0,"normal aim multiplier retained");
  near(run(13,{true,true,true}),mode==1?.3:0,"combined aim and Alt-Fire multiplier retained");
  // Separate-button Alt-Fire: release normal aim, then prepare/fire without FocusAim.
  near(run(0,{true,true,true}),mode==2?.3:0,"release overrides lingering animated aim and Alt-Fire");
  near(run(4,{true,false,false}),mode==1?.1:0,"independent Alt-Fire preparation enables gyro before animation");
  near(run(4,{true,true,true}),mode==1?.3:0,"Alt-Fire preparation uses native multiplier state");
  near(step(8,{true,true,true}),mode==1?.3:0,"preparation-to-fire handoff does not interrupt gyro");
  near(step(12,{true,true,true}),mode==1?.3:0,"both Alt-Fire flags remain active without normal aim");
  near(step(13,{true,true,true}),mode==1?.3:0,"adding normal aim does not interrupt gyro");
  near(step(1,{true,true,false}),mode==1?.2:0,"releasing Alt-Fire keeps gyro when normal aim remains held");
  near(run(8,{true,true,true}),mode==1?.3:0,"direct Alt-Fire without preparation is supported");
  // The interval crossing an activation edge is still discarded; both modes stop immediately.
  near(step(2,{true,true,true}),0,"release to ordinary fire stops Aim Only before exit animation completes");
  near(run(2,{true,true,true}),mode==2?.3:0,"ordinary fire cannot prolong the Alt-Fire activation gate");
  near(run(4,{true,true,true,true}),0,"menu blocks separate-button Alt-Fire gyro");
  near(run(8,{false,true,true}),0,"lost gameplay focus blocks separate-button Alt-Fire gyro");
  // The camera gate also rejects motion queued under the previous input state.
  rg::MotionQueue<> queue;queue.push({{1,2},1'000'000,1});
  auto blocked=queue.consume(1'000'000,rg::gyroAimModeAllows(mode,rg::aimCommandHeld(mode==1?2u:4u)),1);
  near(blocked.yawDegrees,0,"camera drops queued horizontal motion when current mode no longer matches");
  near(blocked.pitchDegrees,0,"camera drops queued vertical motion when current mode no longer matches");
 }
 std::cout<<"Aim command gating: standalone Alt-Fire preparation/fire, handoffs, hip fire, release, multipliers, focus and camera queue passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
