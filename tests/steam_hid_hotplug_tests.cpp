#include "rg/steam_hid_hotplug.hpp"
#include <windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace rg {
std::optional<std::uint32_t> steamVirtualHidMask(){return 0;}
void setSteamInputOwner(bool){}
bool steamHidSupportedForTest(unsigned char*);
bool steamHidSnapshotForTest(unsigned char*,std::uintptr_t&,std::uint32_t&,std::uint32_t&);
bool steamHidRefreshForTest(unsigned char*,std::uintptr_t,std::uint32_t,std::uint32_t);
}
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Pages {
    unsigned char* p=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x1ce000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    Pages(){if(!p)throw std::runtime_error("allocation failed");}
    ~Pages(){VirtualFree(p,0,MEM_RELEASE);}
};
void put32(unsigned char* p,size_t offset,std::uint32_t v){std::memcpy(p+offset,&v,4);}
}
int main(){try{
    constexpr std::uint64_t step=250'000'000;
    check(rg::steamHasVirtualSlot(123,0,123),"verified Steam virtual pad owns input");
    check(!rg::steamHasVirtualSlot(123,-1,0),"disabled Steam still exposes a motion handle but owns no input");
    check(!rg::steamHasVirtualSlot(123,0,456),"mismatched reverse association cannot hide native input");
    check(!rg::steamHasVirtualSlot(123,4,123),"invalid slot cannot hide native input");
    check(!rg::steamHasVirtualSlot(0,0,0),"empty slot is not a controller");
    rg::SteamHidFilterPolicy overridePolicy;
    overridePolicy.observe(0xffff,0x1000,0,1);
    auto overrideDecision=overridePolicy.observe(0xffff,0x1000,step,1);
    check(!overrideDecision.refresh&&!overrideDecision.rediscover&&overrideDecision.value==0xffff,"Steam startup must preserve complete filter and healthy reader");
    check(!overridePolicy.observe(0xffff,0x1000,2*step,1).rediscover,"stable Steam owner keeps one gyro reader");
    overridePolicy.observe(0xffff,0x1000,3*step,0);
    overrideDecision=overridePolicy.observe(0xffff,0x1000,4*step,0);
    check(overrideDecision.refresh&&overrideDecision.value==0x1000,"slot loss hands physical Sony input back to native");
    overridePolicy.committed(0x1000);
    overridePolicy.observe(0x1000,0x1000,5*step,1);
    overrideDecision=overridePolicy.observe(0x1000,0x1000,6*step,1);
    check(overrideDecision.refresh&&overrideDecision.rediscover&&overrideDecision.value==0xffff,"slot return restores full original Steam filtering even with unchanged global config");
    rg::SteamHidFilterPolicy mixed;
    mixed.observe(0,0x1000,0,1|8);
    check(mixed.observe(0,0x1000,step,1|8).value==0xffff,"Steam virtual pad uses the complete Steam enabled filter");
    rg::SteamHidFilterPolicy p;
    check(!p.observe(0xffff,0xffff,0).rediscover,"startup observation waits");
    check(!p.observe(0xffff,0xffff,step).rediscover,"matching initial filter does not reset input");
    check(!p.observe(0xffff,0x1000,2*step).refresh,"config transition waits for stable value");
    auto d=p.observe(0xffff,0x1000,3*step);
    check(d.refresh&&d.rediscover&&d.value==0x1000,"Steam to native refresh copies only authoritative config");
    check(p.observe(0xffff,0x1000,4*step).refresh,"failed CAS remains retryable");
    p.committed(0x1000);
    for(int i=5;i<25;++i)check(!p.observe(0x1000,0x1000,i*step).rediscover,"healthy reader is never reset periodically");
    check(!p.observe(0x1000,0x1002,25*step).refresh,"native to Steam transition debounce");
    d=p.observe(0x1000,0x1002,26*step);check(d.refresh&&d.rediscover&&d.value==0x1002,"restore Sony mask when Steam takes ownership");p.committed(d.value);
    p.observe(0x1002,0x1000,27*step);
    check(!p.observe(0x1002,0x1002,28*step).rediscover,"cancel transient config value");
    check(!p.observe(0x1002,0x1002,29*step).rediscover,"cancelled transition keeps reader");
    p.observe(0x1000,0x1000,30*step);
    check(p.observe(0x1000,0x1000,31*step).rediscover,"external Steam cache update still notifies ownership change");
    check(!p.observe(0x1000,0x1000,32*step).rediscover,"external update notifies once");
    p.observe(0,0x10000,33*step);check(!p.observe(0,0x10000,34*step).refresh,"unknown mask bits are untouched");
    Pages image,config;
    auto base=image.p;auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+0x100);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt->FileHeader.TimeDateStamp=0x6ab95eb1;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt->OptionalHeader.SizeOfImage=0x1ce000;
    const unsigned char object[]={0x48,0x8d,0x0d,0x8e,0x00,0x0c,0x00};
    const unsigned char mask[]={0x83,0xb9,0x9c,0x05,0x01,0x00,0x00};
    const unsigned char copy[]={0x48,0x8b,0x87,0xe8,0x02,0x00,0x00,0x48,0x85,0xc0,0x74,0x05,0x8b,0x40,0x08,0xeb,0x02,0x8b,0xc6,0x89,0x87,0x9c,0x05,0x01,0x00};
    std::memcpy(base+0xc3c2b,object,sizeof(object));std::memcpy(base+0xabe3f,mask,sizeof(mask));std::memcpy(base+0xaac73,copy,sizeof(copy));
    check(rg::steamHidSupportedForTest(base),"recognized overlay layout");
    ++nt->FileHeader.TimeDateStamp;check(!rg::steamHidSupportedForTest(base),"other overlay build rejected");--nt->FileHeader.TimeDateStamp;
    base[0xc3c2b]^=1;check(!rg::steamHidSupportedForTest(base),"changed object instruction rejected");base[0xc3c2b]^=1;
    base[0xabe3f]^=1;check(!rg::steamHidSupportedForTest(base),"changed filter instruction rejected");base[0xabe3f]^=1;
    base[0xaac73]^=1;check(!rg::steamHidSupportedForTest(base),"changed shared-config instruction rejected");base[0xaac73]^=1;
    auto configAddress=reinterpret_cast<std::uintptr_t>(config.p);std::memcpy(base+0x183cc0+0x2e8,&configAddress,sizeof(configAddress));
    put32(base,0x183cc0+0x1059c,0xffff);put32(config.p,8,0x1000);
    std::uintptr_t shared{};std::uint32_t cached{},current{};
    check(rg::steamHidSnapshotForTest(base,shared,cached,current)&&cached==0xffff&&current==0x1000,"read live config and stale cache");
    check(!rg::steamHidRefreshForTest(base,shared,0xff,0x1000),"concurrent cache change is not overwritten");
    put32(config.p,8,0x1002);check(!rg::steamHidRefreshForTest(base,shared,0xffff,0x1000),"concurrent config change is not overwritten");
    put32(config.p,8,0x1000);check(rg::steamHidRefreshForTest(base,shared,0xffff,0x1000),"guarded cache update succeeds");
    check(rg::steamHidSnapshotForTest(base,shared,cached,current)&&cached==current,"cache follows config after write");
    DWORD protection{};VirtualProtect(config.p,4096,PAGE_NOACCESS,&protection);
    check(!rg::steamHidSnapshotForTest(base,shared,cached,current),"unmapped/inaccessible config is ignored");
    VirtualProtect(config.p,4096,protection,&protection);
    VirtualProtect(base+0x183cc0+0x1059c,4,PAGE_READONLY,&protection);
    check(!rg::steamHidSnapshotForTest(base,shared,cached,current),"nonwritable cache is ignored");
    std::cout<<"Steam HID hotplug guards and ownership policy passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
