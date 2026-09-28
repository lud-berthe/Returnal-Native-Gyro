#include "rg/native_input_owner.hpp"
#include <atomic>
#include <MinHook.h>
namespace rg {
namespace {
std::atomic<bool> steamOwner{};
std::atomic<std::uint64_t> suppressed{};
std::atomic<std::uint64_t> identitySuppressed{};
thread_local bool nativeCall{}, muteNative{};
NativeInputLedger ledger;
using Send=void(*)(void*);
using Button=bool(*)(void*,std::uint64_t,int,bool);
using Analog=bool(*)(void*,std::uint64_t,int,float);
// FString arguments remain opaque references; never change device identities.
using Usage=void(*)(void*,int,const void*);
using Connection=void(*)(void*,bool,int,int);
using NamedConnection=void(*)(void*,bool,int,const void*);
Usage originalUsage{};Connection originalConnection{};NamedConnection originalNamedConnection{};
Send originalSense{},originalShock{};Button originalPressed{},originalReleased{};Analog originalAnalog{};
void nativeEvents(void* device,Send original){
    const bool mute=steamOwner.load(std::memory_order_acquire);
    if(mute)ledger.relinquish([](void* app,std::uint64_t key,int slot){originalReleased(app,key,slot,false);},
                              [](void* app,std::uint64_t key,int slot){originalAnalog(app,key,slot,0.0f);});
    struct Scope {bool native=nativeCall,muted=muteNative;~Scope(){nativeCall=native;muteNative=muted;}} scope;
    nativeCall=true;muteNative=mute;
    // Keep native polling/device lifetime intact. Mute both Slate input and
    // the game's identity notifications: usage delegates bypass Slate and
    // otherwise make Returnal recreate Wwise outputs for both owners.
    original(device);
}
void sense(void* device){nativeEvents(device,originalSense);}
void shock(void* device){nativeEvents(device,originalShock);}
bool pressed(void* app,std::uint64_t key,int slot,bool repeat){
    if(nativeCall&&muteNative){++suppressed;return true;}
    if(nativeCall)ledger.button(app,key,slot,true);
    return originalPressed(app,key,slot,repeat);
}
bool released(void* app,std::uint64_t key,int slot,bool repeat){
    if(nativeCall&&muteNative){++suppressed;return true;}
    if(nativeCall)ledger.button(app,key,slot,false);
    return originalReleased(app,key,slot,repeat);
}
bool analog(void* app,std::uint64_t key,int slot,float value){
    if(nativeCall&&muteNative){++suppressed;return true;}
    if(nativeCall)ledger.axis(app,key,slot,value);
    return originalAnalog(app,key,slot,value);
}
bool muteIdentity(){if(nativeCall&&muteNative){++identitySuppressed;return true;}return false;}
void usage(void* pc,int slot,const void* name){if(!muteIdentity())originalUsage(pc,slot,name);}
void connection(void* pc,bool connected,int user,int slot){if(!muteIdentity())originalConnection(pc,connected,user,slot);}
void namedConnection(void* pc,bool connected,int slot,const void* name){if(!muteIdentity())originalNamedConnection(pc,connected,slot,name);}
}
void setSteamInputOwner(bool active){steamOwner.store(active,std::memory_order_release);}
bool steamInputOwnsGamepad(){return steamOwner.load(std::memory_order_acquire);}
std::uint64_t nativeInputSuppressed(){return suppressed.load();}
std::uint64_t nativeIdentitySuppressed(){return identitySuppressed.load();}
bool createNativeInputOwnerHooks(void* ds,void* ds4,void* press,void* release,void* axis,
                                void* use,void* connect,void* namedConnect,std::vector<void*>& targets){
    auto add=[&](void* target,void* detour,void** original){if(MH_CreateHook(target,detour,original)!=MH_OK)return false;targets.push_back(target);return true;};
    return add(ds,reinterpret_cast<void*>(&sense),reinterpret_cast<void**>(&originalSense))&&
           add(ds4,reinterpret_cast<void*>(&shock),reinterpret_cast<void**>(&originalShock))&&
           add(press,reinterpret_cast<void*>(&pressed),reinterpret_cast<void**>(&originalPressed))&&
           add(release,reinterpret_cast<void*>(&released),reinterpret_cast<void**>(&originalReleased))&&
           add(axis,reinterpret_cast<void*>(&analog),reinterpret_cast<void**>(&originalAnalog))&&
           add(use,reinterpret_cast<void*>(&usage),reinterpret_cast<void**>(&originalUsage))&&
           add(connect,reinterpret_cast<void*>(&connection),reinterpret_cast<void**>(&originalConnection))&&
           add(namedConnect,reinterpret_cast<void*>(&namedConnection),reinterpret_cast<void**>(&originalNamedConnection));
}
#ifdef RG_NATIVE_OWNER_TEST
void ownerTestCallbacks(Button press,Button release,Analog axis){originalPressed=press;originalReleased=release;originalAnalog=axis;}
void ownerTestNative(Send send){nativeEvents(nullptr,send);}
bool ownerTestPress(void* app,std::uint64_t key,int slot,bool repeat){return pressed(app,key,slot,repeat);}
bool ownerTestRelease(void* app,std::uint64_t key,int slot,bool repeat){return released(app,key,slot,repeat);}
bool ownerTestAnalog(void* app,std::uint64_t key,int slot,float value){return analog(app,key,slot,value);}
void ownerTestIdentityCallbacks(Usage use,Connection connect,NamedConnection named){originalUsage=use;originalConnection=connect;originalNamedConnection=named;}
void ownerTestUsage(void* pc,int slot,const void* name){usage(pc,slot,name);}
void ownerTestConnection(void* pc,bool connected,int user,int slot){connection(pc,connected,user,slot);}
void ownerTestNamedConnection(void* pc,bool connected,int slot,const void* name){namedConnection(pc,connected,slot,name);}
#endif
}
