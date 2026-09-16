#include "rg/steam_motion.hpp"
#include "rg/settings.hpp"
#include "rg/controller_buttons.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
void check(bool v,const char* text){if(!v)throw std::runtime_error(text);}
void near(double a,double b,const char* text){check(std::abs(a-b)<0.001,text);}
int main(){try{
 rg::SteamMotionData m;m.accelZ=16384;
 auto s=rg::steamMotionSample(m,100);check(s.has_value(),"stationary Steam controller is valid");near(s->accelG.y,1,"flat controller gravity is up");check(s->sensorNs==100&&s->arrivalNs==100,"host polling time retained");
 m.pitch=16384;m.yaw=-16384;m.roll=8192;m.accelX=8192;m.accelY=4096;
 s=rg::steamMotionSample(m,200);near(s->degreesPerSecond.x,1000,"pitch degrees/s");near(s->degreesPerSecond.y,-1000,"yaw degrees/s");near(s->degreesPerSecond.z,500,"roll degrees/s");near(s->accelG.x,.5,"right gravity axis");near(s->accelG.z,-.25,"forward gravity sign");
 check(!rg::steamMotionSample({},100),"zero warming/no-sensor packet rejected");
 m.pitch=std::numeric_limits<float>::quiet_NaN();check(!rg::steamMotionSample(m,100),"NaN rejected");m.pitch=std::numeric_limits<float>::infinity();check(!rg::steamMotionSample(m,100),"infinite packet rejected");m.pitch=32769;check(!rg::steamMotionSample(m,100),"invalid range rejected");
 m={};m.accelZ=16384;m.yaw=-90.0f*32768.0f/2000.0f;
 for(auto dt:{4'000'000ull,10'000'000ull}){
  rg::MotionProcessor p;rg::Settings settings;settings.GyroSpace=1;settings.ActivationMode=0;settings.SensitivityX=settings.SensitivityY=1;
  std::uint64_t time=1'000'000;double yaw=0;
  for(int i=0;i<3;++i){p.process(*rg::steamMotionSample(m,time),settings,{true});time+=dt;}
  for(std::uint64_t elapsed=0;elapsed<1'000'000'000;elapsed+=dt){auto d=p.process(*rg::steamMotionSample(m,time),settings,{true});yaw+=d.yawDegrees;time+=dt;}
  near(yaw,90,"one second Steam turn independent of polling interval");
  auto sample=*rg::steamMotionSample(m,time);sample.buttons=rg::buttonMask(10);settings.ActivationButton=10;near(p.process(sample,settings,{true}).yawDegrees,0,"Steam button suspends immediately");
  sample.sensorNs+=1'000'000'000;near(p.process(sample,settings,{true}).yawDegrees,0,"gap never integrated");
 }

 // Steam's calibrated rates must survive every local calibration request unchanged.
 {
  rg::MotionProcessor p;p.setBias({0,-1,0});p.beginCalibration();p.setExternalCalibration(true);
  rg::Settings settings;settings.AutomaticCalibration=true;settings.ActivationMode=0;settings.GyroSpace=1;settings.SensitivityX=settings.SensitivityY=1;
  m={};m.accelZ=16384;m.yaw=-1.0f*32768.0f/2000.0f;
  p.beginCalibration();p.setBias({0,-1,0});p.resetCalibration();
  for(int i=0;i<2500;++i){p.process(*rg::steamMotionSample(m,1'000'000ull+i*4'000'000ull),settings,{true});}
  near(p.diagnostics().calibrated.y,-1,"slow Steam motion is not calibrated away");near(p.diagnostics().bias.y,0,"no residual or new Steam bias");
  check(p.diagnostics().calibration==rg::CalibrationState::Idle,"manual Steam calibration never starts");
  check(p.diagnostics().active,"Steam gyro is not blocked by calibration");
  p.resetDevice();p.beginCalibration();check(p.diagnostics().calibration==rg::CalibrationState::Idle,"device reset retains Steam ownership");
  p.setExternalCalibration(false);p.beginCalibration();check(p.diagnostics().calibration==rg::CalibrationState::Collecting,"direct sensor manual calibration restored");
  settings.AutomaticCalibration=false;settings.CalibrationSeconds=1;
  for(int i=0;i<600;++i)p.process(*rg::steamMotionSample(m,20'000'000'000ull+i*4'000'000ull),settings,{true});
  check(p.diagnostics().calibration==rg::CalibrationState::Complete,"direct sensor calibration still completes");near(p.diagnostics().bias.y,-1,"direct sensor learns drift");
  p.setExternalCalibration(true);near(p.diagnostics().bias.y,0,"direct bias removed on switch back to Steam");
 }
 check(rg::parseConfig("SensorBackend=0").settings.SensorBackend==0,"legacy default becomes auto");
 check(rg::parseConfig("SensorBackend=1").settings.SensorBackend==1,"explicit SDL preserved");
 check(rg::parseConfig("SensorBackend=2").settings.SensorBackend==2,"explicit Steam accepted");
 check(rg::parseConfig("SensorBackend=3").settings.SensorBackend==3,"direct Sony available");
 std::cout<<"Steam motion conversion, timing, button and configuration checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
