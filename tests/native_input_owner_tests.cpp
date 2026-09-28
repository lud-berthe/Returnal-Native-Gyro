#include "rg/native_input_owner.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace rg {
void ownerTestCallbacks(bool(*)(void*,std::uint64_t,int,bool),bool(*)(void*,std::uint64_t,int,bool),bool(*)(void*,std::uint64_t,int,float));
void ownerTestNative(void(*)(void*));
bool ownerTestPress(void*,std::uint64_t,int,bool);
bool ownerTestRelease(void*,std::uint64_t,int,bool);
bool ownerTestAnalog(void*,std::uint64_t,int,float);
void ownerTestIdentityCallbacks(void(*)(void*,int,const void*),void(*)(void*,bool,int,int),void(*)(void*,bool,int,const void*));
void ownerTestUsage(void*,int,const void*);
void ownerTestConnection(void*,bool,int,int);
void ownerTestNamedConnection(void*,bool,int,const void*);
}
namespace {
struct Event {char type;std::uint64_t key;int slot;float value;};
std::vector<Event> events;int nativePolls{};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool press(void*,std::uint64_t key,int slot,bool){events.push_back({'P',key,slot,1});return true;}
bool release(void*,std::uint64_t key,int slot,bool){events.push_back({'R',key,slot,0});return true;}
bool axis(void*,std::uint64_t key,int slot,float value){events.push_back({'A',key,slot,value});return true;}
void poll(void*){++nativePolls;rg::ownerTestPress(nullptr,12,0,false);rg::ownerTestAnalog(nullptr,30,0,.75f);}
int activeDevice=-1,outputRebuilds{},connectionEvents{},namedConnectionEvents{};
const void* lastName{};void* lastController{};int lastUser{},lastSlot{};bool lastConnected{};
const char nativeName[]="native",steamName[]="Steam";
void usage(void* pc,int slot,const void* name){lastController=pc;lastName=name;lastSlot=slot;if(slot!=activeDevice){activeDevice=slot;++outputRebuilds;}}
void connection(void* pc,bool connected,int user,int slot){++connectionEvents;lastController=pc;lastConnected=connected;lastUser=user;lastSlot=slot;}
void namedConnection(void* pc,bool connected,int slot,const void* name){++namedConnectionEvents;lastController=pc;lastConnected=connected;lastSlot=slot;lastName=name;}
void identityPoll(void* pc){rg::ownerTestUsage(pc,1001,nativeName);rg::ownerTestConnection(pc,true,0,1001);rg::ownerTestNamedConnection(pc,true,1001,nativeName);}
void disconnectPoll(void* pc){rg::ownerTestConnection(pc,false,0,1001);rg::ownerTestNamedConnection(pc,false,1001,nativeName);}
}
int main(){try{
    rg::ownerTestCallbacks(press,release,axis);
    rg::setSteamInputOwner(false);rg::ownerTestNative(poll);
    check(events.size()==2&&events[0].type=='P'&&events[1].value==.75f,"native press and axis pass unchanged");
    events.clear();rg::setSteamInputOwner(true);rg::ownerTestNative(poll);
    check(events.size()==2&&events[0].type=='R'&&events[0].key==12&&events[1].value==0,"Steam takeover releases forwarded native state once");
    check(nativePolls==2,"native polling and device housekeeping continue under Steam");
    events.clear();rg::ownerTestNative(poll);
    check(events.empty(),"existing native HID handle cannot duplicate Steam input");
    check(rg::nativeInputSuppressed()==4,"muted native input observable in diagnostics");
    rg::ownerTestPress(nullptr,12,0,false);rg::ownerTestAnalog(nullptr,30,0,.75f);rg::ownerTestRelease(nullptr,12,0,false);
    check(events.size()==3,"Steam/non-native events always pass exactly once");
    events.clear();rg::setSteamInputOwner(false);rg::ownerTestNative(poll);
    check(events.size()==2&&events[0].type=='P'&&events[1].value==.75f,"disable Steam restores native event delivery");
    events.clear();rg::ownerTestRelease(nullptr,99,1,false);
    check(events.size()==1&&events[0].key==99,"unrelated producer is not filtered");
    rg::NativeInputLedger ledger;ledger.button(nullptr,15,2,true);ledger.button(nullptr,15,2,false);ledger.axis(nullptr,18,2,0);
    int stale=0;ledger.relinquish([&](auto,auto,auto){++stale;},[&](auto,auto,auto){++stale;});
    check(stale==0,"already released buttons and neutral axes are not replayed");
    rg::ownerTestIdentityCallbacks(usage,connection,namedConnection);
    rg::setSteamInputOwner(false);rg::ownerTestNative(identityPoll);
    check(activeDevice==1001&&outputRebuilds==1&&lastName==nativeName&&connectionEvents==1&&namedConnectionEvents==1,"native identity and connection delegates pass when Steam is disabled");
    rg::setSteamInputOwner(true);
    for(int i=0;i<120;++i){rg::ownerTestUsage(nullptr,0,steamName);rg::ownerTestNative(identityPoll);}
    check(activeDevice==0&&outputRebuilds==2&&lastName==steamName,"Steam-native identity alternation cannot repeatedly recreate audio outputs");
    check(connectionEvents==1&&namedConnectionEvents==1,"hidden native connection notifications cannot tear down Steam outputs");
    rg::ownerTestNative(disconnectPoll);
    check(connectionEvents==1&&namedConnectionEvents==1,"hidden native disconnect cannot remove Steam output");
    check(rg::nativeIdentitySuppressed()==362,"suppressed identity notifications counted");
    int controllerToken{};
    rg::ownerTestConnection(&controllerToken,false,3,2);rg::ownerTestNamedConnection(&controllerToken,false,2,steamName);
    check(connectionEvents==2&&namedConnectionEvents==2&&!lastConnected&&lastController==&controllerToken&&lastUser==3&&lastSlot==2&&lastName==steamName,"real Steam disconnect is forwarded with original arguments");
    rg::setSteamInputOwner(false);rg::ownerTestNative(identityPoll);
    check(activeDevice==1001&&outputRebuilds==3&&connectionEvents==3&&namedConnectionEvents==3,"return to native restores identity and audio selection immediately");
    rg::ownerTestUsage(nullptr,2,steamName);
    check(activeDevice==2&&outputRebuilds==4,"native scope ends correctly; subsequent unrelated usage is forwarded");
    std::cout<<"Native input ownership routing passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
