#include "rg/backend.hpp"
#include <array>
#include <vector>
#include <iostream>
#include <stdexcept>
namespace {
std::array<bool,4> available{};std::vector<int> attempts;int live{},peak{};
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
class Fake final:public rg::MotionBackend{
 int type_;bool connected_{};
public:
 explicit Fake(int t):type_(t){peak=std::max(peak,++live);}
 ~Fake(){--live;}
 bool connect(int)override{attempts.push_back(type_);return connected_=available[type_];}
 std::optional<rg::GyroSample> read()override{if(!connected_)return {};rg::GyroSample s;s.sensorNs=type_;return s;}
 bool connected()const override{return connected_;}
 rg::DeviceInfo info()const override{rg::DeviceInfo i;i.name=std::to_string(type_);i.externalCalibration=type_==2;return i;}
 std::string error()const override{return "unavailable "+std::to_string(type_);}
};
void setup(bool sdl,bool steam,bool sony){check(live==0,"reader leaked");available={false,sdl,steam,sony};attempts.clear();peak=0;}
}
namespace rg {
std::unique_ptr<MotionBackend> makeSdlBackend(){return std::make_unique<Fake>(1);}
std::unique_ptr<MotionBackend> makeSteamBackend(){return std::make_unique<Fake>(2);}
std::unique_ptr<MotionBackend> makeSonyPassiveBackend(){return std::make_unique<Fake>(3);}
}
int main(){try{
 setup(true,true,false);{
  auto b=rg::makeMotionBackend(4);check(b->connect(0),"direct SDL connects");check(attempts==std::vector<int>({3,1}),"direct never probes Steam even when enabled");check(!b->info().externalCalibration&&b->read()->sensorNs==1,"direct sample owner");check(peak==1,"only one reader at a time");
 }
 setup(true,true,true);{
  auto b=rg::makeMotionBackend(4);check(b->connect(0)&&attempts==std::vector<int>({3}),"Sony passive path retained");
 }
 setup(false,true,false);{
  auto b=rg::makeMotionBackend(4);check(!b->connect(0)&&!b->connected()&&!b->read(),"hidden direct sensor stays unavailable");check(attempts==std::vector<int>({3,1}),"no silent Steam fallback");
 }
 setup(true,false,true);{
  auto b=rg::makeMotionBackend(2);check(!b->connect(0)&&attempts==std::vector<int>({2}),"missing Steam does not silently use direct sensors");
 }
 setup(true,true,true);{
  auto b=rg::makeMotionBackend(0);check(b->connect(0)&&b->info().externalCalibration,"automatic launch prefers Steam");
  available[2]=false;attempts.clear();check(!b->connect(0)&&attempts==std::vector<int>({2}),"loss of Steam never swaps to direct in session");
  available[2]=true;check(b->connect(0)&&b->info().externalCalibration,"same Steam owner can reconnect");
 }
 setup(true,false,true);{
  auto b=rg::makeMotionBackend(0);check(b->connect(0)&&!b->info().externalCalibration,"direct source selected when Steam absent");
  available[2]=true;available[3]=false;attempts.clear();
  check(b->connect(0)&&attempts==std::vector<int>({3,1})&&!b->info().externalCalibration,"direct reconnect may change physical reader but never calibration owner");
  b.reset();b=rg::makeMotionBackend(0);check(b->connect(0)&&b->info().externalCalibration,"new session can select Steam");check(peak==1,"readers never overlap");
 }
 setup(false,false,false);{
  auto b=rg::makeMotionBackend(0);check(!b->connect(0),"no controller at launch");
  available[2]=true;check(b->connect(0)&&b->info().externalCalibration,"no owner locked before first successful connection");
 }
 check(live==0,"all reader resources released");std::cout<<"Explicit source ownership, unavailable paths, Sony preservation and reader transitions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
