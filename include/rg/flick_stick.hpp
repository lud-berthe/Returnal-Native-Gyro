#pragma once
#include "motion.hpp"
#include <algorithm>
#include <numbers>
namespace rg {
// Returnal axis capture: up is negative, down positive. Flick math uses up positive.
inline float flickVerticalFromGameAxis(float value){return -value;}
inline bool flickEnabled(int mode,GameplayState game){return game.allowed&&!game.overlay&&(mode==2||(mode==1&&!game.aiming&&!game.altFire));}
// Validated EInputTriggerStateFlags: FocusAim=1, AltFirePrep=4, AltFire=8.
// Keep gameplay safety gates, but do not wait for the character's exit animation.
inline GameplayState flickGameplayForTrigger(GameplayState game,unsigned flags){game.aiming=(flags&1)!=0;game.altFire=(flags&12)!=0;return game;}
// Pure horizontal angles, independent of gyro sensitivity and game stick acceleration.
class FlickStickProcessor {
 bool initialized_{},armed_{},edge_{},aimPaused_{};
 double previousAngle_{},spin_{},elapsed_{},duration_{};
 int previousMode_{-1},previousDuration_{-1};
 static double ease(double t){t=std::clamp(t,0.0,1.0);return 1-(1-t)*(1-t)*(1-t);}
public:
 void reset(){initialized_=armed_=edge_=aimPaused_=false;spin_=elapsed_=duration_=0;previousMode_=previousDuration_=-1;}
 // Axis capture and camera processing share this gate so aim suspension retains
 // its reason. UI/focus/mode changes still require a neutral stick before flicking.
 bool observeGameplay(int mode,GameplayState game){
  if(mode==1&&game.allowed&&!game.overlay&&(game.aiming||game.altFire)){reset();aimPaused_=true;return false;}
  if(!flickEnabled(mode,game)){reset();return false;}
  return true;
 }
 double processGameplay(float x,float y,double dt,bool fresh,GameplayState game,int durationMs,int mode){
  if(!fresh){reset();return 0;}
  if(!observeGameplay(mode,game))return 0;
  bool resumeHeld=mode==1&&aimPaused_;aimPaused_=false;
  return process(x,y,dt,true,durationMs,mode,resumeHeld);
 }
 double process(float x,float y,double dt,bool enabled,int durationMs,int mode,bool resumeHeld=false){
  if(!enabled||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(dt)||dt<=0||dt>0.25){reset();return 0;}
  if(mode!=previousMode_||durationMs!=previousDuration_){reset();previousMode_=mode;previousDuration_=durationMs;}
  double magnitude=std::hypot(x,y),angle=std::atan2(x,y)*180/std::numbers::pi;
  if(!initialized_){
   initialized_=true;armed_=magnitude<0.65;
   // Leaving aim with an already-deflected stick resumes circular tracking.
   // Its current angle is the baseline, not a new initial pivot.
   if(resumeHeld&&!armed_){edge_=true;previousAngle_=angle;}
   return 0;
  }
  double result=0;
  if(magnitude<0.65){armed_=true;edge_=false;}
  else if(magnitude>=0.9||edge_){
   if(armed_&&!edge_){
    double remaining=duration_>0?spin_*(1-ease(elapsed_/duration_)):0;
    spin_=remaining+angle;elapsed_=0;duration_=std::clamp(durationMs,0,1000)/1000.0;
    previousAngle_=angle;edge_=true;armed_=false;
    if(duration_==0){result+=spin_;spin_=0;}
   }else if(edge_){result+=std::remainder(angle-previousAngle_,360.0);previousAngle_=angle;}
  }
  if(duration_>0&&elapsed_<duration_){double before=ease(elapsed_/duration_);elapsed_=std::min(duration_,elapsed_+dt);result+=spin_*(ease(elapsed_/duration_)-before);}
  return result;
 }
};
}
