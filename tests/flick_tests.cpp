#include "rg/flick_stick.hpp"
#include "rg/gyro_menu.hpp"
#include <iostream>
#include <stdexcept>
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void near(double a,double b,const char* why){check(std::abs(a-b)<1e-4,why);}
double run(float x,float y,int ms,int fps){rg::FlickStickProcessor p;double dt=1.0/fps;p.process(0,0,dt,true,ms,2);double sum=0;for(int i=0;i<fps*2;++i)sum+=p.process(i?0:x,i?0:y,dt,true,ms,2);return sum;}
int main(){try{
 near(run(0,rg::flickVerticalFromGameAxis(-1),250,60),0,"Returnal native UP axis faces forward");
 near(run(0,rg::flickVerticalFromGameAxis(1),250,60),180,"Returnal native DOWN axis makes a U-turn");
 near(run(1,rg::flickVerticalFromGameAxis(0),250,60),90,"native RIGHT remains right");
 near(run(-1,rg::flickVerticalFromGameAxis(0),250,60),-90,"native LEFT remains left");
 for(int ms:{0,10,150,250,1000})for(int fps:{30,60,144,360}){
  near(run(1,0,ms,fps),90,"right flick is 90 degrees at every duration/FPS");
  near(run(-1,0,ms,fps),-90,"left flick is -90 degrees");
  near(run(0,-1,ms,fps),180,"back flick is 180 degrees");
  near(run(0,1,ms,fps),0,"forward flick does not turn");
 }
 rg::FlickStickProcessor p;p.process(0,0,.01,true,250,2);double sum=0;
 for(int i=0;i<24;++i)sum+=p.process(1,0,.01,true,250,2);
 check(sum<90,"250 ms animation does not finish early");sum+=p.process(1,0,.01,true,250,2);near(sum,90,"250 ms exact completion");
 near(p.process(1,0,.01,true,250,2),0,"holding direction does not continuously spin");
 p.reset();p.process(0,0,.01,true,0,2);sum=p.process(0,1,.01,true,0,2);
 for(int i=1;i<=360;++i){double a=i*std::numbers::pi/180;sum+=p.process(static_cast<float>(sin(a)),static_cast<float>(cos(a)),.01,true,0,2);}near(sum,360,"one circle turns exactly 360 including wraparound");
 p.reset();p.process(1,0,.01,true,0,2);near(p.process(1,0,.01,true,0,2),0,"enabling while held requires release");p.process(0,0,.01,true,0,2);near(p.process(1,0,.01,true,0,2),90,"release rearms flick");
 p.reset();p.process(0,0,.01,true,250,2);p.process(1,0,.01,true,250,2);near(p.process(1,0,.01,false,250,2),0,"pause cancels animation");near(p.process(1,0,.01,true,250,2),0,"resume cannot replay pending flick");
 p.reset();p.process(0,0,.01,true,0,2);near(p.process(.5f,0,.01,true,0,2),0,"center noise does not flick");near(p.process(1,0,.01,true,0,2),90,"outer threshold flicks");near(p.process(.8f,0,.01,true,0,2),0,"hysteresis avoids repeated flick");near(p.process(1,0,.01,true,0,2),0,"edge return without center does not reflick");

 // The aim-release animation deliberately remains active throughout this fixture.
 {
  rg::FlickStickProcessor transition;rg::GameplayState animated{true,true,true};
  auto held=rg::flickGameplayForTrigger(animated,1|4|8);
  near(transition.processGameplay(1,0,.01,true,held,150,1),0,"input held suppresses flick");
  auto released=rg::flickGameplayForTrigger(animated,0);
  check(!released.aiming&&!released.altFire,"input release overrides both lingering animations");
  double total=transition.processGameplay(1,0,.01,true,released,150,1);
  near(total,0,"held stick establishes baseline without aim-release pivot");
  for(int i=1;i<15;++i)total+=transition.processGameplay(1,0,.01,true,rg::flickGameplayForTrigger(animated,0),150,1);
  near(total,0,"stationary held stick does not rotate during aim-exit animation");
  near(transition.processGameplay(0,-1,.01,true,released,150,1),90,"circular rotation works before aim-exit animation finishes");
  check(!rg::flickEnabled(1,rg::flickGameplayForTrigger({false,true},0)),"raw release does not bypass menus");
  check(rg::flickEnabled(1,rg::flickGameplayForTrigger(animated,16|32)),"shoot-only input does not block hip-fire flick");
 }
 // Aim/Alt-Fire release resumes a held stick as an existing circular gesture.
 for(bool alt:{false,true})for(int ms:{0,150,1000})for(float radius:{.66f,.8f,1.f}){
  rg::FlickStickProcessor transition;rg::GameplayState aiming{true,!alt,alt},hip{true};
  transition.processGameplay(0,0,.01,true,hip,ms,1);
  check(!transition.observeGameplay(1,aiming),"aim axis event passes through to normal stick");
  near(transition.processGameplay(radius,0,.01,true,aiming,ms,1),0,"no flick during aim");
  check(transition.observeGameplay(1,hip),"release axis event restores flick filter");
  near(transition.processGameplay(radius,0,.01,true,hip,ms,1),0,"held aim release creates no pivot");
  for(int i=0;i<110;++i)near(transition.processGameplay(radius,0,.01,true,hip,ms,1),0,"held direction has no delayed spin");
  double circle=0;
  for(int i=1;i<=360;++i){
   double angle=(90+i)*std::numbers::pi/180;
   // Test circular tracking at the outer edge after resuming each deflection.
   circle+=transition.processGameplay(static_cast<float>(sin(angle)),static_cast<float>(cos(angle)),.01,true,hip,ms,1);
  }
  near(circle,360,"release baseline allows a full circle across angle wrap without pivot");
  transition.processGameplay(0,0,.01,true,hip,ms,1);
  double pivot=transition.processGameplay(-1,0,.01,true,hip,ms,1);
  for(int i=0;i<110;++i)pivot+=transition.processGameplay(-1,0,.01,true,hip,ms,1);
  near(pivot,-90,"recentering after aim-release tracking rearms a normal flick");
  // Pause or disconnect clears the remembered aim suspension and circular baseline.
  transition.observeGameplay(1,aiming);transition.observeGameplay(1,{false});
  near(transition.processGameplay(1,0,.01,true,hip,ms,1),0,"menu resume cannot trigger held-stick flick");
  near(transition.processGameplay(0,-1,.01,true,hip,ms,1),0,"menu resume still requires neutral before circular tracking");
  transition.observeGameplay(1,aiming);transition.processGameplay(1,0,.01,false,aiming,ms,1);
  near(transition.processGameplay(1,0,.01,true,hip,ms,1),0,"stale input cannot replay aim-release flick");
  near(transition.processGameplay(0,-1,.01,true,hip,ms,1),0,"stale input clears circular tracking");
  transition.observeGameplay(1,aiming);
  near(transition.processGameplay(0,0,.01,true,hip,ms,1),0,"centered release creates no rotation");
  check(transition.processGameplay(-1,0,.01,true,hip,ms,1)<0,"centered release is immediately armed for next flick");
 }
 for(int mode:{0,1,2}){
  check(rg::flickEnabled(mode,{true})==(mode!=0),"mode selection");
  check(rg::flickEnabled(mode,{true,true})==(mode==2),"hip-only restores normal stick while aiming");
  check(rg::flickEnabled(mode,{true,false,true})==(mode==2),"Alt-Fire counts as aiming");
  check(!rg::flickEnabled(mode,{false})&&!rg::flickEnabled(mode,{true,false,false,true}),"menus and suspension pass through stick");
 }
 auto s=rg::parseConfig("FlickStick=2\nFlickSpinDurationMs=1000\n").settings;check(s.FlickStick==2&&s.FlickSpinDurationMs==1000,"flick settings parse");rg::resetGyroOptions(s);check(s.FlickStick==0&&s.FlickSpinDurationMs==150,"native reset restores flick defaults");
 check(rg::localize("ActivationButton","fr")=="BOUTON DU GYROSCOPE","requested button label");
 check(rg::localize("description.ActivationMode","fr")=="Définit comment le gyroscope s’active.","requested exact mode help");
 std::cout<<"PASS: flick angles, timing, circles, hysteresis, cancellation, aim/Alt-Fire gating and defaults\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
