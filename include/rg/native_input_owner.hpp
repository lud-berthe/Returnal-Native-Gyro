#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace rg {
void setSteamInputOwner(bool active);
bool steamInputOwnsGamepad();
std::uint64_t nativeInputSuppressed();
std::uint64_t nativeIdentitySuppressed();
bool createNativeInputOwnerHooks(void* dualSense,void* dualShock,void* pressed,void* released,void* analog,
                                void* usage,void* connection,void* namedConnection,std::vector<void*>& targets);
// Only native events actually forwarded to Slate are recorded. On takeover,
// release those held buttons/axes once before muting further native events.
class NativeInputLedger {
    struct Entry {void* app{};std::uint64_t key{};int slot{};float value{};bool used{};};
    std::array<Entry,256> buttons_{};
    std::array<Entry,64> axes_{};
    template<size_t N>static void record(std::array<Entry,N>& entries,void* app,std::uint64_t key,int slot,float value){
        Entry* free=nullptr;
        for(auto& e:entries){if(e.used&&e.app==app&&e.key==key&&e.slot==slot){e.value=value;return;}if(!e.used&&!free)free=&e;}
        if(free)*free={app,key,slot,value,true};
    }
public:
    void button(void* app,std::uint64_t key,int slot,bool down){record(buttons_,app,key,slot,down?1.0f:0.0f);}
    void axis(void* app,std::uint64_t key,int slot,float value){record(axes_,app,key,slot,value);}
    template<class Release,class Analog>void relinquish(Release release,Analog analog){
        for(auto& e:buttons_){if(e.used&&e.value!=0)release(e.app,e.key,e.slot);e={};}
        for(auto& e:axes_){if(e.used&&e.value!=0)analog(e.app,e.key,e.slot);e={};}
    }
};
}
