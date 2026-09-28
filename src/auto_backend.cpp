#include "rg/backend.hpp"
#include <array>
#include <cmath>
namespace rg {
namespace {
class AutoBackend final:public MotionBackend {
 std::unique_ptr<MotionBackend> active_;
 std::string error_;bool directOnly_{};int mode_{};
 std::uint64_t lastSampleAt_{},lastSensorNs_{},reconnectAt_{};
 std::array<std::uint64_t,4> retryAfter_{};
public:
 explicit AutoBackend(bool directOnly=false):directOnly_(directOnly){}
 bool connect(int index)override{
  // Keep a healthy reader. Rediscover only after loss, never on a polling timer.
  if(connected())return true;
  const auto now=monotonicNs();
  if(active_){active_.reset();reconnectAt_=now+250'000'000;}
  if(now<reconnectAt_){error_="Waiting for controller hotplug to settle";return false;}
  error_.clear();mode_=0;
  // A previous connection does not lock future devices to its calibration owner.
  // Destroy the old reader before opening any replacement; Steam stays preferred.
  for(int mode:{2,3,1}){
   if((directOnly_&&mode==2)||now<retryAfter_[mode])continue;
   auto p=makeMotionBackend(mode);
   if(p->connect(index)){
    mode_=mode;active_=std::move(p);lastSampleAt_=monotonicNs();lastSensorNs_=0;
    error_.clear();return true;
   }
   error_+=(error_.empty()?"":"; ")+p->error();
  }
  if(error_.empty())error_="Waiting to retry motion devices with no sensor data";
  return false;
 }
 std::optional<GyroSample> read()override{
  if(!active_)return {};
  auto sample=active_->read();const auto now=monotonicNs();
  // An open HID/SDL handle is not proof of a working motion stream. Do not keep
  // choosing an accessible but silent device over another usable controller.
  if(!active_->connected()){
   // Some concrete readers close themselves on the same no-data deadline.
   // Apply backoff there too, otherwise a silent Sony handle can win forever.
   if(now-lastSampleAt_>=2'000'000'000)retryAfter_[mode_]=now+3'000'000'000;
   error_=active_->error();active_.reset();reconnectAt_=now+250'000'000;return {};
  }
  auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
  if(sample&&sample->sensorNs>lastSensorNs_&&finite(sample->degreesPerSecond)&&finite(sample->accelG)&&
     (std::abs(sample->accelG.x)+std::abs(sample->accelG.y)+std::abs(sample->accelG.z)>0.0001f)){
   lastSensorNs_=sample->sensorNs;lastSampleAt_=now;return sample;
  }
  if(now-lastSampleAt_>=2'000'000'000){
   error_="Motion stream stopped; rediscovering controller (source="+std::to_string(mode_)+")";
   retryAfter_[mode_]=now+3'000'000'000;active_.reset();reconnectAt_=now+250'000'000;
  }
  return {};
 }
 bool connected()const override{return active_&&active_->connected();}
 DeviceInfo info()const override{return active_?active_->info():DeviceInfo{};}
 std::string error()const override{return active_?active_->error():error_;}
};
}
std::unique_ptr<MotionBackend> makeMotionBackend(int mode){
 switch(mode){case 1:return makeSdlBackend();case 2:return makeSteamBackend();case 3:return makeSonyPassiveBackend();case 4:return std::make_unique<AutoBackend>(true);default:return std::make_unique<AutoBackend>();}
}
}
