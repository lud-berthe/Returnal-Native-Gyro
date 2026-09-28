#include "rg/backend.hpp"
#include <array>
#include <vector>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
std::array<bool,4> available{},silent{},invalid{},selfTimeout{};std::vector<int> attempts;int live{},peak{};
std::uint64_t clockNs=1'000'000'000;int destroyed{};
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
class Fake final:public rg::MotionBackend{
 int type_;bool connected_{};std::uint64_t tick_{},openedAt_{};
public:
 explicit Fake(int t):type_(t){peak=std::max(peak,++live);}
 ~Fake(){--live;++destroyed;}
 bool connect(int)override{attempts.push_back(type_);openedAt_=clockNs;return connected_=available[type_];}
 std::optional<rg::GyroSample> read()override{
  if(selfTimeout[type_]&&clockNs-openedAt_>=2'000'000'000)connected_=false;
  if(!connected()||silent[type_])return {};
  rg::GyroSample s;s.sensorNs=++tick_;s.arrivalNs=clockNs;
  if(invalid[type_])s.degreesPerSecond.x=std::numeric_limits<float>::quiet_NaN();
  return s;
 }
 bool connected()const override{return connected_&&available[type_];}
 rg::DeviceInfo info()const override{rg::DeviceInfo i;i.name=std::to_string(type_);i.path=type_==2?"steam:1":"direct:1";i.externalCalibration=type_==2;return i;}
 std::string error()const override{return "unavailable "+std::to_string(type_);}
};
void setup(bool sdl,bool steam,bool sony){check(live==0,"reader leaked");available={false,sdl,steam,sony};silent={};invalid={};selfTimeout={};attempts.clear();peak=destroyed=0;clockNs=1'000'000'000;}
void advance(std::uint64_t ns=250'000'000){clockNs+=ns;}
}
namespace rg {
std::uint64_t monotonicNs(){return clockNs;}
std::unique_ptr<MotionBackend> makeSdlBackend(){return std::make_unique<Fake>(1);}
std::unique_ptr<MotionBackend> makeSteamBackend(){return std::make_unique<Fake>(2);}
std::unique_ptr<MotionBackend> makeSonyPassiveBackend(){return std::make_unique<Fake>(3);}
}
int main(){try{
 setup(true,true,false);{
  auto b=rg::makeMotionBackend(4);check(b->connect(0),"explicit direct SDL connects");check(attempts==std::vector<int>({3,1}),"explicit direct never probes Steam");check(!b->info().externalCalibration&&b->read().has_value(),"direct stream");check(peak==1,"one reader at a time");
 }
 setup(false,true,false);{
  auto b=rg::makeMotionBackend(4);check(!b->connect(0)&&!b->connected()&&!b->read(),"missing direct sensor remains unavailable");
 }
 // Real ownership changes work in both directions without recreating AutoBackend.
 setup(false,false,true);{
  auto b=rg::makeMotionBackend(0);check(b->connect(0)&&b->read().has_value()&&!b->info().externalCalibration,"Sony startup");
  available={false,false,true,false};attempts.clear();
  check(!b->connected()&&!b->connect(0)&&live==0,"old reader destroyed; hotplug settles");
  advance();check(b->connect(0)&&b->read().has_value()&&b->info().externalCalibration,"native to Steam hotplug");
  check(attempts==std::vector<int>({2}),"Steam preferred on rediscovery");
  available={false,false,false,true};attempts.clear();check(!b->read()&&!b->connected(),"Steam loss detected on read");
  check(!b->connect(0),"brief disconnect debounce");advance();
  check(b->connect(0)&&b->read().has_value()&&!b->info().externalCalibration,"Steam to native hotplug");
  check(attempts==std::vector<int>({2,3})&&peak==1,"no overlapping readers on transition");
 }
 setup(true,true,true);{
  auto b=rg::makeMotionBackend(0);check(b->connect(0)&&b->read().has_value(),"Steam connects");attempts.clear();
  for(int i=0;i<1000;++i){advance(4'000'000);check(b->read().has_value()&&b->connect(0),"healthy stream retained");}
  check(attempts.empty(),"no periodic discovery/reader reset while receiving samples");
  available[2]=false;check(!b->read(),"short Steam disconnect");available[2]=true;
  check(!b->connect(0),"no immediate competing direct reader");advance();
  check(b->connect(0)&&b->info().externalCalibration,"brief Steam reconnect preserves source");
 }
 for(bool malformed:{false,true}){
  setup(true,true,true);{
   silent[2]=!malformed;invalid[2]=malformed;
   auto b=rg::makeMotionBackend(0);check(b->connect(0),"silent handle opens");
   advance(1'999'000'000);check(!b->read()&&b->connected(),"wait for stream startup");
   advance(1'000'000);check(!b->read()&&!b->connected()&&live==0,"silent/invalid stream expires and closes");
   advance();attempts.clear();check(b->connect(0)&&!b->info().externalCalibration,"try usable alternate reader instead of looping on silent Steam");
   check(attempts==std::vector<int>({3})&&b->read().has_value(),"cooldown skips stalled source");
   available[3]=available[1]=false;check(!b->read(),"direct disappears");advance(3'000'000'000);silent[2]=invalid[2]=false;
   check(b->connect(0)&&b->info().externalCalibration&&b->read().has_value(),"Steam eligible again after cooldown");
  }
 }
 setup(true,false,false);{
  auto b=rg::makeMotionBackend(0);check(b->connect(0)&&b->read().has_value(),"SDL stream starts");silent[1]=true;advance(2'000'000'000);
  check(!b->read()&&!b->connected(),"connected SDL handle without sensor reports expires");
  available[2]=true;advance();check(b->connect(0)&&b->info().externalCalibration,"SDL to Steam after stream loss");
 }
 setup(true,false,true);{
  silent[3]=true;selfTimeout[3]=true;
  auto b=rg::makeMotionBackend(0);check(b->connect(0),"Sony opens but never sends extended reports");
  advance(2'000'000'001);check(!b->read()&&!b->connected(),"Sony reader closes itself on its deadline");
  advance();attempts.clear();check(b->connect(0)&&b->info().name=="1"&&b->read().has_value(),"SDL is reachable after silent Sony self-close");
  check(attempts==std::vector<int>({2,1}),"self-closing stalled reader also receives retry backoff");
 }
 setup(false,false,false);{
  auto b=rg::makeMotionBackend(0);check(!b->connect(0),"no controller at launch");available[2]=true;
  check(b->connect(0)&&b->info().externalCalibration,"late controller connection");
 }
 check(live==0,"all reader resources released");std::cout<<"Hotplug both directions, settling, stream watchdog, retry cooldown and healthy-stream stability passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
