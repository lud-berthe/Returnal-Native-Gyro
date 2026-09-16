#include "rg/backend.hpp"
namespace rg {
namespace {
class AutoBackend final:public MotionBackend {
 std::unique_ptr<MotionBackend> active_;
 std::string error_;
public:
 bool connect(int index)override{
  active_.reset();error_.clear();
  // One owner only: Steam-managed controller first, then direct Sony, then SDL sensors.
  for(int mode:{2,3,1}){
   auto p=makeMotionBackend(mode);
   if(p->connect(index)){active_=std::move(p);error_.clear();return true;}
   error_+= (error_.empty()?"":"; ")+p->error();
  }
  return false;
 }
 std::optional<GyroSample> read()override{return active_?active_->read():std::nullopt;}
 bool connected()const override{return active_&&active_->connected();}
 DeviceInfo info()const override{return active_?active_->info():DeviceInfo{};}
 std::string error()const override{return active_?active_->error():error_;}
};
}
std::unique_ptr<MotionBackend> makeMotionBackend(int mode){
 switch(mode){case 1:return makeSdlBackend();case 2:return makeSteamBackend();case 3:return makeSonyPassiveBackend();default:return std::make_unique<AutoBackend>();}
}
}
