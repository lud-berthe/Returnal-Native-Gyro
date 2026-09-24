#include "rg/steam_contact_reader.hpp"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
namespace {
std::mutex gateMutex;
std::condition_variable gate;
bool blocked{};
std::atomic<int> scans{},opens{},closes{},initRefs{},reads{};
std::atomic<unsigned> flags{0x20000000},product{0x1302};
std::atomic<bool> failRead{},emit{true},openBlocked{};
void release(){ {std::lock_guard lock(gateMutex);blocked=false;}gate.notify_all(); }
void block(){std::lock_guard lock(gateMutex);blocked=true;}
void awaitGate(){std::unique_lock lock(gateMutex);gate.wait(lock,[]{return !blocked;});}
void check(bool value,const char* message){if(!value){release();throw std::runtime_error(message);}}
template<class F>void until(F predicate,const char* message){
 auto deadline=std::chrono::steady_clock::now()+2s;
 while(!predicate()){if(std::chrono::steady_clock::now()>deadline){release();throw std::runtime_error(message);}std::this_thread::sleep_for(1ms);}
}
}
struct SDL_hid_device {bool sent{};};
extern "C" {
int SDLCALL SDL_hid_init(){++initRefs;return 0;}
int SDLCALL SDL_hid_exit(){--initRefs;return 0;}
SDL_hid_device_info* SDLCALL SDL_hid_enumerate(unsigned short,unsigned short){
 ++scans;awaitGate();auto d=new SDL_hid_device_info{};
 d->path=const_cast<char*>("fake-controller");d->vendor_id=0x28de;d->product_id=static_cast<unsigned short>(product.load());d->usage_page=0xff00;d->usage=1;d->interface_number=2;return d;
}
void SDLCALL SDL_hid_free_enumeration(SDL_hid_device_info* p){delete p;}
SDL_hid_device* SDLCALL SDL_hid_open_path(const char*){++opens;if(openBlocked)awaitGate();return new SDL_hid_device{};}
int SDLCALL SDL_hid_close(SDL_hid_device* p){++closes;delete p;return 0;}
int SDLCALL SDL_hid_read_timeout(SDL_hid_device* p,unsigned char* bytes,size_t size,int timeout){
 check(timeout==0,"existing HID reads must stay nonblocking");++reads;
 if(failRead)return -1;
 if(!emit||p->sent){p->sent=false;return 0;}
 p->sent=true;std::memset(bytes,0,size);bytes[0]=0x42;
 auto f=flags.load();for(int b=0;b<4;++b)bytes[2+b]=static_cast<unsigned char>(f>>(8*b));return 54;
}
}
int main(){try{
 {
  rg::SteamContactReader reader;
  std::uint64_t now=1'000'000'000;
  // A gated enumeration models an arbitrarily slow OS call. update must return
  // without releasing that gate (the old synchronous reader fails this test).
  block();reader.update(now,1,0x1302);until([]{return scans.load()==1;},"discovery worker started");
  for(int i=0;i<50;++i)reader.update(now+=4'000'000,1,0x1302);
  check(reader.available==0&&scans==1,"one pending discovery, no fabricated contact");
  release();until([&]{reader.update(now+=4'000'000,1,0x1302);return reader.detectedProduct==0x1302;},"adopt discovered controller");
  check((reader.buttons&rg::buttonMask(17))!=0,"first grip received");
  const int opened=opens.load(),beforeReads=reads.load();
  block();now+=2'100'000'000;reader.update(now,1,0x1302);until([]{return scans.load()==2;},"periodic scan started");
  for(int i=0;i<80;++i){reader.update(now+=4'000'000,1,0x1302);check((reader.buttons&rg::buttonMask(17))!=0,"held grip survives slow discovery beyond freshness timeout");}
  flags=0;reader.update(now+=4'000'000,1,0x1302);
  check(reader.buttons==0&&reader.available!=0,"grip release arrives while enumeration is blocked");
  flags=0x02000000;reader.update(now+=4'000'000,1,0x1302);
  check((reader.buttons&rg::buttonMask(12))!=0,"trackpad still updates during slow scan");
  check(reads>beforeReads&&opens==opened,"known handles keep reading without reopening");
  emit=false;reader.update(now+=210'000'000,1,0x1302);
  check(reader.buttons==0&&reader.available==0,"actual stale reports still expire");emit=true;
  release();until([&]{reader.update(now+=4'000'000,1,0x1302);return reader.available!=0;},"fresh contact recovers");
  reader.reset();check(opens==closes,"reset closes all adopted and pending handles");
  flags=0x20000000;now+=2'100'000'000;
  until([&]{reader.update(now+=4'000'000,1,0x1302);return reader.available!=0;},"rediscovery after reset");
  failRead=true;reader.update(now+=4'000'000,1,0x1302);
  check(reader.buttons==0&&reader.available==0,"device failure releases contacts");failRead=false;
  now+=2'100'000'000;until([&]{reader.update(now+=4'000'000,1,0x1302);return reader.available!=0;},"failed handle reopened on next scan");
  reader.update(now,2,0x1302);check(reader.available==0&&reader.buttons==0&&opens==closes,"ambiguous controller association clears reader");
  // A new device generation must never adopt an old completed discovery result.
  reader.update(now,1,0x1302);until([]{return opens>closes;},"pending old-session handle opened");
  reader.update(now,1,0x1102);check(reader.detectedProduct==0&&reader.buttons==0,"product switch discards old session");
  reader.reset();check(opens==closes,"pending results use RAII cleanup");
 }
 check(initRefs==0,"HID library lifetime balanced after joining workers");
 std::cout<<"Async discovery: pending scan, live grip/touch/release, freshness, reconnect, association and cleanup passed\n";
 return 0;
}catch(const std::exception& e){release();std::cerr<<e.what()<<'\n';return 1;}}
