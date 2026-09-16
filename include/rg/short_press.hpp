#pragma once
#include <cstdint>
namespace rg {
enum class KeyDisposition { Forward, Suppress, Tap };
// Native actions wait for release; the gyro continues to read physical HID input.
class ShortPressGate {
 bool held_{},cancelled_{},forwarded_{};std::uint64_t started_{};
public:
 static constexpr std::uint64_t thresholdNs=200'000'000;
 void cancel(){if(held_)cancelled_=true;}
 bool pending()const{return held_;}
 KeyDisposition event(int event,std::uint64_t now,bool eligible,bool nativeHold=false){
  // A native press owns the entire cycle, even if focus, binding or eligibility changes.
  if(forwarded_){if(event==1)forwarded_=false;return KeyDisposition::Forward;}
  if(held_&&!eligible)cancelled_=true;
  if(event==0){
   if(held_)return KeyDisposition::Suppress;
   if(!eligible||nativeHold){forwarded_=true;return KeyDisposition::Forward;}
   held_=true;cancelled_=false;started_=now;return KeyDisposition::Suppress;
  }
  if(!held_)return KeyDisposition::Forward;
  if(event==1){bool tap=!cancelled_&&eligible&&now>=started_&&now-started_<thresholdNs;held_=cancelled_=false;return tap?KeyDisposition::Tap:KeyDisposition::Suppress;}
  return KeyDisposition::Suppress;
 }
};
class CalibrationCountdown {
 std::uint64_t deadline_{};
public:
 bool active()const{return deadline_!=0;}
 void start(std::uint64_t now){if(!active())deadline_=now+5000;}
 void cancel(){deadline_=0;}
 unsigned seconds(std::uint64_t now)const{return deadline_>now?static_cast<unsigned>((deadline_-now+999)/1000):0;}
 bool tick(std::uint64_t now,bool eligible){if(!eligible){cancel();return false;}if(active()&&now>=deadline_){cancel();return true;}return false;}
};
}
