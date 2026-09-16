#include "rg/motion.hpp"
#include "rg/steam_motion.hpp"
#include "rg/motion_session.hpp"
#include "rg/controller_buttons.hpp"
#include <limits>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <memory>
using namespace rg;
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
struct Feed {
 double dt;std::uint64_t ns=1'000'000;GamepadMotionHelpers::Vec acceleration{0,1,0};
 GyroSample next(Vec3 rates={}){
  const double len=std::sqrt(rates.x*rates.x+rates.y*rates.y+rates.z*rates.z);
  auto rot=GamepadMotionHelpers::AngleAxis(float(len*dt*3.141592653589793/180),rates.x,rates.y,rates.z);
  acceleration*=rot.Inverse();ns+=static_cast<std::uint64_t>(std::llround(dt*1e9));
  SteamMotionData raw;raw.pitch=rates.x*32768.f/2000;raw.yaw=rates.y*32768.f/2000;raw.roll=rates.z*32768.f/2000;
  raw.accelX=acceleration.x*16384;raw.accelY=-acceleration.z*16384;raw.accelZ=acceleration.y*16384;
  auto s=steamMotionSample(raw,ns);check(s.has_value(),"generated Steam motion must be valid");return *s;
 }
};
Settings settings(int space=0,int mode=0){Settings s;s.GyroSpace=space;s.ActivationMode=mode;s.SensitivityX=s.SensitivityY=1;s.AimMultiplier=s.AltFireMultiplier=1;return s;}
void warm(MotionProcessor& ref,MotionProcessor& subject,Feed& feed,const Settings& rs,const Settings& ss){
 ref.setExternalCalibration(true);subject.setExternalCalibration(true);
 for(int i=0;i<int(8/feed.dt);++i){auto sample=feed.next();ref.process(sample,rs,{true});subject.process(sample,ss,{true});}
}
int main(int argc,char** argv){try{
 std::ofstream csv;if(argc>1)csv.open(argv[1]);csv<<"scenario,hz,space,seconds,yaw_reference,yaw_subject,pitch_reference,pitch_subject,active\n";
 std::cout<<std::fixed<<std::setprecision(6);
 for(int hz:{100,125,250})for(int space:{0,1,3})for(int mode:{1,2}){
  auto ref=std::make_unique<MotionProcessor>(),sub=std::make_unique<MotionProcessor>();Feed f{1./hz};auto rs=settings(space),ss=settings(space,mode);warm(*ref,*sub,f,rs,ss);
  double maxError=0;int active=0;int longestUnexpected=0,unexpected=0;bool wasEligible=false;
  for(int i=0;i<4*hz;++i){bool aim=(i/(hz/2))%2;bool eligible=mode==1?aim:!aim;
   auto sample=f.next({10,-30,5});auto r=ref->process(sample,rs,{true});auto d=sub->process(sample,ss,{true,aim,false,false,aim});
   if(eligible&&wasEligible){maxError=std::max(maxError,std::max(std::abs(d.yawDegrees-r.yawDegrees),std::abs(d.pitchDegrees-r.pitchDegrees)));++active;
    if(!sub->diagnostics().active)++unexpected;else unexpected=0;longestUnexpected=std::max(longestUnexpected,unexpected);
   }else unexpected=0;
   if(!eligible)check(d.yawDegrees==0&&d.pitchDegrees==0,"disabled aim side must suppress both axes");
   wasEligible=eligible;
  }
  check(active>0&&maxError<1e-8&&longestUnexpected==0,"aim transitions must preserve orientation after the single activation-edge interval");
  std::cout<<"aim mode="<<mode<<" hz="<<hz<<" space="<<space<<" max_difference_deg="<<maxError<<" unexpected_inactive_samples="<<longestUnexpected<<'\n';
 }
 for(int hz:{100,125,250}){
  std::array<std::unique_ptr<MotionProcessor>,4> refs;for(auto& r:refs){r=std::make_unique<MotionProcessor>();r->setExternalCalibration(true);}
  auto sub=std::make_unique<MotionProcessor>();sub->setExternalCalibration(true);Feed f{1./hz};auto cfg=settings();
  for(int i=0;i<8*hz;++i){auto sample=f.next();for(int space=0;space<4;++space)refs[space]->process(sample,settings(space),{true});sub->process(sample,cfg,{true});}
  const int spaces[]={0,1,0,3,0,2,0};double maxError=0;
  for(int block=0;block<7;++block)for(int i=0;i<hz;++i){auto sample=f.next({10,-30,5});CameraDelta expected;
   for(int space=0;space<4;++space){auto d=refs[space]->process(sample,settings(space),{true});if(space==spaces[block])expected=d;}
   cfg.GyroSpace=spaces[block];auto actual=sub->process(sample,cfg,{true});maxError=std::max(maxError,std::max(std::abs(actual.yawDegrees-expected.yawDegrees),std::abs(actual.pitchDegrees-expected.pitchDegrees)));
  }
  check(maxError<1e-8,"switching gyro spaces should preserve the shared orientation state");
  std::cout<<"space_switch hz="<<hz<<" max_difference_deg="<<maxError<<'\n';
 }
 for(int hz:{100,125,250})for(int space:{0,1,3})for(bool reset:{false,true}){
  auto ref=std::make_unique<MotionProcessor>(),sub=std::make_unique<MotionProcessor>();Feed f{1./hz};auto cfg=settings(space);warm(*ref,*sub,f,cfg,cfg);
  if(reset)sub->resetDevice();else f.ns+=150'000'000; // Same-stream gap, no reconnect.
  double recovery=-1,maxPitchError=0;int consecutive=0;double ratio100=0,ratio500=0;bool ratio100Set=false,ratio500Set=false;
  for(int i=0;i<3*hz;++i){auto sample=f.next({10,-30,0});auto r=ref->process(sample,cfg,{true});auto d=sub->process(sample,cfg,{true});double t=(i+1)*f.dt;
   if(i>=3){if(space!=3)maxPitchError=std::max(maxPitchError,std::abs(d.pitchDegrees-r.pitchDegrees));
    double ratio=std::abs(r.yawDegrees)>1e-8?d.yawDegrees/r.yawDegrees:1;
    if(!ratio100Set&&t>=.1){ratio100=ratio;ratio100Set=true;}if(!ratio500Set&&t>=.5){ratio500=ratio;ratio500Set=true;}
    if(ratio>=.95&&ratio<=1.05)++consecutive;else consecutive=0;if(consecutive==5&&recovery<0)recovery=t-4*f.dt;
   }
   csv<<(reset?"reconnect_reset":"sample_gap")<<','<<hz<<','<<space<<','<<t<<','<<r.yawDegrees<<','<<d.yawDegrees<<','<<r.pitchDegrees<<','<<d.pitchDegrees<<','<<sub->diagnostics().active<<'\n';
  }
  std::cout<<(reset?"reset":"gap")<<" hz="<<hz<<" space="<<space<<" yaw_ratio_100ms="<<ratio100<<" yaw_ratio_500ms="<<ratio500<<" recovery_95pct_seconds="<<recovery<<" pitch_max_error_deg="<<maxPitchError<<'\n';
  if(space==0&&reset){check(ratio100>.99&&ratio500>.99&&recovery<.05,"reset initializes horizontal response without gravity ramp");check(maxPitchError<1e-8,"reset leaves active vertical response unchanged");}
  if(!reset||space==1)check(std::abs(ratio100-1)<1e-5,"sample gap/local yaw must not cause horizontal-only recovery");
 }

 // New-device initialization is based on current acceleration, even when tilted.
 for(auto accel:std::array<Vec3,4>{{{0,1,0},{0,0,1},{.6f,.8f,0},{0,-1,0}}}){
  GamepadMotion g;g.PauseContinuousCalibration();
  check(g.InitializeGravityFromAcceleration(accel.x,accel.y,accel.z),"valid gravity seed accepted");float x,y,z;g.GetGravity(x,y,z);
  check(std::abs(x+accel.x)<1e-6&&std::abs(y+accel.y)<1e-6&&std::abs(z+accel.z)<1e-6,"fresh gravity follows actual controller pose");
  check(!g.InitializeGravityFromAcceleration(0,0,0)&&!g.InitializeGravityFromAcceleration(4,0,0)&&!g.InitializeGravityFromAcceleration(std::numeric_limits<float>::quiet_NaN(),0,0),"zero outlier and invalid acceleration rejected");
  float xx,yy,zz;g.GetGravity(xx,yy,zz);check(x==xx&&y==yy&&z==zz,"rejected seed does not damage prior state");
 }
 MotionSession session;
 check(!session.reconnect("steam:1",true,1'000'000'000),"first device resets");session.sample(1'000'000'000);
 check(session.reconnect("steam:1",true,1'100'000'000),"brief same-handle interruption preserves orientation");session.sample(1'100'000'000);
 check(!session.reconnect("steam:2",true,1'200'000'000),"another controller at the same slot must reset");session.sample(1'200'000'000);
 check(!session.reconnect("steam:2",true,3'200'000'001),"expired orientation is not reused");session.sample(4'000'000'000);
 check(!session.reconnect("steam:2",false,4'100'000'000),"calibration owner switch resets");session.sample(5'000'000'000);
 check(!session.reconnect("sony-device",false,5'100'000'000),"direct device follows its existing reset policy");
 // Preserve a user-disabled Toggle state and swallow held contacts at reconnection.
 auto p=std::make_unique<MotionProcessor>();p->setExternalCalibration(true);Settings cfg=settings();cfg.ActivationMode=5;cfg.GyroButton=1;Feed f{.004};
 for(int i=0;i<100;++i)p->process(f.next(),cfg,{true});
 auto sample=f.next();sample.buttons=buttonMask(1);p->process(sample,cfg,{true});check(!p->toggleEnabled(),"toggle disabled before interruption");
 p->resumeDevice();f.ns+=1'000'000'000;
 for(int i=0;i<20;++i){sample=f.next({10,-30,0});sample.buttons=buttonMask(1);auto d=p->process(sample,cfg,{true});check(!p->toggleEnabled()&&d.yawDegrees==0&&d.pitchDegrees==0,"resume preserves toggle and cannot synthesize an edge");}
 p->process(f.next(),cfg,{true});sample=f.next();sample.buttons=buttonMask(1);p->process(sample,cfg,{true});check(p->toggleEnabled(),"new real press still toggles");
 // A resume never integrates the missing interval, and retains the gravity vector.
 cfg=settings();p->process(f.next(),cfg,{true});auto before=p->diagnostics().gravity;
 p->resumeDevice();f.ns+=500'000'000;auto first=p->process(f.next(),cfg,{true});auto second=p->process(f.next(),cfg,{true});auto third=p->process(f.next(),cfg,{true});
 check(first.yawDegrees==0&&first.pitchDegrees==0&&second.yawDegrees==0&&second.pitchDegrees==0,"resume primes timestamps without replaying motion");
 check(std::hypot(third.yawDegrees,third.pitchDegrees)<1e-8,"stationary resume has no artificial camera delta");
 auto after=p->diagnostics().gravity;check(std::sqrt(after.x*after.x+after.y*after.y+after.z*after.z)>.99,"resume gravity never goes back to zero");
 std::cout<<"OFFLINE CHECKS PASSED; no game process, Steam API or device access used.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
